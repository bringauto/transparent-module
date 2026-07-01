# Shim: forward direct find_package(async-function-execution-shared) calls to
# the BA FetchContent wrapper. cmake picks this up via CMAKE_MODULE_PATH.
find_package(BAAsyncFunctionExecution REQUIRED)
set(async-function-execution-shared_FOUND TRUE)
