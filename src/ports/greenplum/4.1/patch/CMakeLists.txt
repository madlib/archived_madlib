# ------------------------------------------------------------------------------
# Greenplum Patch Scripts
# ------------------------------------------------------------------------------

configure_file(Versions_4_1_0_to_4_1_1.sh.in Versions_4_1_0_to_4_1_1.sh @ONLY)

# Version 4.1 of Greenplum has a bug in one of its header files that
# needs to be fixed when building with C++.
set(VERSION "${${DBMS_UC}_VERSION_MAJOR}.${${DBMS_UC}_VERSION_MINOR}.${${DBMS_UC}_VERSION_PATCH}")
if(${GREENPLUM_4_1_VERSION_PATCH} EQUAL 0 OR ${GREENPLUM_4_1_VERSION_PATCH} EQUAL 1)
    message(STATUS "Installed Greenplum version has bug in header file. "
        "Will apply patch for versions 4.1.0 to 4.1.1.")
    
    add_custom_command(
        OUTPUT
            "${CMAKE_CURRENT_BINARY_DIR}/server/utils/syncbitvector.h"
        COMMAND
            "${CMAKE_CURRENT_BINARY_DIR}/Versions_4_1_0_to_4_1_1.sh"
        DEPENDS
            "${GREENPLUM_4_1_SERVER_INCLUDE_DIR}/utils/syncbitvector.h"
            "${CMAKE_CURRENT_BINARY_DIR}/Versions_4_1_0_to_4_1_1.sh"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        COMMENT "Patching syncbitvector.h: Add missing key word \"typedef\"."
    )
    
    add_custom_target(patches_greenplum_4_1
        ALL DEPENDS
        "${CMAKE_CURRENT_BINARY_DIR}/server/utils/syncbitvector.h"
    )

    add_dependencies(madlib_greenplum_4_1 patches_greenplum_4_1)
endif(${GREENPLUM_4_1_VERSION_PATCH} EQUAL 0 OR ${GREENPLUM_4_1_VERSION_PATCH} EQUAL 1)
