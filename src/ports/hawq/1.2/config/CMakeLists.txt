# ------------------------------------------------------------------------------
# MADlib configuration files (these are configuration files used by the
# installer). End users are not supposed to make modifications.
# ------------------------------------------------------------------------------


# -- 1. Copy Modules.yml files ---------------------------------------------------

add_files(CONFIG_TARGET_FILES_HAWQ_1_2 . "${CMAKE_CURRENT_BINARY_DIR}" "Modules.yml")
add_custom_target(configFilesHAWQ11 ALL DEPENDS ${CONFIG_TARGET_FILES_HAWQ_1_2})



# -- 2. Install config files to $MADLIB_ROOT/config ----------------------------

install(FILES ${CONFIG_TARGET_FILES_HAWQ_1_2}
    DESTINATION config
    COMPONENT core
)
