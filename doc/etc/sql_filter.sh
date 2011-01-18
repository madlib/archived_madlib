#!/bin/sh
MADLIB_SCHEMA="MADlib"
FILTER=doc/bin/doxysql

m4 -DMADLIB_SCHEMA="${MADLIB_SCHEMA}" $1 | $FILTER
