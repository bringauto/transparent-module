SET(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH FALSE)

BA_PACKAGE_LIBRARY(nlohmann-json            v3.12.0 NO_DEBUG ON)
BA_PACKAGE_LIBRARY(fleet-protocol-cpp       v1.2.0)
BA_PACKAGE_LIBRARY(async-function-execution v1.0.0)
BA_PACKAGE_LIBRARY(aeron                   v1.48.6)
BA_PACKAGE_LIBRARY(fleet-protocol-interface v2.1.0 NO_DEBUG ON)

IF (FLEET_PROTOCOL_BUILD_EXTERNAL_SERVER)
    BA_PACKAGE_LIBRARY(fleet-http-client-shared             v2.0.2)
    BA_PACKAGE_LIBRARY(boost         v1.86.0)
    BA_PACKAGE_LIBRARY(cpprestsdk    v2.10.20)
    BA_PACKAGE_LIBRARY(zlib                                 v1.3.2)
    # Operator-facing QUIC transport: msquic is provided transitively by ba-quic-lib
    # (linked PUBLIC), not resolved directly here. See cmake/FindBAQuicLib.cmake.
    # BA_PACKAGE_DEPS_IMPORTED(transparent-module-interface) still ships it: it walks
    # LINK_LIBRARIES/INTERFACE_LINK_LIBRARIES recursively through ba-quic-lib (a regular, non-imported
    # target, so it has no IMPORTED_LOCATION itself) down to msquic's own IMPORTED_LOCATION, and
    # installs that .so + its SONAME symlinks.
    BA_PACKAGE_LIBRARY(protobuf                              v4.21.12)

ENDIF ()
