include_guard(GLOBAL)

# Provides: fleet-protocol-interface::{common-headers-interface,
#           module-maintainer-external-server-interface, protobuf-cpp-interface}
# Resolution order: in-scope target → system config package → FetchContent source build.
# Note: fleet-protocol uses cmlib internally; FindCMLIB.cmake shim handles it.

if(TARGET fleet-protocol-interface::common-headers-interface)
    set(BAFleetProtocolInterface_FOUND TRUE)
    return()
endif()

if(fleet-protocol-interface_DIR MATCHES "${CMAKE_BINARY_DIR}")
    unset(fleet-protocol-interface_DIR CACHE)
endif()
find_package(fleet-protocol-interface QUIET CONFIG)
if(fleet-protocol-interface_FOUND)
    message(STATUS "[BA] fleet-protocol-interface: found via config package")
    set(BAFleetProtocolInterface_FOUND TRUE)
    return()
endif()

message(STATUS "[BA] fleet-protocol-interface: not found locally, fetching via FetchContent")
include(FetchContent)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../fleet-protocol/CMakeLists.txt")
    set(FETCHCONTENT_SOURCE_DIR_FLEET_PROTOCOL_INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/../fleet-protocol" CACHE PATH "" FORCE)
endif()
FetchContent_Declare(fleet_protocol_interface
    GIT_REPOSITORY https://github.com/bringauto/fleet-protocol.git
    GIT_TAG        v2.1.0
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(fleet_protocol_interface)
# fleet-protocol's protobuf CMakeLists.txt omits LINK_LIBRARIES from CMDEF_ADD_LIBRARY,
# so protobuf headers are not on the compile path for ExternalProtocol.pb.cc.
# Inject the link dependency explicitly so protobuf::libprotobuf's include dirs propagate.
if(TARGET protobuf-cpp-interface AND TARGET protobuf::libprotobuf)
    target_link_libraries(protobuf-cpp-interface PUBLIC protobuf::libprotobuf)
endif()
# When cmlib installs fleet-protocol, every sub-library's headers land under one
# shared install prefix's include/, so common-headers-interface alone ends up
# exposing every header regardless of which specific interface a consumer linked.
# FetchContent keeps each sub-library's include dir separate (its own build tree),
# so replicate the merged layout by adding every sub-library's include dir to
# common-headers-interface directly — consumers that only link common-headers-interface
# (e.g. a module's own source files needing module_manager.h) still find everything.
if(TARGET common-headers-interface)
    foreach(_fp_extra_inc
            lib/module_gateway/include
            lib/internal_client/include
            lib/module_maintainer/module_gateway/include
            lib/module_maintainer/external_server/include)
        if(EXISTS "${fleet_protocol_interface_SOURCE_DIR}/${_fp_extra_inc}")
            target_include_directories(common-headers-interface INTERFACE
                "$<BUILD_INTERFACE:${fleet_protocol_interface_SOURCE_DIR}/${_fp_extra_inc}>")
        endif()
    endforeach()
    unset(_fp_extra_inc)
endif()
# CMDEF_ADD_LIBRARY creates unnamespaced targets; create fleet-protocol-interface:: aliases
# so that downstream cmake find_package consumers can use the expected namespaced form.
foreach(_fp_iface_target
        common-headers-interface
        internal-client-interface
        module-gateway-interface
        module-maintainer-module-gateway-interface
        module-maintainer-external-server-interface
        protobuf-cpp-interface)
    if(TARGET ${_fp_iface_target}
            AND NOT TARGET fleet-protocol-interface::${_fp_iface_target})
        add_library(fleet-protocol-interface::${_fp_iface_target}
                    ALIAS ${_fp_iface_target})
    endif()
endforeach()
set(BAFleetProtocolInterface_FOUND TRUE)
