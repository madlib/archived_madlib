# ------------------------------------------------------------------------------
# MADlib Documentation Configuration Files
# ------------------------------------------------------------------------------

configure_file(user.doxyfile.in user.doxyfile)
configure_file(developer.doxyfile.in developer.doxyfile)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/doxygen.css
    COMMAND ${DOXYGEN_EXECUTABLE} -w html /dev/null /dev/null doxygen.css
        developer.doxyfile
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../patch/doxygen.css.sh
    DEPENDS developer.doxyfile.in
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMENT "Patching doxygen style sheet..."
)

add_custom_target(doxygen_css DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/doxygen.css)
