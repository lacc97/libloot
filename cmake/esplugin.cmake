include(ExternalProject)

# Protect against multiple inclusion, which would fail when already imported targets are added once more.
set(_targetsDefined)
set(_targetsNotDefined)
set(_expectedTargets)
foreach(_expectedTarget esplugin::esplugin)
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

if(NOT ESPLUGIN_VERSION)
    message(FATAL_ERROR "ESPLUGIN_VERSION is not set")
endif()

ExternalProject_Add(esplugin_external
        PREFIX              "${CMAKE_CURRENT_BINARY_DIR}/external"
        URL                 "https://github.com/Ortham/esplugin/archive/${ESPLUGIN_VERSION}.tar.gz"
        CONFIGURE_COMMAND   ""
        BUILD_IN_SOURCE     1
        BUILD_COMMAND       cargo build --release --manifest-path ffi/Cargo.toml --target ${RUST_TARGET}
            COMMAND         cbindgen ffi/ -o ffi/include/esplugin.hpp
        INSTALL_COMMAND     "")
ExternalProject_Get_Property(esplugin_external SOURCE_DIR)

# Hack to make it work, otherwise INTERFACE_INCLUDE_DIRECTORIES will not be propagated
file(MAKE_DIRECTORY "${SOURCE_DIR}/ffi/include")

add_library(esplugin::esplugin STATIC IMPORTED)
    add_dependencies(esplugin::esplugin esplugin_external)
    set_target_properties(esplugin::esplugin PROPERTIES
        IMPORTED_LOCATION               "${SOURCE_DIR}/target/${RUST_TARGET}/release/${CMAKE_STATIC_LIBRARY_PREFIX}esplugin_ffi${CMAKE_STATIC_LIBRARY_SUFFIX}"
        INTERFACE_INCLUDE_DIRECTORIES   "${SOURCE_DIR}/ffi/include"
        INTERFACE_LINK_LIBRARIES        "\$<LINK_ONLY:\$<IF:\$<PLATFORM_ID:Windows>,Userenv,dl>>")

unset(SOURCE_DIR)
