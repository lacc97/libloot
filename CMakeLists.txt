cmake_minimum_required (VERSION 3.11)
cmake_policy(SET CMP0015 NEW)
project (libloot)

include(ExternalProject)

option(BUILD_TESTS          "Build libloot and libloot_api tests"   OFF)
option(BUILD_SHARED_LIBS    "Build a shared library"                ON)
option(MSVC_STATIC_RUNTIME  "Build with static runtime libs (/MT)"  OFF)

set(MSVC_SHARED_RUNTIME $<NOT:$<BOOL:${MSVC_STATIC_RUNTIME}>>)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

##############################
# Get Build Revision
##############################

find_package(Git)

IF (GIT_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
                  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                  OUTPUT_VARIABLE GIT_COMMIT_STRING
                  OUTPUT_STRIP_TRAILING_WHITESPACE)
ELSE()
    SET (GIT_COMMIT_STRING "unknown")
ENDIF ()

message(STATUS "Git revision: ${GIT_COMMIT_STRING}")

# Write to file.
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/api/loot_version.cpp.in" "${CMAKE_CURRENT_BINARY_DIR}/generated/loot_version.cpp" @ONLY)

##############################
# External Projects
##############################

if(NOT DEFINED Boost_USE_STATIC_LIBS)
    set (Boost_USE_STATIC_LIBS ON)
endif()
set (Boost_USE_MULTITHREADED ON)

IF (MSVC)
    set (Boost_USE_STATIC_RUNTIME ${MSVC_STATIC_RUNTIME})
ELSE()
    set (Boost_USE_STATIC_RUNTIME OFF)
ENDIF()

IF (NOT ${Boost_USE_STATIC_LIBS})
    set(BOOST_DL Boost::dynamic_linking)
ENDIF ()

IF (CMAKE_SYSTEM_NAME MATCHES "Windows")
    IF (NOT "${CMAKE_GENERATOR}" MATCHES "(Win64|IA64)")
        set(RUST_TARGET i686-pc-windows-msvc)
    ELSE ()
        set(RUST_TARGET x86_64-pc-windows-msvc)
    ENDIF ()
ELSE ()
    set(RUST_TARGET x86_64-unknown-linux-gnu)
ENDIF ()

find_package(Boost REQUIRED COMPONENTS locale system)
set(BOOST_LIBS ${BOOST_DL} Boost::locale Boost::system)

if (NOT CMAKE_SYSTEM_NAME MATCHES "Windows")
    find_package(ICU REQUIRED COMPONENTS uc)
endif()

set(ESPLUGIN_VERSION 2.1.2)
include(cmake/esplugin.cmake)

set(LIBGIT2_VERSION 0.28.2)
include(cmake/libgit2.cmake)

set(LIBLOADORDER_VERSION 12.0.1)
include(cmake/libloadorder.cmake)

set(LCI_VERSION 2.0.0)
include(cmake/loot-condition-interpreter.cmake)

set(SPDLOG_VERSION 1.3.1)
include(cmake/spdlog.cmake)

set(YAMLCPP_VERSION 0.6.2+merge-key-support.2)
include(cmake/yaml-cpp.cmake)

if(${BUILD_TESTS})
    set(GTEST_VERSION 1.8.1)
    include(cmake/gtest.cmake)

    ExternalProject_Add(testing-metadata
                        PREFIX "external"
                        GIT_REPOSITORY "https://github.com/loot/testing-metadata"
                        GIT_TAG "1.4.0"
                        CONFIGURE_COMMAND ""
                        BUILD_COMMAND ""
                        INSTALL_COMMAND "")

    ExternalProject_Add(testing-plugins
                        PREFIX "external"
                        URL "https://github.com/Ortham/testing-plugins/archive/1.4.1.tar.gz"
                        CONFIGURE_COMMAND ""
                        BUILD_COMMAND ""
                        INSTALL_COMMAND "")
endif()


##############################
# General Settings
##############################


set (LIBLOOT_SRC "${CMAKE_CURRENT_BINARY_DIR}/generated/loot_version.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/src/api/api.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/src/api/api_database.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/src/api/error_categories.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/src/api/metadata/condition_evaluator.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/src/api/metadata/conditional_metadata.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/src/api/metadata/file.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/src/api/metadata/group.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/src/api/metadata/location.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/src/api/metadata/message.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/src/api/metadata/message_content.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/src/api/metadata/plugin_cleaning_data.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/src/api/metadata/plugin_metadata.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/src/api/metadata/tag.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/src/api/game/game.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/src/api/game/game_cache.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/src/api/game/load_order_handler.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/src/api/metadata_list.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/src/api/masterlist.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/src/api/plugin.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/src/api/sorting/cyclic_interaction_error.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/src/api/sorting/group_sort.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/src/api/sorting/plugin_sorter.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/src/api/sorting/plugin_sorting_data.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/src/api/helpers/crc.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/src/api/helpers/git_helper.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/src/api/helpers/text.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/src/api/vertex.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/src/api/resource.rc")

set (LIBLOOT_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/loot/api.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/include/loot/api_decorator.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/include/loot/database_interface.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/include/loot/exception/error_categories.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/include/loot/exception/condition_syntax_error.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/include/loot/exception/cyclic_interaction_error.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/include/loot/exception/file_access_error.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/include/loot/exception/git_state_error.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/include/loot/exception/undefined_group_error.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/include/loot/enum/edge_type.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/include/loot/enum/game_type.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/include/loot/enum/log_level.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/include/loot/enum/message_type.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/include/loot/game_interface.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/include/loot/loot_version.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/include/loot/metadata/conditional_metadata.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/include/loot/metadata/file.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/include/loot/metadata/group.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/include/loot/metadata/location.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/include/loot/metadata/message.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/include/loot/metadata/message_content.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/include/loot/metadata/plugin_cleaning_data.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/include/loot/metadata/plugin_metadata.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/include/loot/metadata/tag.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/include/loot/plugin_interface.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/include/loot/struct/masterlist_info.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/include/loot/struct/simple_message.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/include/loot/vertex.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/src/api/api_database.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/src/api/metadata/condition_evaluator.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/src/api/metadata/yaml/file.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/src/api/metadata/yaml/group.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/src/api/metadata/yaml/location.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/src/api/metadata/yaml/message.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/src/api/metadata/yaml/message_content.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/src/api/metadata/yaml/plugin_cleaning_data.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/src/api/metadata/yaml/plugin_metadata.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/src/api/metadata/yaml/set.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/src/api/metadata/yaml/tag.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/src/api/game/game.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/src/api/game/game_cache.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/src/api/game/load_order_handler.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/src/api/metadata_list.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/src/api/masterlist.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/src/api/plugin.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/src/api/sorting/group_sort.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/src/api/sorting/plugin_sorter.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/src/api/sorting/plugin_sorting_data.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/src/api/helpers/git_helper.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/src/api/helpers/crc.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/src/api/helpers/logging.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/src/api/helpers/text.h")

set (LOOT_TESTS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/api/internals/main.cpp")

set (LOOT_TESTS_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/api/internals/game/game_test.h"
                        "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/api/internals/game/game_cache_test.h"
                        "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/api/internals/game/load_order_handler_test.h"
                        "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/api/internals/helpers/git_helper_test.h"
                        "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/api/internals/helpers/crc_test.h"
                        "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/api/internals/helpers/text_test.h"
                        "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/api/internals/helpers/yaml_set_helpers_test.h"
                        "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/api/internals/metadata/condition_evaluator_test.h"
                        "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/api/internals/metadata/conditional_metadata_test.h"
                        "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/api/internals/metadata/file_test.h"
                        "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/api/internals/metadata/group_test.h"
                        "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/api/internals/metadata/location_test.h"
                        "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/api/internals/metadata/message_test.h"
                        "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/api/internals/metadata/message_content_test.h"
                        "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/api/internals/metadata/plugin_cleaning_data_test.h"
                        "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/api/internals/metadata/plugin_metadata_test.h"
                        "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/api/internals/metadata/tag_test.h"
                        "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/api/internals/plugin_test.h"
                        "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/api/internals/sorting/group_sort_test.h"
                        "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/api/internals/sorting/plugin_sorter_test.h"
                        "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/api/internals/masterlist_test.h"
                        "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/api/internals/metadata_list_test.h"
                        "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/common_game_test_fixture.h"
                        "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/printers.h")

set(LIBLOOT_TESTS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/api/interface/main.cpp")

set(LIBLOOT_TESTS_HEADERS  "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/api/interface/api_game_operations_test.h"
                            "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/api/interface/create_game_handle_test.h"
                            "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/api/interface/database_interface_test.h"
                            "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/api/interface/game_interface_test.h"
                            "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/api/interface/is_compatible_test.h"
                            "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/common_game_test_fixture.h")

source_group("Header Files\\api" FILES ${LIBLOOT_HEADERS})
source_group("Header Files\\tests" FILES ${LOOT_TESTS_HEADERS})
source_group("Header Files\\tests" FILES ${LIBLOOT_TESTS_HEADERS})

source_group("Source Files\\api" FILES ${LIBLOOT_SRC})
source_group("Source Files\\tests" FILES ${LOOT_TESTS_SRC})
source_group("Source Files\\tests" FILES ${LIBLOOT_TESTS_SRC})

##############################
# System-Specific Settings
##############################

# Settings when compiling for Windows.
IF (CMAKE_SYSTEM_NAME MATCHES "Windows")
    add_definitions (-DUNICODE -D_UNICODE)
ENDIF ()

IF (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set (LOOT_LIBS ssl
                   curl
                   z
                   crypto
                   rt
                   pthread
                   icuuc
                   icui18n
                   ssh2
                   http_parser
                   stdc++fs)

    IF (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set (LOOT_LIBS ${LOOT_LIBS} supc++)
    ENDIF ()
ENDIF ()

IF (MSVC)
    # Turn off permissive mode to be more standards-compliant and avoid compiler errors.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /permissive-")
    # Set /bigobj to allow building Debug and RelWithDebInfo tests
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /bigobj")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /bigobj")

    # Update compiler flags.
    IF (MSVC_STATIC_RUNTIME)
        FOREACH(flag
            CMAKE_C_FLAGS_RELEASE CMAKE_C_FLAGS_RELWITHDEBINFO
            CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_DEBUG_INIT
            CMAKE_CXX_FLAGS_RELEASE  CMAKE_CXX_FLAGS_RELWITHDEBINFO
            CMAKE_CXX_FLAGS_DEBUG  CMAKE_CXX_FLAGS_DEBUG_INIT)
            STRING(REPLACE "/MD"  "/MT" "${flag}" "${${flag}}")
            SET("${flag}" "${${flag}} /EHsc")
        ENDFOREACH()
    ENDIF ()

    set (LOOT_LIBS  version
                    ws2_32
                    shlwapi
                    winhttp
                    crypt32
                    Rpcrt4)
ENDIF ()


##############################
# Define Targets
##############################

# Build API.
if(${BUILD_SHARED_LIBS})
    add_library(loot SHARED ${LIBLOOT_SRC} ${LIBLOOT_HEADERS})
else()
    add_library(loot STATIC ${LIBLOOT_SRC} ${LIBLOOT_HEADERS})
endif()
target_include_directories(loot
    PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(loot
    PRIVATE
        Boost::boost ${BOOST_LIBS} ICU::uc libgit2::libgit2 esplugin::esplugin libloadorder::libloadorder loot_condition_interpreter::lci yaml-cpp::yaml-cpp ${LOOT_LIBS})

if(${BUILD_TESTS})
# Build API tests.
add_executable       (libloot_tests ${LIBLOOT_TESTS_SRC} ${LIBLOOT_TESTS_HEADERS})
add_dependencies     (libloot_tests loot GTest testing-metadata testing-plugins)
target_link_libraries(libloot_tests loot ${GTEST_LIBRARIES})

# Build tests.
add_executable       (libloot_internals_tests ${LIBLOOT_SRC} ${LIBLOOT_HEADERS} ${LOOT_TESTS_SRC} ${LOOT_TESTS_HEADERS})
add_dependencies     (libloot_internals_tests esplugin libgit2 libloadorder loot-condition-interpreter spdlog yaml-cpp GTest testing-metadata testing-plugins)
target_link_libraries(libloot_internals_tests ${Boost_LIBRARIES} ${LIBGIT2_LIBRARIES} ${ESPLUGIN_LIBRARIES} ${LIBLOADORDER_LIBRARIES} ${LOOT_LIBS} ${LCI_LIBRARIES} ${YAML_CPP_LIBRARIES} ${GTEST_LIBRARIES} ${ICU_LIBRARIES})
endif()

##############################
# Set Target-Specific Flags
##############################

IF (CMAKE_SYSTEM_NAME MATCHES "Windows")
    set_target_properties (libloot_internals_tests PROPERTIES COMPILE_DEFINITIONS "${COMPILE_DEFINITIONS} LOOT_STATIC")
    IF (BUILD_SHARED_LIBS)
        set_target_properties (loot PROPERTIES COMPILE_DEFINITIONS "${COMPILE_DEFINITIONS} LOOT_EXPORT")
    ELSE ()
        set_target_properties (loot PROPERTIES COMPILE_DEFINITIONS "${COMPILE_DEFINITIONS} LOOT_STATIC")
    ENDIF ()
ENDIF ()


##############################
# Post-Build Steps
##############################

if(${BUILD_TESTS})
# Copy testing metadata
ExternalProject_Get_Property(testing-metadata SOURCE_DIR)
add_custom_command(TARGET libloot_internals_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${SOURCE_DIR}
        "$<TARGET_FILE_DIR:libloot_internals_tests>/testing-metadata")
add_custom_command(TARGET libloot_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${SOURCE_DIR}
        "$<TARGET_FILE_DIR:libloot_tests>/testing-metadata")

# Copy testing plugins
ExternalProject_Get_Property(testing-plugins SOURCE_DIR)
add_custom_command(TARGET libloot_internals_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${SOURCE_DIR}
        $<TARGET_FILE_DIR:libloot_internals_tests>)
add_custom_command(TARGET libloot_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${SOURCE_DIR}
      $<TARGET_FILE_DIR:libloot_tests>)
endif()

########################################
# Install
########################################

install(TARGETS loot EXPORT libloot
        DESTINATION lib)

IF (MSVC)
    install(FILES $<TARGET_PDB_FILE:loot>
    DESTINATION lib/pdb
    OPTIONAL
    CONFIGURATIONS RelWithDebInfo)
ENDIF ()

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
        DESTINATION include)

#install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/docs/html/"
#        DESTINATION docs)

install(EXPORT libloot NAMESPACE libloot::
        DESTINATION cmake)

########################################
# CPack
########################################

IF (GIT_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags --long --always --abbrev=7
                  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                  OUTPUT_VARIABLE GIT_DESCRIBE_STRING
                  OUTPUT_STRIP_TRAILING_WHITESPACE)

    IF (DEFINED ENV{APPVEYOR_REPO_BRANCH})
        set(GIT_DESCRIBE_STRING "${GIT_DESCRIBE_STRING}_$ENV{APPVEYOR_REPO_BRANCH}")
    ELSEIF (DEFINED ENV{TRAVIS_BRANCH})
        set(GIT_DESCRIBE_STRING "${GIT_DESCRIBE_STRING}_$ENV{TRAVIS_BRANCH}")
    ENDIF()
ELSE()
    SET (GIT_DESCRIBE_STRING "unknown-version")
ENDIF ()

if (CMAKE_SYSTEM_NAME MATCHES "Windows")
    set(CPACK_GENERATOR "7Z")
else()
    set(CPACK_GENERATOR "TXZ")
endif()

set(CPACK_PACKAGE_VERSION ${GIT_DESCRIBE_STRING})
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/package")

include(CPack)
