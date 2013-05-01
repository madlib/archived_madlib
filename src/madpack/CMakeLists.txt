# ------------------------------------------------------------------------------
# madpack source files
# ------------------------------------------------------------------------------

# -- 1. Copy all *.py files ----------------------------------------------------

file(GLOB_RECURSE MADPACK_FILES
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "*.py"
    "*.m4"
    "changelist*.yaml"
)
add_files(MADPACK_TARGET_FILES . "${CMAKE_CURRENT_BINARY_DIR}" ${MADPACK_FILES})
add_custom_target(madpackFiles ALL DEPENDS ${MADPACK_TARGET_FILES})


# -- 2. Install madpack files to $MADLIB_ROOT/madpack, set executable flags
#       for madpack.py ---------------------------------------------------------

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DESTINATION .
    USE_SOURCE_PERMISSIONS
    COMPONENT core
    FILES_MATCHING
    PATTERN "*.py"
    PATTERN "changelist*.yaml"
    PATTERN ".DS_Store" EXCLUDE
)

