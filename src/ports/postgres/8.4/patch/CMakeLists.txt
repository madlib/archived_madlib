# ------------------------------------------------------------------------------
# PostgreSQL Patch Scripts
# ------------------------------------------------------------------------------

configure_file(Versions_8_4_and_below.sh.in Versions_8_4_and_below.sh @ONLY)


# Version 8.4 of PostgreSQL and below ship with server/nodes/parsenodes.h
# and server/nodes/primnodes.h that have C++ keywords as identifiers.
# We need to patch this.
message(STATUS "PostgreSQL versions before 9.0 have header files that "
    "need a patch for C++ compatibility. Will apply patch for versions "
    "below 9.0.")

add_custom_command(
    OUTPUT
        "${CMAKE_CURRENT_BINARY_DIR}/server/nodes/parsenodes.h"
        "${CMAKE_CURRENT_BINARY_DIR}/server/nodes/primnodes.h"
        "${CMAKE_CURRENT_BINARY_DIR}/server/utils/builtins.h"
    COMMAND
        "${CMAKE_CURRENT_BINARY_DIR}/Versions_8_4_and_below.sh"
    DEPENDS
        "${POSTGRESQL_8_4_SERVER_INCLUDE_DIR}/nodes/parsenodes.h"
        "${POSTGRESQL_8_4_SERVER_INCLUDE_DIR}/nodes/primnodes.h"
        "${POSTGRESQL_8_4_SERVER_INCLUDE_DIR}/utils/builtins.h"
        "${CMAKE_CURRENT_BINARY_DIR}/Versions_8_4_and_below.sh"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMENT "Patching parsenodes.h, primnodes.h, builtins.h, memnodes.h: Renaming identifiers that are C++ keywords."
)

add_custom_target(patches_postgresql_8_4
    ALL DEPENDS
    "${CMAKE_CURRENT_BINARY_DIR}/server/nodes/parsenodes.h"
    "${CMAKE_CURRENT_BINARY_DIR}/server/nodes/primnodes.h"
    "${CMAKE_CURRENT_BINARY_DIR}/server/utils/builtins.h"
)

add_dependencies(madlib_postgresql_8_4 patches_postgresql_8_4)
