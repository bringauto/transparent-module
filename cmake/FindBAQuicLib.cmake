INCLUDE_GUARD(GLOBAL)

# Provides: ba-quic-lib::ba-quic-lib
# Resolution order: in-scope target -> system config package -> FetchContent source build.
#
# Shared, transport-only MsQuic server wrapper that replaces this repo's hand-rolled
# operator_stream::QuicOperatorServer msquic internals.

IF(TARGET ba-quic-lib::ba-quic-lib)
    SET(BAQuicLib_FOUND TRUE)
    RETURN()
ENDIF()

STRING(FIND "${ba-quic-lib_DIR}" "${CMAKE_BINARY_DIR}" _ba_quic_lib_dir_pos)
IF(_ba_quic_lib_dir_pos EQUAL 0)
    UNSET(ba-quic-lib_DIR CACHE)
ENDIF()
UNSET(_ba_quic_lib_dir_pos)
FIND_PACKAGE(ba-quic-lib QUIET CONFIG)
IF(ba-quic-lib_FOUND)
    MESSAGE(STATUS "[BA] ba-quic-lib: found via system package")
    SET(BAQuicLib_FOUND TRUE)
    RETURN()
ENDIF()

MESSAGE(STATUS "[BA] ba-quic-lib: system package not found, fetching via FetchContent")
INCLUDE(FetchContent)
FETCHCONTENT_DECLARE(ba-quic-lib
    GIT_REPOSITORY "https://gitlab.bringauto.com/bring-auto/libraries/quic-lib.git"
    GIT_TAG        v0.1.2
    GIT_SHALLOW    TRUE
    OVERRIDE_FIND_PACKAGE)
# Shadow parent cache variables so ba-quic-lib's add_subdirectory does not inherit them:
# BRINGAUTO_TESTS would pull in its test suite; BRINGAUTO_INSTALL/PACKAGE would trigger
# install(EXPORT ba-quic-lib-targets), which fails at generate time: the exported ba-quic-lib
# target links msquic PUBLIC, but msquic is in no export set either way (system package or
# FetchContent subdir), and CMake forbids exporting a target whose public dependency isn't
# exported too.
SET(BRINGAUTO_TESTS OFF)
SET(BRINGAUTO_INSTALL OFF)
SET(BRINGAUTO_PACKAGE OFF)
FETCHCONTENT_MAKEAVAILABLE(ba-quic-lib)
UNSET(BRINGAUTO_TESTS)
UNSET(BRINGAUTO_INSTALL)
UNSET(BRINGAUTO_PACKAGE)
SET(BAQuicLib_FOUND TRUE)
