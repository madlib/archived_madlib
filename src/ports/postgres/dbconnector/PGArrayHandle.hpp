#ifndef MADLIB_POSTGRES_PGARRAYHANDLE_HPP
#define MADLIB_POSTGRES_PGARRAYHANDLE_HPP

#include <dbconnector/PGCommon.hpp>

extern "C" {
    #include "utils/array.h"
} // extern "C"

namespace madlib {

namespace dbconnector {

class PGArrayHandle : public AbstractHandle {
friend class PGAllocator;
public:    
    PGArrayHandle(ArrayType *inArray) : mArray(inArray) { }

    virtual void *ptr() {
        return ARR_DATA_PTR(mArray);
    }
    
    virtual ArrayType *array() {
        return mArray;
    }
    
    virtual MemHandleSPtr clone() const {
        // Use operator new to allocate memory (this will allocate memory in
        // the default postgres context)
        ArrayType   *newArray =
            static_cast<ArrayType *>( ::operator new(VARSIZE(mArray)) );
        std::memcpy(newArray, mArray, VARSIZE(mArray));
        return MemHandleSPtr( new PGArrayHandle(newArray) );
    }
    
protected:
    ArrayType *mArray;
};

} // namespace dbconnector

} // namespace madlib

#endif
