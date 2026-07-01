# Shim: forward direct find_package(fleet-protocol-interface) calls to the BA
# FetchContent wrapper. cmake picks this up via CMAKE_MODULE_PATH.
find_package(BAFleetProtocolInterface REQUIRED)
set(fleet-protocol-interface_FOUND TRUE)
