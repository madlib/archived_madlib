# ------------------------------------------------------------------------------
# MADlib binary files
# ------------------------------------------------------------------------------


# -- 1. Create a symlink madpack -> ../madpack/madpack.py ----------------------

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


# -- 2. Install binary files to $MADLIB_ROOT/bin -------------------------------

install(FILES ${BINARY_TARGET_FILES}
    DESTINATION bin
    COMPONENT core
)
