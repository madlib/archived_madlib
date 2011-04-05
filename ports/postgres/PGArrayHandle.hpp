#ifndef MADLIB_PGARRAYHANDLE_HPP
#define MADLIB_PGARRAYHANDLE_HPP

#include <madlib/ports/postgres/postgres.hpp>

extern "C" {
    #include "utils/array.h"
} // extern "C"

namespace madlib {

namespace ports {

namespace postgres {

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
    
protected:

    ArrayType *mArray;
};

} // namespace postgres

} // namespace ports

} // namespace madlib

#endif
