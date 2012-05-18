/* ----------------------------------------------------------------------- *//**
 *
 * @file SparseVector_proto.cpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_SPARSEVECTOR_PROTO_HPP
#define MADLIB_POSTGRES_SPARSEVECTOR_PROTO_HPP

namespace madlib {

namespace dbconnector {

namespace postgres {

Eigen::SparseVector<double> LegacySparseVectorToSparseColumnVector(
    SvecType* inVec);

SvecType* SparseColumnVectorToLegacySparseVector(
    const Eigen::SparseVector<double> &inVec);

} // namespace postgres

} // namespace dbconnector

} // namespace madlib

#endif // defined(MADLIB_POSTGRES_SPARSEVECTOR_PROTO_HPP)
