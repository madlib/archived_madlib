/* ----------------------------------------------------------------------- *//**
 *
 * @file EigenIntegration_impl.cpp
 *
 * @brief Convert between PostgreSQL types and Eigen types
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_EIGEN_INTEGRATION_IMPL_HPP
#define MADLIB_POSTGRES_EIGEN_INTEGRATION_IMPL_HPP

namespace madlib {

namespace dbal {

namespace eigen_integration {

// Specializations for DBMS-specific types

// ------------------------------------------------------------------------
// HandleMap::HandleMap()
// ------------------------------------------------------------------------
// eigen_integration::NativeMatrix
/**
 * @brief Initialize HandleMap backed by the given handle
 *
 * In PostgreSQL, we represent a matrix as an array of columns. Therefore, the
 * index 0 is the number of columns, and index 1 is the number of rows.
 */
template <>
inline
HandleMap<const Matrix, ArrayHandle<double> >::HandleMap(
    const ArrayHandle<double>& inHandle)
  : Base(const_cast<double*>(inHandle.ptr()), inHandle.sizeOfDim(1),
        inHandle.sizeOfDim(0)),
    mMemoryHandle(inHandle) { }

// eigen_integration::MutableNativeMatrix
/**
 * @brief Initialize HandleMap backed by the given handle
 *
 * In PostgreSQL, we represent a matrix as an array of columns. Therefore, the
 * index 0 is the number of columns, and index 1 is the number of rows.
 */
template <>
inline
HandleMap<Matrix, MutableArrayHandle<double> >::HandleMap(
    const MutableArrayHandle<double>& inHandle)
  : Base(const_cast<double*>(inHandle.ptr()), inHandle.sizeOfDim(1),
        inHandle.sizeOfDim(0)),
    mMemoryHandle(inHandle) { }

// eigen_integration::NativeColumnVector
/**
 * @brief Construct the HandleMap as one-dimensional vector
 *
 * This constructor uses size() method in ArrayHandle to determine the length.
 */
template <>
inline
HandleMap<const ColumnVector, ArrayHandle<double> >::HandleMap(
    const ArrayHandle<double>& inHandle)
  : Base(const_cast<Scalar*>(inHandle.ptr()), inHandle.size()),
    mMemoryHandle(inHandle) { }

// eigen_integration::MutableNativeColumnVector
/**
 * @brief Construct the HandleMap as one-dimensional vector
 *
 * This constructor uses size() method in ArrayHandle to determine the length.
 */
template <>
inline
HandleMap<ColumnVector, MutableArrayHandle<double> >::HandleMap(
    const MutableArrayHandle<double>& inHandle)
  : Base(const_cast<Scalar*>(inHandle.ptr()), inHandle.size()),
    mMemoryHandle(inHandle) { }

// eigen_integration::NativeIntegerVector
/**
 * @brief Construct the HandleMap as one-dimensional vector
 *
 * This constructor uses size() method in ArrayHandle to determine the length.
 */
template <>
inline
HandleMap<const IntegerVector, ArrayHandle<int> >::HandleMap(
    const ArrayHandle<int>& inHandle)
  : Base(const_cast<Scalar*>(inHandle.ptr()), inHandle.size()),
    mMemoryHandle(inHandle) { }

// eigen_integration::MutableNativeIntegerVector
/**
 * @brief Construct the HandleMap as one-dimensional vector
 *
 * This constructor uses size() method in ArrayHandle to determine the length.
 */
template <>
inline
HandleMap<IntegerVector, MutableArrayHandle<int> >::HandleMap(
    const MutableArrayHandle<int>& inHandle)
  : Base(const_cast<Scalar*>(inHandle.ptr()), inHandle.size()),
    mMemoryHandle(inHandle) { }

// ------------------------------------------------------------------------
// HandleMap::rebind()
// ------------------------------------------------------------------------
// eigen_integration::NativeMatrix
/**
 * @brief Rebind HandleMap to a different two-dimensional array
 */
template <>
inline
HandleMap<const Matrix, ArrayHandle<double> >&
HandleMap<const Matrix, ArrayHandle<double> >::rebind(
    const ArrayHandle<double>& inHandle) {

    return rebind(inHandle, inHandle.sizeOfDim(1), inHandle.sizeOfDim(0));
}

// eigen_integration::MutableNativeMatrix
/**
 * @brief Rebind HandleMap to a different two-dimensional array
 */
template <>
inline
HandleMap<Matrix, MutableArrayHandle<double> >&
HandleMap<Matrix, MutableArrayHandle<double> >::rebind(
    const MutableArrayHandle<double>& inHandle) {

    return rebind(inHandle, inHandle.sizeOfDim(1), inHandle.sizeOfDim(0));
}

// eigen_integration::NativeColumnVector
/**
 * @brief Rebind HandleMap to a different one-dimensional array
 */
template <>
inline
HandleMap<const ColumnVector, ArrayHandle<double> >&
HandleMap<const ColumnVector, ArrayHandle<double> >::rebind(
    const ArrayHandle<double>& inHandle) {

    return rebind(inHandle, inHandle.sizeOfDim(0));
}

// eigen_integration::MutableNativeColumnVector
/**
 * @brief Rebind HandleMap to a different one-dimensional array
 */
template <>
inline
HandleMap<ColumnVector, MutableArrayHandle<double> >&
HandleMap<ColumnVector, MutableArrayHandle<double> >::rebind(
    const MutableArrayHandle<double>& inHandle) {

    return rebind(inHandle, inHandle.sizeOfDim(0));
}

// eigen_integration::NativeIntegerVector
/**
 * @brief Rebind HandleMap to a different one-dimensional array
 */
template <>
inline
HandleMap<const IntegerVector, ArrayHandle<int> >&
HandleMap<const IntegerVector, ArrayHandle<int> >::rebind(
    const ArrayHandle<int>& inHandle) {

    return rebind(inHandle, inHandle.sizeOfDim(0));
}

// eigen_integration::MutableNativeIntegerVector
/**
 * @brief Rebind HandleMap to a different one-dimensional array
 */
template <>
inline
HandleMap<IntegerVector, MutableArrayHandle<int> >&
HandleMap<IntegerVector, MutableArrayHandle<int> >::rebind(
    const MutableArrayHandle<int>& inHandle) {

    return rebind(inHandle, inHandle.sizeOfDim(0));
}

} // namespace eigen_integration

} // namespace dbal


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

    const double* values = inVec.valuePtr();
    const Index* indices = inVec.innerIndexPtr();
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


// ------------------------------------------------------------------------

/**
 * @brief Convert an Eigen row or column vector to a one-dimensional
 *     PostgreSQL array
 */
template <typename Derived>
ArrayType*
VectorToNativeArray(const Eigen::MatrixBase<Derived>& inVector) {
    typedef typename Derived::Scalar T;
    typedef typename Derived::Index Index;

    MutableArrayHandle<T> arrayHandle
        = defaultAllocator().allocateArray<T>(inVector.size());

    T* ptr = arrayHandle.ptr();
    for (Index el = 0; el < inVector.size(); ++el)
        *(ptr++) = inVector(el);

    return arrayHandle.array();
}

/**
 * @brief Convert an Eigen matrix to a two-dimensional PostgreSQL array
 */
template <typename Derived>
ArrayType*
MatrixToNativeArray(const Eigen::MatrixBase<Derived>& inMatrix) {
    typedef typename Derived::Scalar T;
    typedef typename Derived::Index Index;

    MutableArrayHandle<T> arrayHandle
        = defaultAllocator().allocateArray<T>(
            inMatrix.cols(), inMatrix.rows());

    T* ptr = arrayHandle.ptr();
    // We use columnar storage, i.e., each column is a contiguous block
    for (Index col = 0; col < inMatrix.cols(); ++col)
        for (Index row = 0; row < inMatrix.rows(); ++row)
            *(ptr++) = inMatrix(row, col);

    return arrayHandle.array();
}

/**
 * @brief Convert a native array to [Mutable]MappedVector
 */
template <class VectorType>
VectorType
NativeArrayToMappedVector(Datum inDatum, bool inNeedMutableClone) {
    typedef typename VectorType::Scalar Scalar;

    ArrayType* array = reinterpret_cast<ArrayType*>(
        madlib_DatumGetArrayTypeP(inDatum));
    size_t arraySize = ARR_NDIM(array) == 1
        ? ARR_DIMS(array)[0]
        : ARR_DIMS(array)[0] * ARR_DIMS(array)[1];

    if (!(ARR_NDIM(array) == 1
        || (ARR_NDIM(array) == 2
            && (ARR_DIMS(array)[0] == 1 || ARR_DIMS(array)[1] == 1)))) {

        std::stringstream errorMsg;
        errorMsg << "Invalid type conversion to matrix. Expected one-"
            "dimensional array but got " << ARR_NDIM(array)
            << " dimensions.";
        throw std::invalid_argument(errorMsg.str());
    }

    Scalar* origData = reinterpret_cast<Scalar*>(ARR_DATA_PTR(array));
    Scalar* data;

    if (inNeedMutableClone) {
        data = reinterpret_cast<Scalar*>(
            defaultAllocator().allocate<dbal::FunctionContext, dbal::DoNotZero,
                dbal::ThrowBadAlloc>(sizeof(Scalar) * arraySize));
        std::copy(origData, origData + arraySize, data);
    } else {
        data = reinterpret_cast<Scalar*>(ARR_DATA_PTR(array));
    }

    return VectorType(data, arraySize);
}

/**
 * @brief Convert an Eigen VectorXcd to a two-dimensional PostgreSQL array
 */
template <typename Derived>
ArrayType*
VectorXcdToNativeArray(const Eigen::MatrixBase<Derived>& inMatrix) {
    typedef typename Derived::Scalar::value_type T;
    typedef typename Derived::Index Index;

    MutableArrayHandle<T> arrayHandle
        = defaultAllocator().allocateArray<T>(
            inMatrix.rows(), 2 /* for real and imag */);

    T* ptr = arrayHandle.ptr();
    // We use columnar storage, i.e., each column is a contiguous block
    for (Index row = 0; row < inMatrix.rows(); ++row) {
        const std::complex<T> &value = inMatrix(row, 0);
        *(ptr++) = value.real();
        *(ptr++) = value.imag();
    }

    return arrayHandle.array();
}

/**
 * @brief Convert a native array to [Mutable]VectorXcd
 */
template <class VectorType>
VectorType
NativeArrayToMappedVectorXcd(Datum inDatum, bool inNeedMutableClone) {
    typedef typename VectorType::Scalar Scalar;

    ArrayType* array = reinterpret_cast<ArrayType*>(
        madlib_DatumGetArrayTypeP(inDatum));

    if (ARR_NDIM(array) != 2 || ARR_DIMS(array)[1] != 2) {
        std::stringstream errorMsg;
        errorMsg << "Invalid type conversion to VectorXcd. Expected two-"
            "dimensional array with two elements for secondary dimension "
            "but got " << ARR_NDIM(array) << " dimensions and "
            << ARR_DIMS(array)[1] << " elements in secondary dimension.";
        throw std::invalid_argument(errorMsg.str());
    }

    size_t arraySize = ARR_DIMS(array)[0];

    Scalar* origData = reinterpret_cast<Scalar*>(ARR_DATA_PTR(array));
    std::complex<Scalar>* data;

    if (inNeedMutableClone) {
        data = reinterpret_cast<Scalar*>(
            defaultAllocator().allocate<dbal::FunctionContext, dbal::DoNotZero,
                dbal::ThrowBadAlloc>(sizeof(std::complex<Scalar>) * arraySize));
        for (size_t i = 0; i < arraySize; ++i) {
            data[i] = std::complex<Scalar>(origData[0], origData[1]);
            origData += 2;
        }
    } else {
        data = reinterpret_cast<std::complex<Scalar>*>(ARR_DATA_PTR(array));
    }

    return VectorType(data, arraySize);
}

/**
 * @brief Convert a native array to [Mutable]MappedMatrix
 */
template <class MatrixType>
MatrixType
NativeArrayToMappedMatrix(Datum inDatum, bool inNeedMutableClone) {
    typedef typename MatrixType::Scalar Scalar;

    ArrayType* array = reinterpret_cast<ArrayType*>(
        madlib_DatumGetArrayTypeP(inDatum));
    size_t arraySize = ARR_DIMS(array)[0] * ARR_DIMS(array)[1];

    if (ARR_NDIM(array) != 2) {
        std::stringstream errorMsg;
        errorMsg << "Invalid type conversion to matrix. Expected two-"
            "dimensional array but got " << ARR_NDIM(array)
            << " dimensions.";
        throw std::invalid_argument(errorMsg.str());
    }

    Scalar* origData = reinterpret_cast<Scalar*>(ARR_DATA_PTR(array));
    Scalar* data;

    if (inNeedMutableClone) {
        data = reinterpret_cast<Scalar*>(
            defaultAllocator().allocate<dbal::FunctionContext, dbal::DoNotZero,
                dbal::ThrowBadAlloc>(sizeof(Scalar) * arraySize));
        std::copy(origData, origData + arraySize, data);
    } else {
        data = reinterpret_cast<Scalar*>(ARR_DATA_PTR(array));
    }

    return MatrixType(data, ARR_DIMS(array)[1], ARR_DIMS(array)[0]);
}

} // namespace postgres

} // namespace dbconnector

} // namespace madlib

#endif // defined(MADLIB_POSTGRES_EIGEN_INTEGRATION_IMPL_HPP)
