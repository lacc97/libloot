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
    <https://www.gnu.org/licenses/>.
    */

#include "plugin_sorter.h"

#include <cstdlib>
#include <queue>

#include <boost/algorithm/string.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/iteration_macros.hpp>
#include <boost/graph/topological_sort.hpp>
#include <boost/locale.hpp>

#include "api/game/game.h"
#include "api/helpers/logging.h"
#include "api/helpers/text.h"
#include "api/metadata/condition_evaluator.h"
#include "api/sorting/group_sort.h"
#include "loot/exception/cyclic_interaction_error.h"
#include "loot/exception/undefined_group_error.h"

using std::list;
using std::string;
using std::vector;

namespace loot {
typedef boost::graph_traits<PluginGraph>::vertex_iterator vertex_it;
typedef boost::graph_traits<PluginGraph>::edge_descriptor edge_t;
typedef boost::graph_traits<PluginGraph>::edge_iterator edge_it;

std::string describeEdgeType(EdgeType edgeType) {
  switch (edgeType) {
    case EdgeType::hardcoded:
      return "Hardcoded";
    case EdgeType::masterFlag:
      return "Master Flag";
    case EdgeType::master:
      return "Master";
    case EdgeType::masterlistRequirement:
      return "Masterlist Requirement";
    case EdgeType::userRequirement:
      return "User Requirement";
    case EdgeType::masterlistLoadAfter:
      return "Masterlist Load After";
    case EdgeType::userLoadAfter:
      return "User Load After";
    case EdgeType::group:
      return "Group";
    case EdgeType::overlap:
      return "Overlap";
    case EdgeType::tieBreak:
      return "Tie Break";
    default:
      return "Unknown";
  }
}

std::vector<std::string> PluginSorter::Sort(Game& game) {
  logger_ = getLogger();

  // Clear existing data.
  graph_.clear();
  indexMap_.clear();
  pathsCache_.clear();

  AddPluginVertices(game);

  // If there aren't any vertices, exit early, because sorting assumes
  // there is at least one plugin.
  if (boost::num_vertices(graph_) == 0)
    return vector<std::string>();

  if (logger_) {
    logger_->info("Current load order: ");
    for (const auto& plugin : game.GetLoadOrder()) {
      logger_->info("\t\t{}", plugin);
    }
  }

  // Now add the interactions between plugins to the graph as edges.
  AddSpecificEdges();
  AddHardcodedPluginEdges(game);
  AddGroupEdges();
  AddOverlapEdges();
  AddTieBreakEdges();

  CheckForCycles();

  // Now we can sort.
  list<vertex_t> sortedVertices;
  if (logger_) {
    logger_->trace("Performing topological sort on plugin graph...");
  }
  boost::topological_sort(graph_,
                          std::front_inserter(sortedVertices),
                          boost::vertex_index_map(vertexIndexMap_));

  // Check that the sorted path is Hamiltonian (ie. unique).
  if (logger_) {
    logger_->trace("Checking uniqueness of calculated load order...");
  }
  for (auto it = sortedVertices.begin(); it != sortedVertices.end(); ++it) {
    if (next(it) != sortedVertices.end() &&
        !boost::edge(*it, *next(it), graph_).second && logger_) {
      logger_->error(
          "The calculated load order is not unique. No edge exists between {} "
          "and {}.",
          graph_[*it].GetName(),
          graph_[*next(it)].GetName());
    }
  }

  // Output a plugin list using the sorted vertices.
  if (logger_) {
    logger_->info("Calculated order: ");
  }
  vector<std::string> plugins;
  for (const auto& vertex : sortedVertices) {
    plugins.push_back(graph_[vertex].GetName());
    if (logger_) {
      logger_->info("\t{}", plugins.back());
    }
  }

  return plugins;
}

void PluginSorter::AddPluginVertices(Game& game) {
  // The resolution of tie-breaks in the plugin graph may be dependent
  // on the order in which vertices are iterated over, as an earlier tie
  // break resolution may cause a potential later tie break to instead
  // cause a cycle. Vertices are stored in a std::list and added to the
  // list using push_back().
  // Plugins are stored in an unordered map, so simply iterating over
  // its elements is not guarunteed to produce a consistent vertex order.
  // MSVC 2013 and GCC 5.0 have been shown to produce consistent
  // iteration orders that differ, and while MSVC 2013's order seems to
  // be independent on the order in which the unordered map was filled
  // (being lexicographical), GCC 5.0's unordered map iteration order is
  // dependent on its insertion order.
  // Given that, the order of vertex creation should be made consistent
  // in order to produce consistent sorting results. While MSVC 2013
  // doesn't strictly need this, there is no guaruntee that this
  // unspecified behaviour will remain in future compiler updates, so
  // implement it generally.

  // Using a set of plugin names followed by finding the matching key
  // in the unordered map, as it's probably faster than copying the
  // full plugin objects then sorting them.
  std::map<std::string, std::vector<std::string>> groupPlugins;

  auto loadOrder = game.GetLoadOrder();

  for (const auto& plugin : game.GetCache()->GetPlugins()) {
    auto masterlistMetadata =
        game.GetDatabase()
            ->GetPluginMetadata(plugin->GetName(), false, true)
            .value_or(PluginMetadata(plugin->GetName()));
    auto userMetadata = game.GetDatabase()
                            ->GetPluginUserMetadata(plugin->GetName(), true)
                            .value_or(PluginMetadata(plugin->GetName()));

    auto pluginSortingData =
        PluginSortingData(*plugin, masterlistMetadata, userMetadata, loadOrder);

    auto groupName = pluginSortingData.GetGroup();
    auto groupIt = groupPlugins.find(groupName);
    if (groupIt == groupPlugins.end()) {
      groupPlugins.emplace(groupName,
                           std::vector<std::string>({plugin->GetName()}));
    } else {
      groupIt->second.push_back(plugin->GetName());
    }

    boost::add_vertex(pluginSortingData, graph_);
  }

  // Map sets of transitive group dependencies to sets of transitive plugin
  // dependencies.
  groups_ = game.GetDatabase()->GetGroups();

  auto groups = GetTransitiveAfterGroups(game.GetDatabase()->GetGroups(false),
                                         game.GetDatabase()->GetUserGroups());
  for (auto& group : groups) {
    std::unordered_set<std::string> transitivePlugins;
    for (const auto& afterGroup : group.second) {
      auto pluginsIt = groupPlugins.find(afterGroup);
      if (pluginsIt != groupPlugins.end()) {
        transitivePlugins.insert(pluginsIt->second.begin(),
                                 pluginsIt->second.end());
      }
    }
    group.second = transitivePlugins;
  }

  // Add all transitive plugin dependencies for a group to the plugin's load
  // after metadata.
  for (const auto& vertex :
       boost::make_iterator_range(boost::vertices(graph_))) {
    PluginSortingData& plugin = graph_[vertex];

    if (logger_) {
      logger_->trace(
          "Plugin \"{}\" belongs to group \"{}\", setting after group plugins",
          plugin.GetName(),
          plugin.GetGroup());
    }

    auto groupsIt = groups.find(plugin.GetGroup());
    if (groupsIt == groups.end()) {
      throw UndefinedGroupError(plugin.GetGroup());
    } else {
      plugin.SetAfterGroupPlugins(groupsIt->second);
    }
  }

  // Prebuild an index map, which std::list-based VertexList graphs don't have.
  vertexIndexMap_ = vertex_map_t(indexMap_);
  size_t i = 0;
  BGL_FORALL_VERTICES(v, graph_, PluginGraph)
  put(vertexIndexMap_, v, i++);
}

std::optional<vertex_t> PluginSorter::GetVertexByName(
    const std::string& name) const {
  for (const auto& vertex :
       boost::make_iterator_range(boost::vertices(graph_))) {
    if (CompareFilenames(graph_[vertex].GetName(), name) == 0) {
      return vertex;
    }
  }

  return std::nullopt;
}

void PluginSorter::CheckForCycles() const {
  if (logger_) {
    logger_->trace("Checking plugin graph for cycles...");
  }
  boost::depth_first_search(
      graph_,
      visitor(CycleDetector<PluginGraph>()).vertex_index_map(vertexIndexMap_));
}

bool PluginSorter::EdgeCreatesCycle(const vertex_t& fromVertex,
                                    const vertex_t& toVertex) {
  if (pathsCache_.count(GraphPath(toVertex, fromVertex)) != 0) {
    return true;
  }

  auto start = toVertex;
  auto end = fromVertex;

  std::queue<vertex_t> forwardQueue;
  std::queue<vertex_t> reverseQueue;
  std::unordered_set<vertex_t> forwardVisited;
  std::unordered_set<vertex_t> reverseVisited;

  forwardQueue.push(start);
  forwardVisited.insert(start);
  reverseQueue.push(end);
  reverseVisited.insert(end);

  while (!forwardQueue.empty() && !reverseQueue.empty()) {
    if (!forwardQueue.empty()) {
      auto v = forwardQueue.front();
      forwardQueue.pop();
      if (v == end || reverseVisited.count(v) > 0) {
        return true;
      }
      for (auto adjacentV :
           boost::make_iterator_range(boost::adjacent_vertices(v, graph_))) {
        if (forwardVisited.count(adjacentV) == 0) {
          pathsCache_.insert(GraphPath(start, adjacentV));

          forwardVisited.insert(adjacentV);
          forwardQueue.push(adjacentV);
        }
      }
    }
    if (!reverseQueue.empty()) {
      auto v = reverseQueue.front();
      reverseQueue.pop();
      if (v == start || forwardVisited.count(v) > 0) {
        return true;
      }
      for (auto adjacentV : boost::make_iterator_range(
               boost::inv_adjacent_vertices(v, graph_))) {
        if (reverseVisited.count(adjacentV) == 0) {
          pathsCache_.insert(GraphPath(adjacentV, end));

          reverseVisited.insert(adjacentV);
          reverseQueue.push(adjacentV);
        }
      }
    }
  }

  return false;
}

void PluginSorter::AddEdge(const vertex_t& fromVertex,
                           const vertex_t& toVertex,
                           EdgeType edgeType) {
  auto graphPath = GraphPath(fromVertex, toVertex);

  if (pathsCache_.count(graphPath) != 0) {
    return;
  }

  if (logger_) {
    logger_->trace("Adding {} edge from \"{}\" to \"{}\".",
                   describeEdgeType(edgeType),
                   graph_[fromVertex].GetName(),
                   graph_[toVertex].GetName());
  }

  boost::add_edge(fromVertex, toVertex, edgeType, graph_);
  pathsCache_.insert(graphPath);
}

void PluginSorter::AddHardcodedPluginEdges(Game& game) {
  using std::filesystem::u8path;

  auto implicitlyActivePlugins =
      game.GetLoadOrderHandler()->GetImplicitlyActivePlugins();

  std::set<std::filesystem::path> processedPluginPaths;
  for (const auto& plugin : implicitlyActivePlugins) {
    auto pluginPath = game.DataPath() / u8path(plugin);

    try {
      processedPluginPaths.insert(std::filesystem::canonical(pluginPath));
    } catch (std::filesystem::filesystem_error& e) {
      if (logger_) {
        logger_->trace(
            "Skipping adding hardcoded plugin edges for \"{}\" as its "
            "canonical path could not be determined: {}",
            plugin,
            e.what());
      }
      continue;
    }

    if (game.Type() == GameType::tes5 &&
        loot::equivalent(plugin, "update.esm")) {
      if (logger_) {
        logger_->trace(
            "Skipping adding hardcoded plugin edges for Update.esm as it does "
            "not have a hardcoded position for Skyrim.");
        continue;
      }
    }

    auto pluginVertex = GetVertexByName(plugin);

    if (!pluginVertex.has_value()) {
      if (logger_) {
        logger_->trace(
            "Skipping adding hardcoded plugin edges for \"{}\" as it has not "
            "been loaded.",
            plugin);
      }
      continue;
    }

    vertex_it vit, vitend;
    for (tie(vit, vitend) = boost::vertices(graph_); vit != vitend; ++vit) {
      auto& graphPlugin = graph_[*vit];

      auto graphPluginPath = game.DataPath() / u8path(graphPlugin.GetName());
      if (!std::filesystem::exists(graphPluginPath)) {
        graphPluginPath += ".ghost";
      }

      if (!std::filesystem::exists(graphPluginPath)) {
        continue;
      }

      if (processedPluginPaths.count(
              std::filesystem::canonical(graphPluginPath)) == 0) {
        AddEdge(pluginVertex.value(), *vit, EdgeType::hardcoded);
      }
    }
  }
}

void PluginSorter::AddSpecificEdges() {
  // Add edges for all relationships that aren't overlaps.
  vertex_it vit, vitend;
  for (tie(vit, vitend) = boost::vertices(graph_); vit != vitend; ++vit) {
    for (vertex_it vit2 = vit; vit2 != vitend; ++vit2) {
      if (graph_[*vit].IsMaster() == graph_[*vit2].IsMaster())
        continue;

      vertex_t vertex, parentVertex;
      if (graph_[*vit2].IsMaster()) {
        parentVertex = *vit2;
        vertex = *vit;
      } else {
        parentVertex = *vit;
        vertex = *vit2;
      }

      AddEdge(parentVertex, vertex, EdgeType::masterFlag);
    }

    for (const auto& master : graph_[*vit].GetMasters()) {
      auto parentVertex = GetVertexByName(master);
      if (parentVertex.has_value()) {
        AddEdge(parentVertex.value(), *vit, EdgeType::master);
      }
    }

    for (const auto& file : graph_[*vit].GetMasterlistRequirements()) {
      auto parentVertex = GetVertexByName(file.GetName());
      if (parentVertex.has_value()) {
        AddEdge(parentVertex.value(), *vit, EdgeType::masterlistRequirement);
      }
    }
    for (const auto& file : graph_[*vit].GetUserRequirements()) {
      auto parentVertex = GetVertexByName(file.GetName());
      if (parentVertex.has_value()) {
        AddEdge(parentVertex.value(), *vit, EdgeType::userRequirement);
      }
    }

    for (const auto& file : graph_[*vit].GetMasterlistLoadAfterFiles()) {
      auto parentVertex = GetVertexByName(file.GetName());
      if (parentVertex.has_value()) {
        AddEdge(parentVertex.value(), *vit, EdgeType::masterlistLoadAfter);
      }
    }
    for (const auto& file : graph_[*vit].GetUserLoadAfterFiles()) {
      auto parentVertex = GetVertexByName(file.GetName());
      if (parentVertex.has_value()) {
        AddEdge(parentVertex.value(), *vit, EdgeType::userLoadAfter);
      }
    }
  }
}

bool shouldIgnorePlugin(
    const std::string& group,
    const std::string& pluginName,
    const std::map<std::string, std::unordered_set<std::string>>&
        groupPluginsToIgnore) {
  auto pluginsToIgnore = groupPluginsToIgnore.find(group);
  if (pluginsToIgnore != groupPluginsToIgnore.end()) {
    return pluginsToIgnore->second.count(pluginName) > 0;
  }

  return false;
}

bool shouldIgnoreGroupEdge(
    const PluginSortingData& fromPlugin,
    const PluginSortingData& toPlugin,
    const std::map<std::string, std::unordered_set<std::string>>&
        groupPluginsToIgnore) {
  return shouldIgnorePlugin(
             fromPlugin.GetGroup(), toPlugin.GetName(), groupPluginsToIgnore) ||
         shouldIgnorePlugin(
             toPlugin.GetGroup(), fromPlugin.GetName(), groupPluginsToIgnore);
}

void ignorePlugin(const std::string& pluginName,
                  const std::unordered_set<std::string>& groups,
                  std::map<std::string, std::unordered_set<std::string>>&
                      groupPluginsToIgnore) {
  for (const auto& group : groups) {
    auto pluginsToIgnore = groupPluginsToIgnore.find(group);
    if (pluginsToIgnore != groupPluginsToIgnore.end()) {
      pluginsToIgnore->second.insert(pluginName);
    } else {
      groupPluginsToIgnore.emplace(
          group, std::unordered_set<std::string>({pluginName}));
    }
  }
}

// Look for paths to targetGroupName from group. Don't pass visitedGroups by
// reference as each after group should be able to record paths independently.
std::unordered_set<std::string> pathfinder(
    const Group& group,
    const std::string& targetGroupName,
    const std::unordered_set<Group>& groups,
    std::unordered_set<std::string> visitedGroups) {
  // If the current group is the target group, return the set of groups in the
  // path leading to it.
  if (group.GetName() == targetGroupName) {
    return visitedGroups;
  }

  if (group.GetAfterGroups().empty()) {
    return std::unordered_set<std::string>();
  }

  visitedGroups.insert(group.GetName());

  // Call pathfinder on each after group. We want to find all paths, so merge
  // all return values.
  std::unordered_set<std::string> mergedVisitedGroups;
  for (const auto& afterGroupName : group.GetAfterGroups()) {
    auto afterGroup = *groups.find(Group(afterGroupName));

    auto recursedVisitedGroups =
        pathfinder(afterGroup, targetGroupName, groups, visitedGroups);

    mergedVisitedGroups.insert(recursedVisitedGroups.begin(),
                               recursedVisitedGroups.end());
  }

  // Return mergedVisitedGroups if it is empty, to indicate the current group's
  // after groups had no path to the target group.
  if (mergedVisitedGroups.empty()) {
    return mergedVisitedGroups;
  }

  // If any after groups had paths to the target group, mergedVisitedGroups
  // will be non-empty. To ensure that it contains full paths, merge it
  // with visitedGroups and return that merged set.
  visitedGroups.insert(mergedVisitedGroups.begin(), mergedVisitedGroups.end());

  return visitedGroups;
}

std::unordered_set<std::string> getGroupsInPaths(
    const std::unordered_set<Group>& groups,
    const std::string& firstGroupName,
    const std::string& lastGroupName) {
  // Groups are linked in reverse order, i.e. firstGroup can be found from
  // lastGroup, but not the other way around.
  auto lastGroup = *groups.find(Group(lastGroupName));

  auto groupsInPaths = pathfinder(
      lastGroup, firstGroupName, groups, std::unordered_set<std::string>());

  groupsInPaths.erase(lastGroupName);

  return groupsInPaths;
}

void PluginSorter::AddGroupEdges() {
  std::vector<std::pair<vertex_t, vertex_t>> acyclicEdgePairs;
  std::map<std::string, std::unordered_set<std::string>> groupPluginsToIgnore;
  for (const vertex_t& vertex :
       boost::make_iterator_range(boost::vertices(graph_))) {
    for (const auto& pluginName : graph_[vertex].GetAfterGroupPlugins()) {
      auto parentVertex = GetVertexByName(pluginName);
      if (!parentVertex.has_value()) {
        continue;
      }

      if (EdgeCreatesCycle(parentVertex.value(), vertex)) {
        auto& fromPlugin = graph_[parentVertex.value()];
        auto& toPlugin = graph_[vertex];

        if (logger_) {
          logger_->trace(
              "Skipping group edge from \"{}\" to \"{}\" as it would "
              "create a cycle.",
              fromPlugin.GetName(),
              toPlugin.GetName());
        }

        // If the earlier plugin is not a master and the later plugin is,
        // don't ignore the plugin with the default group for all
        // intermediate plugins, as some of those plugins may be masters
        // that wouldn't be involved in the cycle, and any of those
        // plugins that are not masters would have their own cycles
        // detected anyway.
        if (!fromPlugin.IsMaster() && toPlugin.IsMaster()) {
          continue;
        }

        // The default group is a special case, as it's given to plugins
        // with no metadata. If a plugin in the default group causes
        // a cycle due to its group, ignore that plugin's group for all
        // groups in the group graph paths between default and the other
        // plugin's group.
        std::string pluginToIgnore;
        if (toPlugin.GetGroup() == Group().GetName()) {
          pluginToIgnore = toPlugin.GetName();
        } else if (fromPlugin.GetGroup() == Group().GetName()) {
          pluginToIgnore = fromPlugin.GetName();
        } else {
          // If neither plugin is in the default group, it's impossible
          // to decide which group to ignore, so ignore neither of them.
          continue;
        }

        auto groupsInPaths = getGroupsInPaths(
            groups_, fromPlugin.GetGroup(), toPlugin.GetGroup());

        ignorePlugin(pluginToIgnore, groupsInPaths, groupPluginsToIgnore);

        continue;
      }

      acyclicEdgePairs.push_back(std::make_pair(parentVertex.value(), vertex));
    }
  }

  for (const auto& edgePair : acyclicEdgePairs) {
    auto& fromPlugin = graph_[edgePair.first];
    auto& toPlugin = graph_[edgePair.second];
    bool ignore =
        shouldIgnoreGroupEdge(fromPlugin, toPlugin, groupPluginsToIgnore);

    if (!ignore) {
      AddEdge(edgePair.first, edgePair.second, EdgeType::group);
    } else if (logger_) {
      logger_->trace(
          "Skipping group edge from \"{}\" to \"{}\" as it would "
          "create a multi-group cycle.",
          fromPlugin.GetName(),
          toPlugin.GetName());
    }
  }
}

void PluginSorter::AddOverlapEdges() {
  vertex_it vit, vitend;
  for (tie(vit, vitend) = boost::vertices(graph_); vit != vitend; ++vit) {
    vertex_t vertex = *vit;

    if (graph_[vertex].NumOverrideFormIDs() == 0) {
      if (logger_) {
        logger_->trace(
            "Skipping vertex for \"{}\": the plugin contains no override "
            "records.",
            graph_[vertex].GetName());
      }
      continue;
    }

    for (vertex_it vit2 = std::next(vit); vit2 != vitend; ++vit2) {
      vertex_t otherVertex = *vit2;

      if (vertex == otherVertex ||
          boost::edge(vertex, otherVertex, graph_).second ||
          boost::edge(otherVertex, vertex, graph_).second ||
          graph_[vertex].NumOverrideFormIDs() ==
              graph_[otherVertex].NumOverrideFormIDs() ||
          !graph_[vertex].DoFormIDsOverlap(graph_[otherVertex])) {
        continue;
      }

      vertex_t toVertex, fromVertex;
      if (graph_[vertex].NumOverrideFormIDs() >
          graph_[otherVertex].NumOverrideFormIDs()) {
        fromVertex = vertex;
        toVertex = otherVertex;
      } else {
        fromVertex = otherVertex;
        toVertex = vertex;
      }

      if (!EdgeCreatesCycle(fromVertex, toVertex))
        AddEdge(fromVertex, toVertex, EdgeType::overlap);
    }
  }
}

int ComparePlugins(const PluginSortingData& plugin1,
                   const PluginSortingData& plugin2) {
  if (plugin1.GetLoadOrderIndex().has_value() &&
      !plugin2.GetLoadOrderIndex().has_value()) {
    return -1;
  }

  if (!plugin1.GetLoadOrderIndex().has_value() &&
      plugin2.GetLoadOrderIndex().has_value()) {
    return 1;
  }

  if (plugin1.GetLoadOrderIndex().has_value() &&
      plugin2.GetLoadOrderIndex().has_value()) {
    if (plugin1.GetLoadOrderIndex().value() <
        plugin2.GetLoadOrderIndex().value()) {
      return -1;
    } else {
      return 1;
    }
  }

  // Neither plugin has a load order position. Compare plugin basenames to
  // get an ordering.
  auto name1 = plugin1.GetName();
  auto name2 = plugin2.GetName();
  auto basename1 = name1.substr(0, name1.length() - 4);
  auto basename2 = name2.substr(0, name2.length() - 4);

  int result = CompareFilenames(basename1, basename2);

  if (result != 0) {
    return result;
  } else {
    // Could be a .esp and .esm plugin with the same basename,
    // compare their extensions.
    auto ext1 = name1.substr(name1.length() - 4);
    auto ext2 = name2.substr(name2.length() - 4);
    return CompareFilenames(ext1, ext2);
  }
}

void PluginSorter::AddTieBreakEdges() {
  // In order for the sort to be performed stably, there must be only one
  // possible result. This can be enforced by adding edges between all vertices
  // that aren't already linked. Use existing load order to decide the direction
  // of these edges.
  vertex_it vit, vitend;
  for (tie(vit, vitend) = boost::vertices(graph_); vit != vitend; ++vit) {
    vertex_t vertex = *vit;

    for (vertex_it vit2 = std::next(vit); vit2 != vitend; ++vit2) {
      vertex_t otherVertex = *vit2;

      vertex_t toVertex, fromVertex;
      if (ComparePlugins(graph_[vertex], graph_[otherVertex]) < 0) {
        fromVertex = vertex;
        toVertex = otherVertex;
      } else {
        fromVertex = otherVertex;
        toVertex = vertex;
      }

      if (!EdgeCreatesCycle(fromVertex, toVertex))
        AddEdge(fromVertex, toVertex, EdgeType::tieBreak);
    }
  }
}
}
