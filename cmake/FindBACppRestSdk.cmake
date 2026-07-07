include_guard(GLOBAL)

# Provides: cpprestsdk::cpprest
# Resolution order: in-scope target → system config package → FetchContent source build.
# Uses the BringAuto fork (github.com/bringauto/cpprestsdk) — vanilla cpprestsdk only
# goes up to v2.10.19, so the v2.10.20 tag pinned by the old cmlib Dependencies.cmake
# confirms this is the fork, not upstream Microsoft/cpprestsdk.

if(TARGET cpprestsdk::cpprest)
    set(BACppRestSdk_FOUND TRUE)
    return()
endif()

if(cpprestsdk_DIR MATCHES "${CMAKE_BINARY_DIR}")
    unset(cpprestsdk_DIR CACHE)
endif()
find_package(cpprestsdk QUIET CONFIG)
if(cpprestsdk_FOUND)
    message(STATUS "[BA] cpprestsdk: found via system package")
    set(BACppRestSdk_FOUND TRUE)
    return()
endif()

message(STATUS "[BA] cpprestsdk: not found locally, fetching via FetchContent")
include(FetchContent)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
# The bundled websocketpp fails to compile against GCC 13's stricter parsing of
# template-id destructors; the module only needs plain HTTP polling, not websockets.
set(CPPREST_EXCLUDE_WEBSOCKETS ON CACHE BOOL "" FORCE)
set(WERROR OFF CACHE BOOL "" FORCE)
set(BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS   OFF CACHE BOOL "" FORCE)
# OVERRIDE_FIND_PACKAGE: FetchContent_Declare's name matches the plain
# FIND_PACKAGE(cpprestsdk ...) call in CMakeLists.txt, so it intercepts it directly.
FetchContent_Declare(cpprestsdk
    GIT_REPOSITORY https://github.com/bringauto/cpprestsdk.git
    GIT_TAG        v2.10.20
    GIT_SHALLOW    TRUE
    OVERRIDE_FIND_PACKAGE)
FetchContent_MakeAvailable(cpprestsdk)
# cpprestsdk's build tree only defines the raw "cpprest" target — the
# namespaced "cpprestsdk::cpprest" alias normally comes from its install-export,
# which FetchContent bypasses. Create it manually.
if(TARGET cpprest AND NOT TARGET cpprestsdk::cpprest)
    add_library(cpprestsdk::cpprest ALIAS cpprest)
endif()
set(BACppRestSdk_FOUND TRUE)
