#!/bin/sh

# Unfortunately, utils/builtin.h from GP < 4.1 contains identifiers that are
# C++ keywords. Since this file changed frequently even within major builds
# we do a simple find & substitute. Essentially, we replace
# "const char *namespace," by "const char *qualifier,".

sed 's/\(const[[:space:]]\{1,\}char[[:space:]]*\*[[:space:]]*\)namespace[[:space:]]*,/\1qualifier,/g' \
    server/utils/builtins.orig.h > \
    server/utils/builtins.h
