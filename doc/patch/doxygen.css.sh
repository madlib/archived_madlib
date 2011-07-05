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

pre.fragment {
    word-wrap: normal;
}

EOF
