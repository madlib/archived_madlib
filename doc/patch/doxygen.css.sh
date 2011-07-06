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

/* No automtic line wrapping at white spaces in <pre> or \verbatim
   environments. */

p,pre {  
    padding-left: 1.8em;  
}  
    
li {
    margin-bottom: 1em;
}

pre.fragment {
    word-wrap: normal;
}

dl {
    padding-left: 0;
}

dt {
    font-size: 120%;
    margin-bottom: 1em;
    margin-top: 2em;
}

dd {
    margin-left: 0;
}

table.params {
    border-spacing: 0;
}

td.paramname {
    padding: 1px 1em 1px 0;
}

EOF
