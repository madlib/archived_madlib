#ifndef MADLIB_POSTGRES_PGARRAYHANDLE_HPP
#define MADLIB_POSTGRES_PGARRAYHANDLE_HPP

#include <dbconnector/PGCommon.hpp>

extern "C" {
    #include "utils/array.h"
} // extern "C"

namespace madlib {

namespace dbconnector {

/**
 * @brief Handle to a PostgreSQL array
 */
class PGArrayHandle : public AbstractHandle {
friend class PGAllocator;
public:
    PGArrayHandle(ArrayType *inArray, bool inCopyMem = false);
        
    ~PGArrayHandle();

    virtual void *ptr() {
        return ARR_DATA_PTR(mArray);
    }
    
    virtual ArrayType *array() {
        return mArray;
    }
    
    virtual void release() {
        mOwnsArray = false;
    }
    
    virtual MemHandleSPtr clone() const {
        return MemHandleSPtr( new PGArrayHandle(mArray, true) );
    }
    
protected:
    ArrayType *mArray;
    bool mOwnsArray;
};

} // namespace dbconnector

} // namespace madlib

#endif
