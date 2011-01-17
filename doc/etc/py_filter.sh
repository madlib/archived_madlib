#!/bin/sh
MADLIB_SCHEMA="MADlib"
FILTER="doc/bin/doxypy.py /dev/stdin"

m4 -DMADLIB_SCHEMA="${MADLIB_SCHEMA}" $1 | $FILTER
