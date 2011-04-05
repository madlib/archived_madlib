#ifndef MADLIB_PGALLOCATOR_HPP
#define MADLIB_PGALLOCATOR_HPP

#include <madlib/ports/postgres/postgres.hpp>

extern "C" {
    #include "utils/array.h"
} // extern "C"

namespace madlib {

namespace ports {

namespace postgres {

class PGAllocator : public AbstractAllocator {
friend class PGInterface;
public:
    MemHandleSPtr allocateArray(
        uint32_t inNumElements, double * /* ignored */) const;
    
    void deallocate(MemHandleSPtr inHandle) const;

protected:
    PGAllocator(Context inContext, const PGInterface &inPGInterface)
        : mContext(inContext), mPGInterface(inPGInterface) { }

    
    ArrayType *internalAllocateForArray(Oid inElementType,
        uint32_t inNumElements, size_t inElementSize) const;

    size_t sizeofOid(Oid inOid) const;
    
    void *allocate(const uint32_t inSize) const;
    
    inline void free(void *inPtr) const {
        pfree(inPtr);
    }
    
    Context mContext;
    const PGInterface &mPGInterface;    
};

} // namespace postgres

} // namespace ports

} // namespace madlib

#endif
