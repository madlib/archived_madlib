# ------------------------------------------------------------------------------
# madpack source files for Greenplum
# ------------------------------------------------------------------------------

# -- 1. Copy all *.py and *.m4 files -------------------------------------------

file(GLOB_RECURSE MADPACK_FILES_GREENPLUM
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/../../postgres/madpack"
    "../../postgres/madpack/*.py"
    "../../postgres/madpack/*.m4"
)
add_files(MADPACK_TARGET_FILES_GREENPLUM "../../postgres/madpack"
    "${CMAKE_CURRENT_BINARY_DIR}" ${MADPACK_FILES_GREENPLUM})
add_custom_target(madpackFiles_Greenplum ALL DEPENDS ${MADPACK_TARGET_FILES_GREENPLUM})


# -- 2. Install madpack files to $MADLIB_ROOT/ports/greenplum/madpack ----------

install(FILES ${MADPACK_TARGET_FILES_GREENPLUM}
    DESTINATION ports/greenplum/madpack
    COMPONENT greenplum
)
