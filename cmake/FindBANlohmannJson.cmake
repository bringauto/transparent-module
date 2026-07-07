include_guard(GLOBAL)

# Provides: nlohmann_json::nlohmann_json
# Resolution order: in-scope target → system config package → FetchContent source build.

if(TARGET nlohmann_json::nlohmann_json)
    set(BANlohmannJson_FOUND TRUE)
    return()
endif()

if(nlohmann_json_DIR MATCHES "${CMAKE_BINARY_DIR}")
    unset(nlohmann_json_DIR CACHE)
endif()
find_package(nlohmann_json 3.10.5 QUIET CONFIG)
if(nlohmann_json_FOUND)
    message(STATUS "[BA] nlohmann_json: found via system package")
    set(BANlohmannJson_FOUND TRUE)
    return()
endif()

message(STATUS "[BA] nlohmann_json: not found locally, fetching via FetchContent")
include(FetchContent)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install    ON  CACHE BOOL "" FORCE)
# OVERRIDE_FIND_PACKAGE: FetchContent_Declare's name matches the plain
# FIND_PACKAGE(nlohmann_json ...) call in CMakeLists.txt, so it intercepts it directly.
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.12.0
    GIT_SHALLOW    TRUE
    OVERRIDE_FIND_PACKAGE)
FetchContent_MakeAvailable(nlohmann_json)
set(BANlohmannJson_FOUND TRUE)
