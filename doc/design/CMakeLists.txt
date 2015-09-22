# ------------------------------------------------------------------------------
# MADlib Design Document
# ------------------------------------------------------------------------------

find_program(
    LATEX_BINARY
    latex
    DOC "Path to latex"
)
find_program(
    BIBTEX_BINARY
    latex
    DOC "Path to bibtex"
)
find_program(
    MAKEINDEX_BINARY
    makeindex
    DOC "Path to makeindex"
)

file(GLOB_RECURSE DESIGN_DOC_MODULES
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/latex/*.sty"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/*.tex"
    "${CMAKE_CURRENT_SOURCE_DIR}/other-chapters/*.tex"
    "${CMAKE_CURRENT_SOURCE_DIR}/figures/*")

if(LATEX_BINARY AND BIBTEX_BINARY AND MAKEINDEX_BINARY)
    include(UseLATEX)

    add_latex_document(design.tex
        BIBFILES ../literature.bib
        INPUTS
            ${DESIGN_DOC_MODULES}
        EXCLUDE_FROM_ALL
    )
else()
    message(STATUS "A complete LaTeX installation could not be found. "
        "Compiling the design document will not be possible.")
endif(LATEX_BINARY AND BIBTEX_BINARY AND MAKEINDEX_BINARY)
