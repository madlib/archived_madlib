/* ----------------------------------------------------------------------- *//**
 *
 * @file ArrayHandle_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */
 
#ifndef MADLIB_POSTGRES_ARRAYHANDLE_PROTO_HPP
#define MADLIB_POSTGRES_ARRAYHANDLE_PROTO_HPP

namespace madlib {

namespace dbconnector {

namespace postgres {

template <typename T>
class ArrayHandle {
public:
    enum { isMutable = false };

    ArrayHandle(const ArrayType *inArray);
    
    const T* ptr() const;
    size_t size() const;
    size_t dims() const;
    size_t sizeOfDim(size_t inDim) const;
    const ArrayType *array() const;    
    const T& operator[](size_t inIndex) const;

protected:
    const ArrayType *mArray;
    static size_t internalArraySize(const ArrayType *inArray);
};


template <typename T>
class MutableArrayHandle : public ArrayHandle<T> {
    typedef ArrayHandle<T> Base;
    
public:
    enum { isMutable = true };

    MutableArrayHandle(ArrayType *inArray)
      : Base(inArray) { }
    
    using Base::ptr;
    using Base::array;
    using Base::dims;
    using Base::sizeOfDim;
    
    T* ptr();
    ArrayType *array();
    T& operator[](size_t inIndex);
};

} // namespace postgres

} // namespace dbconnector

} // namespace madlib

#endif // defined(MADLIB_POSTGRES_ARRAYHANDLE_PROTO_HPP)
