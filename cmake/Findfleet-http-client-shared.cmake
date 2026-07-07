# Shim: forward direct find_package(fleet-http-client-shared) calls to the BA
# FetchContent wrapper. cmake picks this up via CMAKE_MODULE_PATH.
find_package(BAFleetHttpClient REQUIRED)
set(fleet-http-client-shared_FOUND TRUE)
