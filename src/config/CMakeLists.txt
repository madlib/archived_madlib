file(GLOB_RECURSE CONFIG_FILES
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "*.yml"
)

# FIXME: Refactor and put into its own function (the same code is used several
# times in our build scripts)
# Directory we will use for SQL files
foreach(CURRENT_FILE ${CONFIG_FILES})
    get_filename_component(CURRENT_PATH "${CURRENT_FILE}" ABSOLUTE)

    set(OUTFILE "${CMAKE_CURRENT_BINARY_DIR}/${CURRENT_FILE}")
    add_custom_command(OUTPUT "${OUTFILE}"
        COMMAND ${CMAKE_COMMAND} -E copy "${CURRENT_PATH}" "${OUTFILE}"
        DEPENDS "${CURRENT_PATH}"
        COMMENT "Copying ${CURRENT_FILE}.")
    
    list(APPEND CONFIG_TARGET_FILES
        ${OUTFILE})
endforeach(CURRENT_FILE)
add_custom_target(configFiles ALL DEPENDS ${CONFIG_TARGET_FILES})

install(FILES ${CONFIG_TARGET_FILES}
    DESTINATION config
)
