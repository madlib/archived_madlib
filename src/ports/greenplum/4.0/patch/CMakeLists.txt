# ------------------------------------------------------------------------------
# Greenplum Patch Scripts
# ------------------------------------------------------------------------------

configure_file(Versions_4_0_and_below.sh.in Versions_4_0_and_below.sh @ONLY)


# Version 4.0 of Greenplum and below ship with a utils/builtin.h that has
# C++ keywords as identifiers. We need to patch this.
message(STATUS "Greenplum versions before 4.1 have header files that "
    "need a patch for C++ compatibility. Will apply patch for versions "
    "below 4.1.")

add_custom_command(
    OUTPUT
        "${CMAKE_CURRENT_BINARY_DIR}/server/utils/builtins.h"
        "${CMAKE_CURRENT_BINARY_DIR}/server/executor/executor.h"
    COMMAND 
        "${CMAKE_CURRENT_BINARY_DIR}/Versions_4_0_and_below.sh"
    DEPENDS
        "${GREENPLUM_4_0_SERVER_INCLUDE_DIR}/utils/builtins.h"
        "${GREENPLUM_4_0_SERVER_INCLUDE_DIR}/executor/executor.h"
        "${CMAKE_CURRENT_BINARY_DIR}/Versions_4_0_and_below.sh"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMENT "Patching builtins.h, executor.h: Renaming identifiers that are C++ keywords."
)

add_custom_target(patches_greenplum_4_0
    ALL DEPENDS
    "${CMAKE_CURRENT_BINARY_DIR}/server/utils/builtins.h"
    "${CMAKE_CURRENT_BINARY_DIR}/server/executor/executor.h"
)

add_dependencies(madlib_greenplum_4_0 patches_greenplum_4_0)
