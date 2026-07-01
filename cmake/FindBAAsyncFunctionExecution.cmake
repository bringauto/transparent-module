include_guard(GLOBAL)

# Provides: async-function-execution-shared::async-function-execution-shared
# Resolution order: in-scope target → system config package → FetchContent source build.
# Note: async-function-execution uses cmlib internally; FindCMLIB.cmake shim handles it.

if(TARGET async-function-execution-shared::async-function-execution-shared)
    set(BAAsyncFunctionExecution_FOUND TRUE)
    return()
endif()

if(async-function-execution-shared_DIR MATCHES "${CMAKE_BINARY_DIR}")
    unset(async-function-execution-shared_DIR CACHE)
endif()
find_package(async-function-execution-shared QUIET CONFIG)
if(async-function-execution-shared_FOUND)
    message(STATUS "[BA] async-function-execution: found via config package")
    set(BAAsyncFunctionExecution_FOUND TRUE)
    return()
endif()

message(STATUS "[BA] async-function-execution: not found locally, fetching via FetchContent")
include(FetchContent)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../async-function-execution/CMakeLists.txt")
    set(FETCHCONTENT_SOURCE_DIR_ASYNC_FUNCTION_EXECUTION
        "${CMAKE_CURRENT_SOURCE_DIR}/../async-function-execution" CACHE PATH "" FORCE)
endif()
FetchContent_Declare(async_function_execution
    GIT_REPOSITORY https://github.com/bringauto/async-function-execution.git
    GIT_TAG        v1.0.0
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(async_function_execution)
# CMDEF_ADD_LIBRARY creates async-function-execution-shared; create the namespaced alias.
if(TARGET async-function-execution-shared
        AND NOT TARGET async-function-execution-shared::async-function-execution-shared)
    add_library(async-function-execution-shared::async-function-execution-shared
                ALIAS async-function-execution-shared)
endif()

# Install the shared library so the binary can find it via RPATH at runtime.
# CMDEF_INSTALL() is a no-op in our shim, so we register the install rule here.
if(TARGET async-function-execution-shared)
    get_target_property(_afe_ttype async-function-execution-shared TYPE)
    if(_afe_ttype STREQUAL "SHARED_LIBRARY")
        install(TARGETS async-function-execution-shared
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
    endif()
    unset(_afe_ttype)
endif()

set(BAAsyncFunctionExecution_FOUND TRUE)
