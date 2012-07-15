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

if(LATEX_BINARY AND BIBTEX_BINARY AND MAKEINDEX_BINARY)
    include(UseLATEX)

    add_latex_document(design.tex
        BIBFILES ../literature.bib
        INPUTS
            modules/sampling.tex
            modules/k-means.tex
            modules/low-rank-matrix-decomposition.tex
        DEFAULT_PDF
        MANGLE_TARGET_NAMES
    )
else()
    message(STATUS "A complete LaTeX installation could not be found. "
        "Compiling the design document will not be possible.")
endif(LATEX_BINARY AND BIBTEX_BINARY AND MAKEINDEX_BINARY)
