include(ExternalProject)

# Protect against multiple inclusion, which would fail when already imported targets are added once more.
set(_targetsDefined)
set(_targetsNotDefined)
set(_expectedTargets)
foreach(_expectedTarget yaml-cpp::yaml-cpp)
    list(APPEND _expectedTargets ${_expectedTarget})
    if(NOT TARGET ${_expectedTarget})
        list(APPEND _targetsNotDefined ${_expectedTarget})
    endif()
    if(TARGET ${_expectedTarget})
        list(APPEND _targetsDefined ${_expectedTarget})
    endif()
endforeach()
if("${_targetsDefined}" STREQUAL "${_expectedTargets}")
    unset(_targetsDefined)
    unset(_targetsNotDefined)
    unset(_expectedTargets)
    set(CMAKE_IMPORT_FILE_VERSION)
    cmake_policy(POP)
    return()
endif()
if(NOT "${_targetsDefined}" STREQUAL "")
    message(FATAL_ERROR "Some (but not all) targets in this export set were already defined.\nTargets Defined: ${_targetsDefined}\nTargets not yet defined: ${_targetsNotDefined}\n")
endif()
unset(_targetsDefined)
unset(_targetsNotDefined)
unset(_expectedTargets)

if(NOT YAMLCPP_VERSION)
    message(FATAL_ERROR "YAMLCPP_VERSION is not set")
endif()

ExternalProject_Add(yaml-cpp_external
        PREFIX          "${CMAKE_CURRENT_BINARY_DIR}/external"
        URL             "https://github.com/loot/yaml-cpp/archive/yaml-cpp-${YAMLCPP_VERSION}.tar.gz"
        CMAKE_ARGS      -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DMSVC_SHARED_RT=${MSVC_SHARED_RUNTIME}
                        -DCMAKE_POSITION_INDEPENDENT_CODE=1
        BUILD_COMMAND   ${CMAKE_COMMAND} --build . --target yaml-cpp
        INSTALL_COMMAND "")
ExternalProject_Get_Property(yaml-cpp_external SOURCE_DIR BINARY_DIR)

if (MSVC)
    if (${MSVC_STATIC_RUNTIME})
        set(YAML_CPP_LIBRARY_SUFFIX "${YAML_CPP_LIBRARY_SUFFIX}mt")
    else()
        set(YAML_CPP_LIBRARY_SUFFIX "${YAML_CPP_LIBRARY_SUFFIX}md")
    endif()

    if("${CMAKE_BUILD_TYPE}" MATCHES "^Debug$")
        set(YAML_CPP_LIBRARY_SUFFIX "${YAML_CPP_LIBRARY_SUFFIX}d")
    endif()
endif()

set(YAML_CPP_LIBRARY_SUFFIX "${LIB_SUFFIX}${YAML_CPP_LIBRARY_SUFFIX}${CMAKE_STATIC_LIBRARY_SUFFIX}")

# Hack to make it work, otherwise INTERFACE_INCLUDE_DIRECTORIES will not be propagated
file(MAKE_DIRECTORY "${SOURCE_DIR}/include")

add_library(yaml-cpp::yaml-cpp STATIC IMPORTED)
    add_dependencies(yaml-cpp::yaml-cpp yaml-cpp_external)
    set_target_properties(yaml-cpp::yaml-cpp PROPERTIES
        IMPORTED_LOCATION               "${BINARY_DIR}/${CMAKE_CFG_INTDIR}/libyaml-cpp${YAML_CPP_LIBRARY_SUFFIX}"
        INTERFACE_INCLUDE_DIRECTORIES   "${SOURCE_DIR}/include")

unset(YAML_DEBUG_LIBRARY_SUFFIX)
unset(YAML_CPP_LIBRARY_SUFFIX)
unset(SOURCE_DIR)
unset(BINARY_DIR)