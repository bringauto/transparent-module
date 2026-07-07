include_guard(GLOBAL)

# Provides: fleet-http-client-shared::fleet-http-client-shared
# Resolution order: in-scope target → system config package → FetchContent source build.
# Repo is named "fleet-http-client" but (like the other BringAuto cmlib packages)
# exports a CMake target "fleet-http-client-shared" via CMDEF_ADD_LIBRARY
# (LIBRARY_GROUP "fleet-http-client" + TYPE SHARED). It uses cmlib internally,
# so FindCMLIB.cmake must be on CMAKE_MODULE_PATH before this fetch, and it needs
# cpprestsdk/Boost/ZLIB findable already — declare those first in Dependencies.cmake.

if(TARGET fleet-http-client-shared::fleet-http-client-shared)
    set(BAFleetHttpClient_FOUND TRUE)
    return()
endif()

if(fleet-http-client-shared_DIR MATCHES "${CMAKE_BINARY_DIR}")
    unset(fleet-http-client-shared_DIR CACHE)
endif()
find_package(fleet-http-client-shared QUIET CONFIG)
if(fleet-http-client-shared_FOUND)
    message(STATUS "[BA] fleet-http-client: found via config package")
    set(BAFleetHttpClient_FOUND TRUE)
    return()
endif()

message(STATUS "[BA] fleet-http-client: not found locally, fetching via FetchContent")
include(FetchContent)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../fleet-http-client/CMakeLists.txt")
    set(FETCHCONTENT_SOURCE_DIR_FLEET_HTTP_CLIENT_SHARED
        "${CMAKE_CURRENT_SOURCE_DIR}/../fleet-http-client" CACHE PATH "" FORCE)
endif()
FetchContent_Declare(fleet_http_client_shared
    GIT_REPOSITORY https://github.com/bringauto/fleet-http-client.git
    GIT_TAG        v2.0.2
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(fleet_http_client_shared)
# CMDEF_ADD_LIBRARY creates the unnamespaced fleet-http-client-shared target.
# The module's own CMakeLists.txt later calls TARGET_LINK_LIBRARIES a second
# time on the *namespaced* target to bolt on Boost/ZLIB (its "obsolete
# cpprestsdk cmake export package" workaround) — an ALIAS can't receive that
# (CMake forbids target_link_libraries on ALIAS targets), so forward via an
# IMPORTED INTERFACE library instead, which can.
if(TARGET fleet-http-client-shared
        AND NOT TARGET fleet-http-client-shared::fleet-http-client-shared)
    add_library(fleet-http-client-shared::fleet-http-client-shared INTERFACE IMPORTED)
    target_link_libraries(fleet-http-client-shared::fleet-http-client-shared
        INTERFACE fleet-http-client-shared)
endif()

# Install the shared library so consumers can find it via RPATH at runtime.
# CMDEF_INSTALL() is a no-op in our shim, so register the install rule here.
if(TARGET fleet-http-client-shared)
    get_target_property(_fhc_ttype fleet-http-client-shared TYPE)
    if(_fhc_ttype STREQUAL "SHARED_LIBRARY")
        install(TARGETS fleet-http-client-shared
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
    endif()
    unset(_fhc_ttype)
endif()

set(BAFleetHttpClient_FOUND TRUE)
