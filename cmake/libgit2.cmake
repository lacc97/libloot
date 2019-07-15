if(NOT ${USE_SYSTEM_LIBGIT2})
    include(ExternalProject)

    # Protect against multiple inclusion, which would fail when already imported targets are added once more.
    set(_targetsDefined)
    set(_targetsNotDefined)
    set(_expectedTargets)
    foreach(_expectedTarget libgit2::libgit2)
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

    if(NOT LIBGIT2_VERSION)
        message(FATAL_ERROR "LIBGIT2_VERSION is not set")
    endif()

    ExternalProject_Add(libgit2_external
            PREFIX          "${CMAKE_CURRENT_BINARY_DIR}/external"
            URL             "https://github.com/libgit2/libgit2/archive/v${LIBGIT2_VERSION}.tar.gz"
            CMAKE_ARGS      -DBUILD_SHARED_LIBS=OFF -DBUILD_CLAR=OFF -DSTATIC_CRT=${MSVC_STATIC_RUNTIME} -DCMAKE_POSITION_INDEPENDENT_CODE=${CMAKE_POSITION_INDEPENDENT_CODE}
            BUILD_COMMAND   ${CMAKE_COMMAND} --build . --target git2 --config $(CONFIGURATION)
            INSTALL_COMMAND "")
    ExternalProject_Get_Property(libgit2_external SOURCE_DIR BINARY_DIR)

    # Hack to make it work, otherwise INTERFACE_INCLUDE_DIRECTORIES will not be propagated
    file(MAKE_DIRECTORY "${SOURCE_DIR}/include")

    add_library(libgit2::libgit2 STATIC IMPORTED)
        add_dependencies(libgit2::libgit2 libgit2_external)
        set_target_properties(libgit2::libgit2 PROPERTIES
            IMPORTED_LOCATION               "${BINARY_DIR}/${CMAKE_CFG_INTDIR}/${CMAKE_STATIC_LIBRARY_PREFIX}git2${CMAKE_STATIC_LIBRARY_SUFFIX}"
            INTERFACE_INCLUDE_DIRECTORIES   "${SOURCE_DIR}/include")

    unset(SOURCE_DIR)
    unset(BINARY_DIR)
else()
    find_package(PkgConfig REQUIRED)
    if(LIBGIT2_VERSION)
        pkg_check_modules(libgit2 REQUIRED IMPORTED_TARGET libgit2>=${LIBGIT2_VERSION})
    else()
        pkg_check_modules(libgit2 REQUIRED IMPORTED_TARGET libgit2)
    endif()
    add_library(libgit2::libgit2 INTERFACE IMPORTED)
        set_target_properties(libgit2::libgit2 PROPERTIES
            INTERFACE_LINK_LIBRARIES        "PkgConfig::libgit2")
endif()