# ------------------------------------------------------------------------------
# MADlib configuration files (these are configuration files used by the
# installer). End users are not supposed to make modifications.
# ------------------------------------------------------------------------------


# -- 1. Copy all *.yml files ---------------------------------------------------

file(GLOB_RECURSE CONFIG_FILES
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "*.yml"
)
add_files(CONFIG_TARGET_FILES . "${CMAKE_CURRENT_BINARY_DIR}" ${CONFIG_FILES})
add_custom_target(configFiles_greenplum_4_0 ALL DEPENDS ${CONFIG_TARGET_FILES})



# -- 2. Install config files to $MADLIB_ROOT/config ----------------------------

install(FILES ${CONFIG_TARGET_FILES}
    DESTINATION ports/${PORT_DIR_NAME}/4.0/config
    COMPONENT greenplum_4_0
)
