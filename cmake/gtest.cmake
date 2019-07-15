
if(NOT ${USE_SYSTEM_GTEST})
    include(ExternalProject)

    # Protect against multiple inclusion, which would fail when already imported targets are added once more.
    set(_targetsDefined)
    set(_targetsNotDefined)
    set(_expectedTargets)
    foreach(_expectedTarget GTest::GTest)
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

    if(NOT GTEST_VERSION)
        message(FATAL_ERROR "GTEST_VERSION is not set")
    endif()

    ExternalProject_Add(gtest_external
            PREFIX          "${CMAKE_CURRENT_BINARY_DIR}/external"
            URL             "https://github.com/google/googletest/archive/release-${GTEST_VERSION}.tar.gz"
            CMAKE_ARGS      -Dgtest_force_shared_crt=${MSVC_SHARED_RUNTIME} -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
            INSTALL_COMMAND "")
    ExternalProject_Get_Property(gtest_external SOURCE_DIR BINARY_DIR)

    # Hack to make it work, otherwise INTERFACE_INCLUDE_DIRECTORIES will not be propagated
    file(MAKE_DIRECTORY "${SOURCE_DIR}/googletest/include")

    add_library(GTest::GTest STATIC IMPORTED)
        add_dependencies(GTest::GTest gtest_external)
        set_target_properties(GTest::GTest PROPERTIES
            IMPORTED_LOCATION               "${BINARY_DIR}/googlemock/gtest/${CMAKE_CFG_INTDIR}/${CMAKE_STATIC_LIBRARY_PREFIX}gtest${CMAKE_STATIC_LIBRARY_SUFFIX}"
            INTERFACE_INCLUDE_DIRECTORIES   "${SOURCE_DIR}/googletest/include")

    unset(SOURCE_DIR)
    unset(BINARY_DIR)
else()
    if(NOT GTEST_VERSION)
        message(WARNING "GTEST_VERSION is not set")
        find_package(GTest REQUIRED)
    else()
        find_package(GTest REQUIRED VERSION ${GTEST_VERSION})
    endif()
endif()