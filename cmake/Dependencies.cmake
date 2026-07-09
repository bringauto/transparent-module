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
    # Operator-facing QUIC transport, ported from teleop-module's equivalent: msquic +
    # protobuf for the operator stream protocol (own .proto — see its file header for why it can't
    # share teleop-module's).
    BA_PACKAGE_LIBRARY(msquic                                v2.5.6)
    BA_PACKAGE_LIBRARY(protobuf                              v4.21.12)

ENDIF ()
