# Shim: forward direct find_package(fleet-protocol-cxx-helpers-static) calls to
# the BA FetchContent wrapper. cmake picks this up via CMAKE_MODULE_PATH.
find_package(BAFleetProtocolCpp REQUIRED)
set(fleet-protocol-cxx-helpers-static_FOUND TRUE)
