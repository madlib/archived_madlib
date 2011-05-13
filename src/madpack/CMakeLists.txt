file(GLOB_RECURSE MADPACK_FILES
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "*.py"
)

# FIXME: Refactor and put into its own function (the same code is used several
# times in our build scripts)
# Directory we will use for SQL files
foreach(CURRENT_FILE ${MADPACK_FILES})
    get_filename_component(CURRENT_PATH "${CURRENT_FILE}" ABSOLUTE)

    set(OUTFILE "${CMAKE_CURRENT_BINARY_DIR}/${CURRENT_FILE}")
    add_custom_command(OUTPUT "${OUTFILE}"
        COMMAND ${CMAKE_COMMAND} -E copy "${CURRENT_PATH}" "${OUTFILE}"
        DEPENDS "${CURRENT_PATH}"
        COMMENT "Copying ${CURRENT_FILE}.")
    
    list(APPEND MADPACK_TARGET_FILES
        ${OUTFILE})
endforeach(CURRENT_FILE)
add_custom_target(madpackFiles ALL DEPENDS ${MADPACK_TARGET_FILES})

install(PROGRAMS madpack.py
    DESTINATION madpack
)
install(FILES ${MADPACK_TARGET_FILES}
    DESTINATION madpack
)
#    REGEX "^(.*/)?\\.DS_Store\$" EXCLUDE
#    REGEX "^(.*/)?madpack\\.py\$" PERMISSIONS
#        OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ
#        WORLD_EXECUTE WORLD_READ)
