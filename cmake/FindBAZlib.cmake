include_guard(GLOBAL)

# Provides: ZLIB::ZLIB
# Resolution order: in-scope target → system package → FetchContent source build.
# The bringauto/cpp-build-environment image has no libz-dev, so this normally
# falls through to FetchContent (upstream zlib has a v1.3.2 tag, matching the
# old cmlib pin).

if(TARGET ZLIB::ZLIB)
    set(BAZlib_FOUND TRUE)
    return()
endif()

find_package(ZLIB QUIET)
if(ZLIB_FOUND)
    message(STATUS "[BA] zlib: found via system package")
    set(BAZlib_FOUND TRUE)
    return()
endif()

message(STATUS "[BA] zlib: system package not found, fetching via FetchContent")
include(FetchContent)
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG        v1.3.2
    GIT_SHALLOW    TRUE
    OVERRIDE_FIND_PACKAGE)
FetchContent_MakeAvailable(zlib)

# zlib's CMakeLists.txt creates unnamespaced "zlib"/"zlibstatic" targets, not
# the ZLIB::ZLIB imported target that FindZLIB.cmake (module mode) normally
# provides. Alias the static target since BUILD_SHARED_LIBS is forced OFF.
if(NOT TARGET ZLIB::ZLIB)
    if(TARGET zlibstatic)
        add_library(ZLIB::ZLIB ALIAS zlibstatic)
    elseif(TARGET zlib)
        add_library(ZLIB::ZLIB ALIAS zlib)
    endif()
endif()
if(TARGET zlibstatic)
    target_include_directories(zlibstatic PUBLIC
        "$<BUILD_INTERFACE:${zlib_SOURCE_DIR}>"
        "$<BUILD_INTERFACE:${zlib_BINARY_DIR}>")
endif()

set(BAZlib_FOUND TRUE)
