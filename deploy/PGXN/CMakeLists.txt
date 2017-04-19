# ------------------------------------------------------------------------------
# Packaging for the PostgreSQL Extension Network (PGXN), http://pgxn.org
# ------------------------------------------------------------------------------

set(MADLIB_PGXN_RELEASE_NUMBER 1)
# set(MADLIB_PGXN_VERSION_STR
#    "${MADLIB_VERSION_MAJOR}.${MADLIB_VERSION_MINOR}.${MADLIB_VERSION_PATCH}release${MADLIB_PGXN_RELEASE_NUMBER}
set(MADLIB_PGXN_VERSION_STR
    "${MADLIB_VERSION_MAJOR}.${MADLIB_VERSION_MINOR}.${MADLIB_VERSION_PATCH}")
set(MADLIB_PGXN_NAME "madlib-pgxn-${MADLIB_PGXN_VERSION_STR}")

configure_file(META.json.in META.json)
configure_file(generate_package.sh.in generate_package.sh @ONLY)
configure_file(zipignore.in zipignore)
configure_file(ReadMe.txt ReadMe.txt COPYONLY)
add_custom_command(
    OUTPUT madlib.zip
    COMMAND "${CMAKE_COMMAND}" -E create_symlink
        "${CMAKE_CURRENT_BINARY_DIR}/META.json"
        "${CMAKE_SOURCE_DIR}/META.json"
    COMMAND "${CMAKE_CURRENT_BINARY_DIR}/generate_package.sh"
    COMMAND "${CMAKE_COMMAND}" -E remove
        "${CMAKE_SOURCE_DIR}/META.json"
    COMMENT "Creating PGXN zip file."
    VERBATIM
)
add_custom_target(pgxn DEPENDS madlib.zip)
