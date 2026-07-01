include_guard(GLOBAL)

# Provides: fleet-protocol-cxx-helpers-static::fleet-protocol-cxx-helpers-static
# Resolution order: in-scope target → system config package → FetchContent source build.
# Note: fleet-protocol-cpp uses cmlib internally; FindCMLIB.cmake shim handles it.

if(TARGET fleet-protocol-cxx-helpers-static::fleet-protocol-cxx-helpers-static)
    set(BAFleetProtocolCpp_FOUND TRUE)
    return()
endif()

if(fleet-protocol-cxx-helpers-static_DIR MATCHES "${CMAKE_BINARY_DIR}")
    unset(fleet-protocol-cxx-helpers-static_DIR CACHE)
endif()
find_package(fleet-protocol-cxx-helpers-static QUIET CONFIG)
if(fleet-protocol-cxx-helpers-static_FOUND)
    message(STATUS "[BA] fleet-protocol-cpp: found via config package")
    set(BAFleetProtocolCpp_FOUND TRUE)
    return()
endif()

message(STATUS "[BA] fleet-protocol-cpp: not found locally, fetching via FetchContent")
include(FetchContent)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../fleet-protocol-cpp/CMakeLists.txt")
    set(FETCHCONTENT_SOURCE_DIR_FLEET_PROTOCOL_CPP
        "${CMAKE_CURRENT_SOURCE_DIR}/../fleet-protocol-cpp" CACHE PATH "" FORCE)
endif()
FetchContent_Declare(fleet_protocol_cpp
    GIT_REPOSITORY https://github.com/bringauto/fleet-protocol-cpp.git
    GIT_TAG        v1.2.0
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(fleet_protocol_cpp)
# CMDEF_ADD_LIBRARY creates fleet-protocol-cxx-helpers-static; create the namespaced alias.
if(TARGET fleet-protocol-cxx-helpers-static
        AND NOT TARGET fleet-protocol-cxx-helpers-static::fleet-protocol-cxx-helpers-static)
    add_library(fleet-protocol-cxx-helpers-static::fleet-protocol-cxx-helpers-static
                ALIAS fleet-protocol-cxx-helpers-static)
endif()
set(BAFleetProtocolCpp_FOUND TRUE)
