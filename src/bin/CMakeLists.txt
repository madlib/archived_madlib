# Create a symlink from bin/madpack into Python directory
set(MADPACK_EXE madpack)

add_custom_command(
    OUTPUT ${MADPACK_EXE}
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        ../madpack/madpack.py ${MADPACK_EXE}
    COMMENT "Creating symbolic link for bin/madpack.")
list(APPEND BINARY_TARGET_FILES
    "${CMAKE_CURRENT_BINARY_DIR}/${MADPACK_EXE}"
)

add_custom_target(binaryFiles ALL DEPENDS ${BINARY_TARGET_FILES})
add_dependencies(binaryFiles madpackFiles)

install(FILES ${BINARY_TARGET_FILES}
    DESTINATION bin
)
