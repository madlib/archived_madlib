#include <madlib/ports/postgres/PGAllocator.hpp>
#include <madlib/ports/postgres/PGArrayHandle.hpp>
#include <madlib/ports/postgres/PGInterface.hpp>

extern "C" {
    #include <postgres.h>
    #include <fmgr.h>
    #include <catalog/pg_type.h>
} // extern "C"

namespace madlib {

namespace ports {

namespace postgres {

MemHandleSPtr PGAllocator::allocateArray(uint32_t inNumElements,
    double * /* ignored */) const {
    
    return MemHandleSPtr(new PGArrayHandle(
        internalAllocateForArray(FLOAT8OID, inNumElements, sizeof(double))));
}
    
void PGAllocator::deallocate(MemHandleSPtr inHandle) const {
    shared_ptr<PGArrayHandle> arrayPtr = dynamic_pointer_cast<PGArrayHandle>(inHandle);
    
    if (arrayPtr)
        pfree(arrayPtr->mArray);
    else
        throw std::logic_error("Tried to deallocate invalid handle");
}

ArrayType *PGAllocator::internalAllocateForArray(Oid inElementType,
    uint32_t inNumElements, size_t inElementSize) const {
    
    int64		size = inElementSize * inNumElements + ARR_OVERHEAD_NONULLS(1);
    ArrayType	*array;
    void		*arrayData;

    array = static_cast<ArrayType *>(allocate(size));
    SET_VARSIZE(array, size);
    array->ndim = 1;
    array->dataoffset = 0;
    array->elemtype = inElementType;
    ARR_DIMS(array)[0] = inNumElements;
    ARR_LBOUND(array)[0] = 1;
    arrayData = ARR_DATA_PTR(array);
    memset(arrayData, 0, inElementSize * inNumElements);
    return array;
}

void *PGAllocator::allocate(const uint32_t inSize) const {
    void *ptr;
    MemoryContext oldContext;
    MemoryContext aggContext;
    
    if (mContext == kAggregate) {
        if (!AggCheckCallContext(mPGInterface.fcinfo, &aggContext))
            throw std::logic_error("Internal error: Tried to allocate "
                "memory in aggregate context while not in aggregate");
        
        oldContext = MemoryContextSwitchTo(aggContext);
        ptr = palloc(inSize);
        MemoryContextSwitchTo(oldContext);
    } else {
        ptr = palloc(inSize);
    }
    
    return ptr;
}

size_t PGAllocator::sizeofOid(Oid inOid) const {
    switch (inOid) {
        case FLOAT8OID: return sizeof(double);
    }
    
    throw std::logic_error("Internal error: Tried to determine size of "
        "unsupported data type");
}



} // namespace postgres

} // namespace ports

} // namespace madlib
