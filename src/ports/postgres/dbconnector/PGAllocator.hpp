#ifndef MADLIB_POSTGRES_PGALLOCATOR_HPP
#define MADLIB_POSTGRES_PGALLOCATOR_HPP

#include <dbconnector/PGCommon.hpp>

extern "C" {
    #include "utils/array.h"
} // extern "C"

namespace madlib {

namespace dbconnector {

/**
 * @brief PostgreSQL memory allocation
 *
 * Implement the AbstractAllocator interface on top of the PostgreSQL API.
 */
class PGAllocator : public AbstractAllocator {
friend class PGInterface;
public:
    /**
     * @brief Return the default allocator used by operator new and operator delete.
     */
    static PGAllocator &defaultAllocator() {
        static PGAllocator sDefaultAllocator;
        
        return sDefaultAllocator;
    }

    MemHandleSPtr allocateArray(
        uint64_t inNumElements, double * /* ignored */) const;
    
    void *allocate(const size_t inSize) const throw(std::bad_alloc);

    void *allocate(const size_t inSize, const std::nothrow_t&) const
        throw();
    
    void free(void *inPtr) const throw();

protected:
    PGAllocator()
        : mPGInterface(NULL), mContext(kFunction), mZeroMemory(false)
        { }

    PGAllocator(const PGInterface *const inPGInterface, Context inContext,
        ZeroMemory inZeroMemory)
        : mPGInterface(inPGInterface), mContext(inContext),
          mZeroMemory(inZeroMemory == kZero)
        { }

    ArrayType *internalAllocateForArray(Oid inElementType,
        uint64_t inNumElements, size_t inElementSize) const;
    
    static void *internalPalloc(size_t inSize, bool inZero = false);
    
    static void internalPfree(void *inPtr);
        
    const PGInterface *const mPGInterface;    
    Context mContext;
    bool mZeroMemory;
};

} // namespace dbconnector

} // namespace madlib

#endif
