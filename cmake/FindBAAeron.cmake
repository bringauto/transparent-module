include_guard(GLOBAL)

# Provides: aeron (imported target from aeron cmake package)
# Resolution order: in-scope target → system config package → FetchContent source build.

if(TARGET aeron)
    set(BAAeron_FOUND TRUE)
    return()
endif()

if(aeron_DIR MATCHES "${CMAKE_BINARY_DIR}")
    unset(aeron_DIR CACHE)
endif()
find_package(aeron QUIET CONFIG)
if(aeron_FOUND)
    message(STATUS "[BA] aeron: found via system package")
    set(BAAeron_FOUND TRUE)
    return()
endif()

message(STATUS "[BA] aeron: system package not found, fetching via FetchContent")
include(FetchContent)
set(AERON_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(AERON_TESTS OFF CACHE BOOL "" FORCE)
set(AERON_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(AERON_SYSTEM_TESTS OFF CACHE BOOL "" FORCE)
set(AERON_INSTALL_TARGETS OFF CACHE BOOL "" FORCE)
# Archive API requires Java for code generation; disable it.
# Driver (C media driver) IS needed — async-function-execution links aeron::aeron_driver.
set(BUILD_AERON_ARCHIVE_API OFF CACHE BOOL "" FORCE)
# OVERRIDE_FIND_PACKAGE: intercepts direct find_package(aeron ...) calls in
# subdirectories so they resolve to this FetchContent copy (name matches case-insensitively).
FetchContent_Declare(aeron
    GIT_REPOSITORY https://github.com/aeron-io/aeron.git
    GIT_TAG        1.48.6
    GIT_SHALLOW    TRUE
    OVERRIDE_FIND_PACKAGE)
FetchContent_MakeAvailable(aeron)

# async-function-execution includes <aeronmd/aeron_driver.h> which expects the installed
# header layout (install(DIRECTORY ./ DESTINATION include/aeronmd ...)).
# In a FetchContent build, headers live in aeron-driver/src/main/c/ with no aeronmd/
# subdirectory. Create a shim include dir containing an aeronmd/ symlink so the header
# resolves without running cmake --install.
set(_aeron_driver_c "${aeron_SOURCE_DIR}/aeron-driver/src/main/c")
set(_aeron_inc_shim "${aeron_BINARY_DIR}/include-shim")
file(MAKE_DIRECTORY "${_aeron_inc_shim}")
if(NOT EXISTS "${_aeron_inc_shim}/aeronmd")
    file(CREATE_LINK "${_aeron_driver_c}" "${_aeron_inc_shim}/aeronmd" SYMBOLIC)
endif()
target_include_directories(aeron_driver PUBLIC
    "$<BUILD_INTERFACE:${_aeron_inc_shim}>")
target_include_directories(aeron_driver_static PUBLIC
    "$<BUILD_INTERFACE:${_aeron_inc_shim}>")

# Install shared aeron libraries so the external-server-cpp binary can find them
# at runtime via its RPATH ($ORIGIN/../lib).  AERON_INSTALL_TARGETS=OFF skips
# aeron's own install targets, so we register them explicitly here.
foreach(_aeron_tgt aeron aeron_client_shared aeron_driver)
    if(TARGET ${_aeron_tgt})
        get_target_property(_aeron_ttype ${_aeron_tgt} TYPE)
        if(_aeron_ttype STREQUAL "SHARED_LIBRARY")
            install(TARGETS ${_aeron_tgt} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
        endif()
    endif()
endforeach()
unset(_aeron_tgt)
unset(_aeron_ttype)

set(BAAeron_FOUND TRUE)
