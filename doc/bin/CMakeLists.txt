# ------------------------------------------------------------------------------
# MADlib Documentation Executable Files
# ------------------------------------------------------------------------------

set(BIN_FILES
    doxypy.py
)
add_files(BIN_TARGET_FILES . "${CMAKE_CURRENT_BINARY_DIR}" ${BIN_FILES})
add_custom_target(doxyBinFiles ALL DEPENDS ${BIN_TARGET_FILES})

list(APPEND M4_ARGUMENTS
    "\"-DMADLIB_SCHEMA=MADlib\""
    "\"-I${CMAKE_BINARY_DIR}/tmp\""
)

join_strings(_M4_ARGUMENTS " " "${M4_ARGUMENTS}")
configure_file(py_filter.sh.in py_filter.sh @ONLY)

execute_process(
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/tmp"
    COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_BINARY_DIR}/tmp/SQLCommon.m4"
)
configure_file(sql_filter.sh.in sql_filter.sh @ONLY)

get_dir_name(_MATHJAX_PARENT_DIR "${MATHJAX_RELPATH}")
execute_process(
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_MATHJAX_PARENT_DIR}"
)
configure_file(update_mathjax.sh.in update_mathjax.sh @ONLY)
