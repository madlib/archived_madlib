# ------------------------------------------------------------------------------
# MADlib binary files
# ------------------------------------------------------------------------------


# -- 1. Copy files -------------------------------------------------------------

set(MADPACK_BINARIES
    madpack
)
add_files(BINARY_TARGET_FILES . "${CMAKE_CURRENT_BINARY_DIR}" ${MADPACK_BINARIES})
add_custom_target(binaryFiles ALL DEPENDS ${BINARY_TARGET_FILES})
add_dependencies(binaryFiles madpackFiles)


# -- 2. Install binary files to $MADLIB_ROOT/bin -------------------------------

install(PROGRAMS ${BINARY_TARGET_FILES}
    DESTINATION bin
    COMPONENT core
)
