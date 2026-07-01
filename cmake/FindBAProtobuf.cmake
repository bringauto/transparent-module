include_guard(GLOBAL)

# Provides: protobuf::libprotobuf
# Resolution order: in-scope target → system config package → FetchContent source build.

if(TARGET protobuf::libprotobuf)
    set(BAProtobuf_FOUND TRUE)
    return()
endif()

if(Protobuf_DIR MATCHES "${CMAKE_BINARY_DIR}")
    unset(Protobuf_DIR CACHE)
endif()
find_package(Protobuf QUIET)
if(Protobuf_FOUND)
    message(STATUS "[BA] Protobuf: found via system package")
    set(BAProtobuf_FOUND TRUE)
    return()
endif()

message(STATUS "[BA] Protobuf: system package not found, fetching via FetchContent")
include(FetchContent)
set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(protobuf_INSTALL OFF CACHE BOOL "" FORCE)
# OVERRIDE_FIND_PACKAGE: "protobuf" matches "Protobuf" case-insensitively, so
# subsequent find_package(Protobuf ...) calls in subdirectories resolve here.
FetchContent_Declare(protobuf
    GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
    GIT_TAG        v3.21.12
    GIT_SHALLOW    TRUE
    OVERRIDE_FIND_PACKAGE)
FetchContent_MakeAvailable(protobuf)
set(BAProtobuf_FOUND TRUE)
