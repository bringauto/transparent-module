find_package(BANlohmannJson REQUIRED)
# fleet-protocol-interface's internal protobuf fetch is a no-op under our
# FindCMLIB.cmake shim (BA_PACKAGE_LIBRARY does nothing) — resolve protobuf::libprotobuf
# ourselves first so FindBAFleetProtocolInterface.cmake's link-libraries fixup has
# a real target to attach to. Same fix BAF-1706 needed in external-server-cpp.
find_package(BAProtobuf REQUIRED)
find_package(BAFleetProtocolInterface REQUIRED)
find_package(BAAeron REQUIRED)
find_package(BAAsyncFunctionExecution REQUIRED)
find_package(BAFleetProtocolCpp REQUIRED)

if (FLEET_PROTOCOL_BUILD_EXTERNAL_SERVER)
    find_package(BABoost REQUIRED)
    find_package(BAZlib REQUIRED)
    find_package(BACppRestSdk REQUIRED)
    find_package(BAFleetHttpClient REQUIRED)
endif ()
