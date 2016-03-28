# ------------------------------------------------------------------------------
# MADlib configuration files (these are configuration files used by the
# installer). End users are not supposed to make modifications.
# ------------------------------------------------------------------------------


# -- 1. Copy Modules.yml files ---------------------------------------------------

add_files(CONFIG_TARGET_FILES_HAWQ_2_0 . "${CMAKE_CURRENT_BINARY_DIR}" "Modules.yml")
add_custom_target(configFilesHAWQ20 ALL DEPENDS ${CONFIG_TARGET_FILES_HAWQ_2_0})



# -- 3. Install config files to $MADLIB_ROOT/config ----------------------------

install(FILES ${CONFIG_TARGET_FILES_HAWQ_2_0}
    DESTINATION ports/hawq/2.0/config
    COMPONENT hawq_2_0
)
