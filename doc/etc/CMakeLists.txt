# ------------------------------------------------------------------------------
# MADlib Documentation Configuration Files
# ------------------------------------------------------------------------------

set(MADLIB_DOXYGEN_SOURCE_DIR "\"${CMAKE_SOURCE_DIR}/src\" \"${CMAKE_SOURCE_DIR}/methods\"")
set(USE_MATHJAX NO CACHE BOOL "Render LaTeX formulas using MathJax")
set(MATHJAX_RELPATH "third_party/src/mathjax" CACHE PATH
    "Path to MathJax installation")

configure_file(user.doxyfile.in user.doxyfile)
configure_file(developer.doxyfile.in developer.doxyfile)
