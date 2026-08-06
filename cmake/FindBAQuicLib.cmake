include_guard(GLOBAL)

# Provides: ba-quic-lib::ba-quic-lib
# Resolution order: in-scope target -> system config package -> FetchContent source build.
#
# Shared, transport-only MsQuic server wrapper that replaces this repo's hand-rolled
# operator_stream::QuicOperatorServer msquic internals.

if(TARGET ba-quic-lib::ba-quic-lib)
    set(BAQuicLib_FOUND TRUE)
    return()
endif()

string(FIND "${ba-quic-lib_DIR}" "${CMAKE_BINARY_DIR}" _ba_quic_lib_dir_pos)
if(_ba_quic_lib_dir_pos EQUAL 0)
    unset(ba-quic-lib_DIR CACHE)
endif()
unset(_ba_quic_lib_dir_pos)
find_package(ba-quic-lib QUIET CONFIG)
if(ba-quic-lib_FOUND)
    message(STATUS "[BA] ba-quic-lib: found via system package")
    set(BAQuicLib_FOUND TRUE)
    return()
endif()

message(STATUS "[BA] ba-quic-lib: system package not found, fetching via FetchContent")
include(FetchContent)
set(_token "$ENV{BA_GITLAB_TOKEN_URI}")
if(_token)
    # Credential stays in this process's environment (inherited by the FetchContent clone
    # subprocess) rather than being copied to disk -- a `git config --global` rewrite would
    # otherwise persist the token in ~/.gitconfig keyed by its own value, so a rotated token
    # appends a second section instead of replacing the first, and git keeps resolving to
    # whichever token was written there first. Requires git >= 2.31.
    set(ENV{GIT_CONFIG_COUNT} 1)
    set(ENV{GIT_CONFIG_KEY_0}   "url.https://${_token}gitlab.bringauto.com/.insteadOf")
    set(ENV{GIT_CONFIG_VALUE_0} "https://gitlab.bringauto.com/")
endif()
unset(_token)
FetchContent_Declare(ba-quic-lib
    GIT_REPOSITORY "https://gitlab.bringauto.com/bring-auto/libraries/quic-lib.git"
    GIT_TAG        v0.1.2
    GIT_SHALLOW    TRUE
    OVERRIDE_FIND_PACKAGE)
# ba-quic-lib's own CMakeLists.txt declares `option(BRINGAUTO_TESTS ...)` and
# `option(BRINGAUTO_INSTALL ...)` with the same names as this repo's. Since FetchContent pulls it in
# via add_subdirectory, an already-cached value from *this* repo would otherwise leak into it:
#  - BRINGAUTO_TESTS=ON would also build ba-quic-lib's own test suite (gtest, test certs, etc.).
#  - BRINGAUTO_INSTALL=ON (set by this repo's CMDEF packaging) would run ba-quic-lib's
#    install(EXPORT ba-quic-lib-targets), which fails at generate time: the exported ba-quic-lib
#    target links msquic PUBLIC, but msquic -- resolved by ba-quic-lib's own FindBAMsquic.cmake as
#    an imported prebuilt/system package, or (last resort) a plain FetchContent subdir target -- is
#    in no export set either way, and CMake forbids exporting a target whose public dependency
#    isn't exported too. We link ba-quic-lib statically in-tree and never consume its
#    install/export, so force both OFF.
# Shadow them with plain (non-cache) variables for the duration of this add_subdirectory only --
# CMake resolves the nearest-scope normal variable before falling back to the cache entry, and a
# child directory inherits the parent's normal-variable values at the point it's added.
set(_ba_quic_lib_saved_tests_flag "${BRINGAUTO_TESTS}")
set(_ba_quic_lib_saved_install_flag "${BRINGAUTO_INSTALL}")
set(BRINGAUTO_TESTS OFF)
set(BRINGAUTO_INSTALL OFF)
FetchContent_MakeAvailable(ba-quic-lib)
unset(ENV{GIT_CONFIG_COUNT})
unset(ENV{GIT_CONFIG_KEY_0})
unset(ENV{GIT_CONFIG_VALUE_0})
set(BRINGAUTO_TESTS "${_ba_quic_lib_saved_tests_flag}")
set(BRINGAUTO_INSTALL "${_ba_quic_lib_saved_install_flag}")
unset(_ba_quic_lib_saved_tests_flag)
unset(_ba_quic_lib_saved_install_flag)
set(BAQuicLib_FOUND TRUE)
