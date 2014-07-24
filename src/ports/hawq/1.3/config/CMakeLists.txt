# ------------------------------------------------------------------------------
# MADlib configuration files (these are configuration files used by the
# installer). End users are not supposed to make modifications.
# ------------------------------------------------------------------------------


# -- 1. Copy Modules.yml files ---------------------------------------------------

add_files(CONFIG_TARGET_FILES_HAWQ_1_3 . "${CMAKE_CURRENT_BINARY_DIR}" "Modules.yml")
add_custom_target(configFilesHAWQ13 ALL DEPENDS ${CONFIG_TARGET_FILES_HAWQ_1_3})



# -- 3. Install config files to $MADLIB_ROOT/config ----------------------------

install(FILES ${CONFIG_TARGET_FILES_HAWQ_1_3}
    DESTINATION ports/hawq/1.3/config
    COMPONENT hawq_1_3
)
