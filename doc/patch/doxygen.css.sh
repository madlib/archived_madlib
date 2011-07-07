#!/bin/sh
#
# By default, doxygen.css contains
#
#     pre.fragment {
#         word-wrap: break-word;
#     }
#
# which makes text in <pre> or \verbatim environments wrap at white spaces.
# However, long lines are usually intended (e.g., psql output), so we want to
# override this with "word-warp: normal"

cat - >> doxygen.css <<'EOF'

/* -- MADlib patches -------------------------------------------------------- */

/* Indent paragraphs in the main text, but not in framed boxes */
div.contents > p, div.contents > pre, div.contents > ul, div.contents > div.fragment, dd {  
    margin-left: 20px;
}

/* Increase spacing between titled paragraphs in the main text, but not in 
   framed boxes */
div.contents > dl {
    margin-top: 2em;
}

/* Increase spacing between list items in the main text */
div.contents li {
    margin-top: 1em;
}

/* No automtic line wrapping at white spaces in <pre> or \verbatim
   environments. */
pre.fragment {
    word-wrap: normal;
}

/* No padding for paragraph headers (in its infinite wisdom, doxygen uses <dl>
   environments for that) */
dl {
    padding-left: 0;
}

/* Increase the font size for paragraph headers */
dt {
    font-size: 120%;
    margin-bottom: 1em;
}

/* The first column should align with normal text. So we cannot use
   border-spacing. */
table.params {
    border-spacing: 0;
}

/* Add some padding instead of border-spacing */
td.paramname {
    padding: 1px 1em 1px 0;
}

/* We move the bar a out of the text frame, so that the text aligns well with
   the rest. Note: margin-left + border-width-left + padding-left = 0 */
dl.note, dl.warning, dl.attention, dl.pre, dl.post, dl.invariant, dl.deprecated, dl.todo, dl.test, dl.bug
{
    margin-left: -6px;
    padding-left: 2px;
}

EOF
