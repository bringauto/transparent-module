# CMake 4.x removed FindBoost.cmake (CMP0167) — always use CONFIG mode.
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

# Provides: Boost::<component> targets matching requested components.
# No include_guard — callers may request different components.
find_package(Boost 1.86 CONFIG QUIET COMPONENTS ${BABoost_FIND_COMPONENTS})
if(Boost_FOUND)
    set(BABoost_FOUND TRUE)
    return()
endif()

message(STATUS "[BA] Boost 1.86 config package not found, fetching via FetchContent")
include(FetchContent)

# BOOST_INCLUDE_LIBRARIES controls which Boost libraries are built.
# List all components used by the external-server variant's HTTP client stack
# (fleet-http-client-shared -> cpprestsdk) here so that the first
# FetchContent_MakeAvailable call (whichever component triggers it) builds all
# of them — subsequent calls are no-ops and cannot extend the component list.
# OVERRIDE_FIND_PACKAGE (CMake 3.24+) makes the direct FIND_PACKAGE(Boost CONFIG ...)
# call in CMakeLists.txt resolve to this FetchContent version automatically.
set(BOOST_INCLUDE_LIBRARIES
    regex date_time atomic random chrono system filesystem thread asio uuid)
set(BOOST_ENABLE_PYTHON OFF)
set(BOOST_ENABLE_MPI    OFF)
set(BUILD_SHARED_LIBS   OFF)

FetchContent_Declare(Boost
    URL https://github.com/boostorg/boost/releases/download/boost-1.86.0/boost-1.86.0-cmake.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    OVERRIDE_FIND_PACKAGE)
FetchContent_MakeAvailable(Boost)

# Boost's cmake config does NOT set Boost_LIBRARIES or Boost_INCLUDE_DIR —
# those are FindBoost.cmake (module-mode) variables. The modules' CMakeLists.txt
# explicitly link against Boost::<component> targets, but set these too in case
# any consumer still expects the module-mode variables.
set(Boost_LIBRARIES
    Boost::regex
    Boost::date_time
    Boost::atomic
    Boost::random
    Boost::chrono
    Boost::system
    Boost::filesystem
    Boost::thread
    Boost::asio
    Boost::uuid)
set(Boost_INCLUDE_DIR "")
set(BABoost_FOUND TRUE)
