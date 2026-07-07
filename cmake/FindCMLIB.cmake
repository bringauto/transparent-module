# Compatibility shim replacing cmlib for BringAuto deps fetched via FetchContent.
# Satisfies: FIND_PACKAGE(CMLIB COMPONENTS CMDEF CMUTIL STORAGE REQUIRED)
# Provides: CMDEF_ADD_LIBRARY, CMDEF_ADD_EXECUTABLE, CMDEF_COMPILE_DEFINITIONS,
#           CMDEF_INSTALL, CMDEF_PACKAGE, BA_PACKAGE_LIBRARY, BA_PACKAGE_DEPS_IMPORTED,
#           CMCONF_INIT_SYSTEM

set(CMLIB_FOUND TRUE)
foreach(_comp ${CMLIB_FIND_COMPONENTS})
    set(CMLIB_${_comp}_FOUND TRUE)
endforeach()

# Variable referenced in RPATH settings by BringAuto CMakeLists files.
set(CMDEF_LIBRARY_INSTALL_DIR "lib")

if(NOT COMMAND CMDEF_ADD_LIBRARY)
    macro(CMDEF_ADD_LIBRARY)
        cmake_parse_arguments(_cmdef_al ""
            "LIBRARY_GROUP;TYPE;VERSION;SOURCE_BASE_DIRECTORY"
            "SOURCES;INCLUDE_DIRECTORIES;INSTALL_INCLUDE_DIRECTORIES;COMPILE_DEFINITIONS;LINK_LIBRARIES"
            ${ARGN})
        string(TOLOWER "${_cmdef_al_TYPE}" _cmdef_type_lower)
        set(_cmdef_target "${_cmdef_al_LIBRARY_GROUP}-${_cmdef_type_lower}")
        # cmake forbids compiled source files on INTERFACE libraries. When TYPE is
        # INTERFACE but SOURCES are provided (e.g. fleet-protocol protobuf target),
        # create a STATIC library under the same name so sources are compiled.
        if("${_cmdef_al_TYPE}" STREQUAL "INTERFACE" AND _cmdef_al_SOURCES)
            add_library(${_cmdef_target} STATIC ${_cmdef_al_SOURCES})
            set(_cmdef_al_prop_scope PUBLIC)
        elseif("${_cmdef_al_TYPE}" STREQUAL "INTERFACE")
            add_library(${_cmdef_target} INTERFACE)
            set(_cmdef_al_prop_scope INTERFACE)
        else()
            add_library(${_cmdef_target} ${_cmdef_al_TYPE} ${_cmdef_al_SOURCES})
            set(_cmdef_al_prop_scope PUBLIC)
        endif()
        if(_cmdef_al_INCLUDE_DIRECTORIES)
            target_include_directories(${_cmdef_target} ${_cmdef_al_prop_scope} ${_cmdef_al_INCLUDE_DIRECTORIES})
        endif()
        if(_cmdef_al_COMPILE_DEFINITIONS)
            target_compile_definitions(${_cmdef_target} ${_cmdef_al_prop_scope} ${_cmdef_al_COMPILE_DEFINITIONS})
        endif()
        if(_cmdef_al_LINK_LIBRARIES)
            target_link_libraries(${_cmdef_target} ${_cmdef_al_prop_scope} ${_cmdef_al_LINK_LIBRARIES})
        endif()
    endmacro()
endif()

if(NOT COMMAND CMDEF_ADD_EXECUTABLE)
    macro(CMDEF_ADD_EXECUTABLE)
        cmake_parse_arguments(_cmdef_ae "" "TARGET;VERSION" "SOURCES" ${ARGN})
        add_executable(${_cmdef_ae_TARGET} ${_cmdef_ae_SOURCES})
    endmacro()
endif()

if(NOT COMMAND CMDEF_COMPILE_DEFINITIONS)
    # First arg is scope (ALL/PUBLIC/PRIVATE), rest are definitions.
    macro(CMDEF_COMPILE_DEFINITIONS _cmdef_scope)
        foreach(_cmdef_def ${ARGN})
            add_compile_definitions(${_cmdef_def})
        endforeach()
    endmacro()
endif()

if(NOT COMMAND CMDEF_INSTALL)
    # Unlike fetched third-party deps (which get their own manual install() rules
    # in cmake/FindBA*.cmake), the module's own gateway/external-server .so is
    # ONLY installed through this macro — a no-op here would make
    # -DBRINGAUTO_INSTALL=ON silently produce an empty install tree for the
    # actual deliverable. Do a real install (harmless no-op on INTERFACE
    # libraries, which have no artifacts to copy).
    macro(CMDEF_INSTALL)
        cmake_parse_arguments(_cmdef_inst "" "TARGET;NAMESPACE" "" ${ARGN})
        if(_cmdef_inst_TARGET AND TARGET ${_cmdef_inst_TARGET})
            install(TARGETS ${_cmdef_inst_TARGET}
                RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
                LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
                ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
        endif()
        unset(_cmdef_inst_TARGET)
        unset(_cmdef_inst_NAMESPACE)
    endmacro()
endif()

if(NOT COMMAND CMDEF_PACKAGE)
    macro(CMDEF_PACKAGE)
        cmake_parse_arguments(_cmdef_pkg "" "MAIN_TARGET;VERSION" "" ${ARGN})
        if(_cmdef_pkg_VERSION)
            set(CPACK_PACKAGE_VERSION "${_cmdef_pkg_VERSION}")
        endif()
    endmacro()
endif()

if(NOT COMMAND BA_PACKAGE_LIBRARY)
    macro(BA_PACKAGE_LIBRARY)
        # no-op — parent project provides all packages via FindBA*.cmake
    endmacro()
endif()

if(NOT COMMAND BA_PACKAGE_DEPS_IMPORTED)
    macro(BA_PACKAGE_DEPS_IMPORTED)
        # no-op
    endmacro()
endif()

if(NOT COMMAND CMCONF_INIT_SYSTEM)
    macro(CMCONF_INIT_SYSTEM)
        # no-op
    endmacro()
endif()
