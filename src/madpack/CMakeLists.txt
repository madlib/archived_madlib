# ------------------------------------------------------------------------------
# madpack source files
# ------------------------------------------------------------------------------


# -- 1. Copy all *.py files ----------------------------------------------------

file(GLOB_RECURSE MADPACK_FILES
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "*.py"
    "*.m4"
)
add_files(MADPACK_TARGET_FILES . "${CMAKE_CURRENT_BINARY_DIR}" ${MADPACK_FILES})
add_custom_target(madpackFiles ALL DEPENDS ${MADPACK_TARGET_FILES})


# -- 2. Install madpack files to $MADLIB_ROOT/madpack, set executable flagse
#       for madpack.py ---------------------------------------------------------

install(FILES ${MADPACK_TARGET_FILES}
    DESTINATION madpack
)
install(PROGRAMS madpack.py
    DESTINATION madpack
)
