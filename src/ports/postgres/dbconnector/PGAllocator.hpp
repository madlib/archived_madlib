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
    static inline PGAllocator &defaultAllocator() {
        static PGAllocator sDefaultAllocator;
        
        return sDefaultAllocator;
    }

    MemHandleSPtr allocateArray(
        uint64_t inNumElements, double * /* ignored */) const;
    
    inline void *allocate(const size_t inSize) const throw(std::bad_alloc) {
        return internalAllocate<false>(NULL, inSize);
    }

    inline void *reallocate(void *inPtr, const size_t inSize) const
        throw(std::bad_alloc) {
    
        return internalAllocate<true>(inPtr, inSize);
    }

    inline void *allocate(const size_t inSize, const std::nothrow_t& dummy) const
        throw() {
    
        return internalAllocate<false>(NULL, inSize, dummy);
    }
    
    inline void *reallocate(void *inPtr, const size_t inSize,
        const std::nothrow_t& dummy) const throw() {
    
        return internalAllocate<true>(inPtr, inSize, dummy);
    }
    
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
    
    static inline void *makeAligned(void *inPtr);
        
    static inline void *unaligned(void *inPtr);
    
    void *internalPalloc(size_t inSize, bool inZero = false) const;

    void *internalRePalloc(void *inPtr, size_t inSize) const;
    
    template <bool Reallocate>
    void *internalAllocate(void *inPtr, const size_t inSize) const
        throw(std::bad_alloc);
    
    template <bool Reallocate>
    void *internalAllocate(void *inPtr, const size_t inSize,
        const std::nothrow_t&) const throw();
    
    const PGInterface *const mPGInterface;    
    Context mContext;
    bool mZeroMemory;
};

} // namespace dbconnector

} // namespace madlib

#endif
