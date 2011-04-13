/* ----------------------------------------------------------------------- *//**
 *
 * @file ArmadilloIntegration.hpp
 *
 * @brief Helper class for integration of Vector_const with Armadillo
 *
 * An arma::Base object can be converted to a Mat object by the unwrap class.
 * See also armadillo_bits/base.hpp and armadillo_bits/unwrap.hpp
 *
 *//* ----------------------------------------------------------------------- */

template<template <class> class T, typename eT>
class unwrap< madlib::dbal::Vector_const<T,eT> > {
public:
    inline unwrap(const madlib::dbal::Vector_const<T,eT> &inVector)
        : M(inVector.mVector) {
        
        arma_extra_debug_sigprint();
    }

    const T<eT>& M;
};

template<template <class> class T, typename eT>
class Proxy< madlib::dbal::Vector_const<T,eT> > {
public:
    typedef eT                                       elem_type;
    typedef typename get_pod_type<elem_type>::result pod_type;
    typedef T<eT>                                    stored_type;
    typedef const eT*                                ea_type;

    arma_aligned const T<eT>& Q;

    inline explicit Proxy(const madlib::dbal::Vector_const<T,eT>& inVector)
        : Q(inVector.mVector) {

        arma_extra_debug_sigprint();
    }

    arma_inline u32 get_n_rows() const { return Q.n_rows; }
    arma_inline u32 get_n_cols() const { return Q.n_cols; }
    arma_inline u32 get_n_elem() const { return Q.n_elem; }

    arma_inline elem_type operator[] (const u32 i)                  const { return Q[i];           }
    arma_inline elem_type at         (const u32 row, const u32 col) const { return Q.at(row, col); }

    arma_inline ea_type get_ea()                   const { return Q.memptr(); }
    arma_inline bool    is_alias(const Mat<eT>& X) const { return (&Q == &X); }
};
