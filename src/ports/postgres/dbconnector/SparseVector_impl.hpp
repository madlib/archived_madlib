/* ----------------------------------------------------------------------- *//**
 *
 * @file SparseVector_impl.cpp
 *
 * @brief Wrap MADlib legacy sparse vectors
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_SPARSEVECTOR_IMPL_HPP
#define MADLIB_POSTGRES_SPARSEVECTOR_IMPL_HPP

namespace madlib {

namespace dbconnector {

namespace postgres {

/**
 * @brief Convert a run-length encoded Greenplum sparse vector to an Eigen
 *     sparse vector
 *
 * @param inVec A Greenplum sparse vector
 * @returns Eigen sparse vector
 */
inline
Eigen::SparseVector<double>
LegacySparseVectorToSparseColumnVector(SvecType* inVec) {
    SparseData sdata = sdata_from_svec(inVec);
    Eigen::SparseVector<double> vec(sdata->total_value_count);

    char* ix = sdata->index->data;
    double* vals = reinterpret_cast<double*>(sdata->vals->data);

    int64_t logicalIdx = 0;
    for (int64_t physicalIdx = 0; physicalIdx < sdata->unique_value_count;
        ++physicalIdx) {

        int64_t runLength = compword_to_int8(ix);
        if (vals[physicalIdx] == 0.) {
            logicalIdx += runLength;
        } else {
            for (int64_t i = 0; i < runLength; ++i)
                vec.insertBack(static_cast<int>(logicalIdx++))
                    = vals[physicalIdx];
        }
        ix += int8compstoragesize(ix);
    }
    return vec;
}


/**
 * @brief Convert an Eigen sparse vector to a run-length encoded Greenplum
 *     sparse vector
 *
 * @param inVec An Eigen sparse vector
 * @returns Greenplum sparse vector
 *
 * @internal We implement this function here and not in the legacy sparse-vector
 *     code because the indices of type \c Index, as defined by Eigen.
 */
inline
SvecType*
SparseColumnVectorToLegacySparseVector(
    const Eigen::SparseVector<double> &inVec) {

    typedef Eigen::SparseVector<double>::Index Index;
    const size_t kValueLength = sizeof(double);

    const double* values = inVec._valuePtr();
    const Index* indices = inVec._innerIndexPtr();
    Index nnz = inVec.nonZeros();
    Index size = inVec.size();

    Index lastIndex = 0;
    double runValue = 0.;
    SparseData sdata = makeSparseData();

    sdata->type_of_data = FLOAT8OID;

    madlib_assert(nnz == 0 || (indices && values), std::logic_error(
        "SparseColumnVectorToLegacySparseVector(): Missing values or indices "
        "in Eigen sparse vector."));

    if (nnz > 0) {
        if (indices[0] == 0) {
            runValue = values[0];
        } else if (std::memcmp(&values[0], &runValue, kValueLength)) {
            // In this case, we implicitly have: indices[0] > 0
            // The first run is therefore a sequence of zeros.
            add_run_to_sdata(reinterpret_cast<char*>(&runValue),
                indices[0], kValueLength, sdata);
            runValue = values[0];
            lastIndex = indices[0];
        }
        // The remaining case is: indices[0] > 0 && values[0] == 0
        // In this case, the original representation is not normalized --
        // storing (indices[0], values[0]) is unncessary. We therefore just
        // ignore this value.
    }
    for (int i = 1; i < nnz; ++i) {
        if (std::memcmp(&values[i], &runValue, kValueLength)) {
            add_run_to_sdata(reinterpret_cast<char*>(&runValue),
                indices[i] - lastIndex, kValueLength, sdata);
            runValue = values[i];
            lastIndex = indices[i];
        }
    }
    add_run_to_sdata(reinterpret_cast<char*>(&runValue),
        size - lastIndex, kValueLength, sdata);

    // Add the final tallies
    sdata->unique_value_count
        = static_cast<int>(sdata->vals->len / kValueLength);
    sdata->total_value_count = static_cast<int>(size);

    return svec_from_sparsedata(sdata, true /* trim */);
}

} // namespace postgres

} // namespace dbconnector

} // namespace madlib

#endif // defined(MADLIB_POSTGRES_SPARSEVECTOR_IMPL_HPP)
