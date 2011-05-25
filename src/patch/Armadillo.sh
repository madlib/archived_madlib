#!/bin/sh
#
# On platforms with a flat namespace for symbols in executables, patch
# Armadillo so that it calls madlib_{LAPACK/BLAS function} instead of
# {LAPACK/BLAS function}. This is necessary because some DBMSs contain
# LAPACK/BLAS symbols, which we do not want to use. Instead, the connector
# libraries will define madlib_{LAPACK/BLAS function} to look up
# {LAPACK/BLAS function} dynamically with dlsym()

cat - >> armadillo_bits/compiler_setup.hpp <<'EOF'

// The following lines have been added for use with MADlib. We want to route all
// LAPACK/BLAS calls through the MADlib connector library.

#undef arma_fortran
#define arma_fortran(function) madlib_##function
EOF
