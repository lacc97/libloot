if(NOT ${USE_SYSTEM_SPDLOG})
    include(ExternalProject)

    # Protect against multiple inclusion, which would fail when already imported targets are added once more.
    set(_targetsDefined)
    set(_targetsNotDefined)
    set(_expectedTargets)
    foreach(_expectedTarget spdlog::spdlog spdlog::spdlog_header_only)
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

    if(NOT SPDLOG_VERSION)
        message(FATAL_ERROR "SPDLOG_VERSION is not set")
    endif()

    ExternalProject_Add(spdlog_external
            PREFIX          "${CMAKE_CURRENT_BINARY_DIR}/external"
            URL             "https://github.com/gabime/spdlog/archive/v${SPDLOG_VERSION}.tar.gz"
            INSTALL_COMMAND "")
    ExternalProject_Get_Property(spdlog_external SOURCE_DIR)

    # Hack to make it work, otherwise INTERFACE_INCLUDE_DIRECTORIES will not be propagated
    file(MAKE_DIRECTORY "${SOURCE_DIR}/include")

    add_library(spdlog::spdlog_header_only INTERFACE IMPORTED)
        add_dependencies(spdlog::spdlog_header_only spdlog_external)
        set_target_properties(spdlog::spdlog_header_only PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES   "${SOURCE_DIR}/include")

    add_library(spdlog::spdlog STATIC IMPORTED)
        add_dependencies(spdlog::spdlog spdlog_external)
        set_target_properties(spdlog::spdlog PROPERTIES
            IMPORTED_LOCATION               "${BINARY_DIR}/googlemock/gtest/${CMAKE_CFG_INTDIR}/${CMAKE_STATIC_LIBRARY_PREFIX}gtest${CMAKE_STATIC_LIBRARY_SUFFIX}"
            INTERFACE_LINK_LIBRARIES        "spdlog::spdlog_header_only")

    unset(SOURCE_DIR)
else()
    find_package(spdlog REQUIRED)
endif()