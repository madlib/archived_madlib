/* ----------------------------------------------------------------------- *//**
 *
 * @file unwrap.hpp
 *
 * @brief Helper class for integration of Vector_const with Armadillo
 *
 * An arma::Base object can be converted to a Mat object by the unwrap class.
 * See also armadillo_bits/base.hpp and armadillo_bits/unwrap.hpp
 *
 *//* ----------------------------------------------------------------------- */

template<template <class> class T, typename eT>
class unwrap< T<eT> > {
public:
    inline unwrap(const T<eT> &inVector)
        : M(inVector.mVector) {
        
        arma_extra_debug_sigprint();
    }

    const Col<double>& M;
};
