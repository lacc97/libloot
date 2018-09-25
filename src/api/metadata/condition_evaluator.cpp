/*  LOOT

    A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
    Fallout: New Vegas.

    Copyright (C) 2012-2016    WrinklyNinja

    This file is part of LOOT.

    LOOT is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation, either version 3 of
    the License, or (at your option) any later version.

    LOOT is distributed in the hope that it will
    be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with LOOT.  If not, see
    <http://www.gnu.org/licenses/>.
    */

#include "api/metadata/condition_evaluator.h"

#include "api/helpers/crc.h"
#include "api/helpers/logging.h"
#include "api/metadata/condition_grammar.h"
#include "loot/exception/condition_syntax_error.h"

using std::filesystem::u8path;

namespace loot {
ConditionEvaluator::ConditionEvaluator() :
    gameType_(GameType::tes4),
    gameCache_(nullptr),
    loadOrderHandler_(nullptr) {}
ConditionEvaluator::ConditionEvaluator(
    const GameType gameType,
    const std::filesystem::path& dataPath,
    std::shared_ptr<GameCache> gameCache,
    std::shared_ptr<LoadOrderHandler> loadOrderHandler) :
    gameType_(gameType),
    dataPath_(dataPath),
    gameCache_(gameCache),
    loadOrderHandler_(loadOrderHandler) {}

bool ConditionEvaluator::evaluate(const std::string& condition) const {
  if (shouldParseOnly()) {
    // Still check that the syntax is valid.
    parseCondition(condition);
    return false;
  }

  if (condition.empty())
    return true;

  auto logger = getLogger();
  if (logger) {
    logger->trace("Evaluating condition: {}", condition);
  }

  auto cachedValue = gameCache_->GetCachedCondition(condition);
  if (cachedValue.second)
    return cachedValue.first;

  bool result = parseCondition(condition);

  gameCache_->CacheCondition(condition, result);

  return result;
}

bool ConditionEvaluator::evaluate(const PluginCleaningData& cleaningData,
                                  const std::string& pluginName) const {
  if (shouldParseOnly() || pluginName.empty())
    return false;

  return cleaningData.GetCRC() == getCrc(pluginName);
}

PluginMetadata ConditionEvaluator::evaluateAll(
    const PluginMetadata& pluginMetadata) const {
  if (shouldParseOnly())
    return pluginMetadata;

  PluginMetadata evaluatedMetadata(pluginMetadata.GetName());
  evaluatedMetadata.SetEnabled(pluginMetadata.IsEnabled());
  evaluatedMetadata.SetLocations(pluginMetadata.GetLocations());

  if (pluginMetadata.GetGroup()) {
    evaluatedMetadata.SetGroup(pluginMetadata.GetGroup().value());
  }

  std::set<File> fileSet;
  for (const auto& file : pluginMetadata.GetLoadAfterFiles()) {
    if (evaluate(file.GetCondition()))
      fileSet.insert(file);
  }
  evaluatedMetadata.SetLoadAfterFiles(fileSet);

  fileSet.clear();
  for (const auto& file : pluginMetadata.GetRequirements()) {
    if (evaluate(file.GetCondition()))
      fileSet.insert(file);
  }
  evaluatedMetadata.SetRequirements(fileSet);

  fileSet.clear();
  for (const auto& file : pluginMetadata.GetIncompatibilities()) {
    if (evaluate(file.GetCondition()))
      fileSet.insert(file);
  }
  evaluatedMetadata.SetIncompatibilities(fileSet);

  std::vector<Message> messages;
  for (const auto& message : pluginMetadata.GetMessages()) {
    if (evaluate(message.GetCondition()))
      messages.push_back(message);
  }
  evaluatedMetadata.SetMessages(messages);

  std::set<Tag> tagSet;
  for (const auto& tag : pluginMetadata.GetTags()) {
    if (evaluate(tag.GetCondition()))
      tagSet.insert(tag);
  }
  evaluatedMetadata.SetTags(tagSet);

  if (!evaluatedMetadata.IsRegexPlugin()) {
    std::set<PluginCleaningData> infoSet;
    for (const auto& info : pluginMetadata.GetDirtyInfo()) {
      if (evaluate(info, pluginMetadata.GetName()))
        infoSet.insert(info);
    }
    evaluatedMetadata.SetDirtyInfo(infoSet);

    infoSet.clear();
    for (const auto& info : pluginMetadata.GetCleanInfo()) {
      if (evaluate(info, pluginMetadata.GetName()))
        infoSet.insert(info);
    }
    evaluatedMetadata.SetCleanInfo(infoSet);
  }

  return evaluatedMetadata;
}

bool ConditionEvaluator::fileExists(const std::string& filePath) const {
  validatePath(filePath);

  if (shouldParseOnly())
    return false;

  if (filePath == "LOOT")
    return true;

  // Try first checking the plugin cache, as most file entries are
  // for plugins.
  auto plugin = gameCache_->GetPlugin(filePath);
  if (plugin) {
    return true;
  }

  // Not a loaded plugin, check the filesystem.
  if (hasPluginFileExtension(filePath, gameType_))
    return std::filesystem::exists(dataPath_ / u8path(filePath)) ||
            std::filesystem::exists(dataPath_ / u8path(filePath + ".ghost"));
  else
    return std::filesystem::exists(dataPath_ / u8path(filePath));
}

bool ConditionEvaluator::regexMatchExists(
    const std::string& regexString) const {
  auto pathRegex = splitRegex(regexString);

  if (shouldParseOnly())
    return false;

  return isRegexMatchInDataDirectory(pathRegex,
                                     [](const std::string&) { return true; });
}

bool ConditionEvaluator::regexMatchesExist(
    const std::string& regexString) const {
  auto pathRegex = splitRegex(regexString);

  if (shouldParseOnly())
    return false;

  return areRegexMatchesInDataDirectory(
      pathRegex, [](const std::string&) { return true; });
}

bool ConditionEvaluator::isPluginActive(const std::string& pluginName) const {
  validatePath(pluginName);

  if (shouldParseOnly())
    return false;

  if (pluginName == "LOOT")
    return false;

  return loadOrderHandler_->IsPluginActive(pluginName);
}

bool ConditionEvaluator::isPluginMatchingRegexActive(
    const std::string& regexString) const {
  auto pathRegex = splitRegex(regexString);

  if (shouldParseOnly())
    return false;

  return isRegexMatchInDataDirectory(
      pathRegex, [&](const std::string& filename) {
        return loadOrderHandler_->IsPluginActive(filename);
      });
}

bool ConditionEvaluator::arePluginsActive(
    const std::string& regexString) const {
  auto pathRegex = splitRegex(regexString);

  if (shouldParseOnly())
    return false;

  return areRegexMatchesInDataDirectory(
      pathRegex, [&](const std::string& filename) {
        return loadOrderHandler_->IsPluginActive(filename);
      });
}

bool ConditionEvaluator::checksumMatches(const std::string& filePath,
                                         const uint32_t checksum) const {
  validatePath(filePath);

  if (shouldParseOnly())
    return false;

  return checksum == getCrc(filePath);
}

bool ConditionEvaluator::compareVersions(const std::string& filePath,
                                         const std::string& testVersion,
                                         const std::string& comparator) const {
  if (!fileExists(filePath))
    return comparator == "!=" || comparator == "<" || comparator == "<=";

  Version givenVersion = Version(testVersion);
  Version trueVersion = getVersion(filePath);

  auto logger = getLogger();
  if (logger) {
    logger->trace("Version extracted: {}", trueVersion.AsString());
  }

  return ((comparator == "==" && trueVersion == givenVersion) ||
          (comparator == "!=" && trueVersion != givenVersion) ||
          (comparator == "<" && trueVersion < givenVersion) ||
          (comparator == ">" && trueVersion > givenVersion) ||
          (comparator == "<=" && trueVersion <= givenVersion) ||
          (comparator == ">=" && trueVersion >= givenVersion));
}

void ConditionEvaluator::validatePath(const std::filesystem::path& path) {
  auto logger = getLogger();
  if (logger) {
    logger->trace("Checking to see if the path \"{}\" is safe.", path.u8string());
  }

  std::filesystem::path temp;
  for (const auto& component : path) {
    if (component == ".")
      continue;

    if (component == ".." && temp.filename() == "..") {
      throw ConditionSyntaxError("Invalid file path: " + path.u8string());
    }

    temp /= component;
  }
}
void ConditionEvaluator::validateRegex(const std::string& regexString) {
  try {
    std::regex(regexString, std::regex::ECMAScript | std::regex::icase);
  } catch (std::regex_error& e) {
    throw ConditionSyntaxError("Invalid regex string \"" + regexString +
                               "\": " + e.what());
  }
}

std::filesystem::path ConditionEvaluator::getRegexParentPath(
    const std::string& regexString) {
  size_t pos = regexString.rfind('/');

  if (pos == std::string::npos)
    return std::filesystem::path();

  return u8path(regexString.substr(0, pos));
}

std::string ConditionEvaluator::getRegexFilename(
    const std::string& regexString) {
  size_t pos = regexString.rfind('/');

  if (pos == std::string::npos)
    return regexString;

  return regexString.substr(pos + 1);
}

std::pair<std::filesystem::path, std::regex> ConditionEvaluator::splitRegex(
    const std::string& regexString) {
  // Can't support a regex string where all path components may be regex, since
  // this could lead to massive scanning if an unfortunately-named directory is
  // encountered. As such, only the filename portion can be a regex. Need to
  // separate that from the rest of the string.

  validateRegex(regexString);

  std::string filename = getRegexFilename(regexString);
  std::filesystem::path parent = getRegexParentPath(regexString);

  validatePath(parent);

  std::regex reg;
  try {
    reg = std::regex(filename, std::regex::ECMAScript | std::regex::icase);
  } catch (std::regex_error& e) {
    throw ConditionSyntaxError("Invalid regex string \"" + filename +
                               "\": " + e.what());
  }

  return std::pair<std::filesystem::path, std::regex>(parent, reg);
}

bool ConditionEvaluator::isGameSubdirectory(
    const std::filesystem::path& path) const {
  std::filesystem::path parentPath = dataPath_ / path;

  return std::filesystem::exists(parentPath) &&
         std::filesystem::is_directory(parentPath);
}

bool ConditionEvaluator::isRegexMatchInDataDirectory(
    const std::pair<std::filesystem::path, std::regex>& pathRegex,
    const std::function<bool(const std::string&)> condition) const {
  // Now we have a valid parent path and a regex filename. Check that the
  // parent path exists and is a directory.
  if (!isGameSubdirectory(pathRegex.first)) {
    auto logger = getLogger();
    if (logger) {
      logger->trace("The path \"{}\" is not a game subdirectory.",
                    pathRegex.first.u8string());
    }
    return false;
  }

  return std::any_of(
      std::filesystem::directory_iterator(dataPath_ / pathRegex.first),
      std::filesystem::directory_iterator(),
      [&](const std::filesystem::directory_entry& entry) {
        const std::string filename = entry.path().filename().u8string();
        return std::regex_match(filename, pathRegex.second) &&
               condition(filename);
      });
}

bool ConditionEvaluator::areRegexMatchesInDataDirectory(
    const std::pair<std::filesystem::path, std::regex>& pathRegex,
    const std::function<bool(const std::string&)> condition) const {
  bool foundOneFile = false;

  return isRegexMatchInDataDirectory(pathRegex,
                                     [&](const std::string& filename) {
                                       if (condition(filename)) {
                                         if (foundOneFile)
                                           return true;

                                         foundOneFile = true;
                                       }

                                       return false;
                                     });
}
bool ConditionEvaluator::parseCondition(const std::string& condition) const {
  if (condition.empty())
    return true;

  ConditionGrammar<std::string::const_iterator, boost::spirit::qi::space_type>
      grammar(*this);
  boost::spirit::qi::space_type skipper;
  std::string::const_iterator begin = condition.begin();
  std::string::const_iterator end = condition.end();
  bool evaluation;

  bool parseResult =
      boost::spirit::qi::phrase_parse(begin, end, grammar, skipper, evaluation);

  if (!parseResult || begin != end) {
    throw ConditionSyntaxError("Failed to parse condition \"" + condition +
                               "\": only partially matched expected syntax.");
  }

  return evaluation;
}
Version ConditionEvaluator::getVersion(const std::string& filePath) const {
  if (filePath == "LOOT")
    return Version(std::filesystem::absolute("LOOT.exe"));
  else {
    // If the file is a plugin, its version needs to be extracted
    // from its description field. Try getting an entry from the
    // plugin cache.

    auto plugin = gameCache_->GetPlugin(filePath);
    if (plugin) {
      return Version(plugin->GetVersion().value_or(""));
    }

    // The file wasn't in the plugin cache, load it as a plugin
    // if it appears to be valid, otherwise treat it as a non
    // plugin file.
    auto pluginPath = dataPath_ / u8path(filePath);
    if (Plugin::IsValid(gameType_, pluginPath))
      return Version(
          Plugin(gameType_, gameCache_, loadOrderHandler_, pluginPath, true)
              .GetVersion()
              .value_or(""));

    return Version(pluginPath);
  }
}
bool ConditionEvaluator::shouldParseOnly() const {
  return gameCache_ == nullptr || loadOrderHandler_ == nullptr;
}

uint32_t ConditionEvaluator::getCrc(const std::string & file) const {
  uint32_t crc = gameCache_->GetCachedCrc(file);

  if (crc != 0) {
    return crc;
  }

  if (file == "LOOT") {
    crc = GetCrc32(std::filesystem::absolute("LOOT.exe"));
    gameCache_->CacheCrc(file, crc);
    return crc;
  }

  // Get the CRC from the game plugin cache if possible.
  auto plugin = gameCache_->GetPlugin(file);
  if (plugin) {
    crc = plugin->GetCRC().value_or(0);
  }

  // Otherwise calculate it from the file.
  if (crc == 0) {
    if (std::filesystem::exists(dataPath_ / u8path(file))) {
      crc = GetCrc32(dataPath_ / u8path(file));
    }
    else if (hasPluginFileExtension(file, gameType_) &&
      std::filesystem::exists(dataPath_ / u8path(file + ".ghost"))) {
      crc = GetCrc32(dataPath_ / u8path(file + ".ghost"));
    }
  }

  if (crc != 0) {
    gameCache_->CacheCrc(file, crc);
  }

  return crc;
}
}
