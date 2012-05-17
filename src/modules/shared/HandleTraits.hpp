/* ----------------------------------------------------------------------- *//**
 *
 * @file HandleTraits.hpp
 *
 *//* ----------------------------------------------------------------------- */

namespace madlib {

namespace modules {

/**
 * @brief Define mutable and immutable references
 *
 * Some modules store transition states in a composite type that they present to
 * the backend only as double array (for performance reasons). The elements of
 * the composite type "inherit" their mutability from the array. To that end,
 * HandleTraits takes a Handle type as template argument and correspondingly
 * defines mutable or immutable reference types.
 * 
 * @note
 *     HandleTraits are used for strict type safety and const-correctness.
 *     Just using <tt>const_cast</tt> is arguably a bit shorter, but less
 *     "correct".
 *
 * @see For an example usage, see linear.cpp.
 */
template <class Handle>
struct HandleTraits;

template <>
struct HandleTraits<ArrayHandle<double> > {
    typedef dbal::eigen_integration::ColumnVector ColumnVector;
    typedef dbal::eigen_integration::Matrix Matrix;

    typedef utils::Reference<double, uint64_t> ReferenceToUInt64;
    typedef utils::Reference<double, int64_t> ReferenceToInt64;
    typedef utils::Reference<double, uint32_t> ReferenceToUInt32;
    typedef utils::Reference<double, uint16_t> ReferenceToUInt16;
    typedef utils::Reference<double, bool> ReferenceToBool;
    typedef utils::Reference<double> ReferenceToDouble;
    typedef const double* DoublePtr;
    typedef dbal::eigen_integration::HandleMap<
        const ColumnVector, TransparentHandle<double> >
        ColumnVectorTransparentHandleMap;
    typedef dbal::eigen_integration::HandleMap<const Matrix,
        TransparentHandle<double> > MatrixTransparentHandleMap;
};

template <>
struct HandleTraits<MutableArrayHandle<double> > {
    typedef dbal::eigen_integration::ColumnVector ColumnVector;
    typedef dbal::eigen_integration::Matrix Matrix;

    typedef utils::MutableReference<double, uint64_t> ReferenceToUInt64;
    typedef utils::MutableReference<double, int64_t> ReferenceToInt64;
    typedef utils::MutableReference<double, uint32_t> ReferenceToUInt32;
    typedef utils::MutableReference<double, uint16_t> ReferenceToUInt16;
    typedef utils::MutableReference<double, bool> ReferenceToBool;
    typedef utils::MutableReference<double> ReferenceToDouble;
    typedef double* DoublePtr;
    typedef dbal::eigen_integration::HandleMap<ColumnVector,
        MutableTransparentHandle<double> > ColumnVectorTransparentHandleMap;
    typedef dbal::eigen_integration::HandleMap<Matrix,
        MutableTransparentHandle<double> > MatrixTransparentHandleMap;
};

} // namespace modules

} // namespace madlib
