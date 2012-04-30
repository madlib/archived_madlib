/* ----------------------------------------------------------------------- *//**
 *
 * @file Allocator_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_ALLOCATOR_PROTO_HPP
#define MADLIB_POSTGRES_ALLOCATOR_PROTO_HPP

namespace madlib {

namespace dbconnector {

namespace postgres {

template <typename T>
class MutableArrayHandle;

/**
 * @brief PostgreSQL memory allocator
 *
 * PostgreSQL knows the concept of "memory contexts" such as current function
 * call, current aggregate function, or current transaction. Memory
 * allocation using \c palloc() always occurs within a specific memory
 * context -- and once a memory context goes out of scope all memory
 * associated with it will be deallocated (garbage collected).
 * 
 * In C++ code one should in general not rely on this form of garbage
 * collection, as destructors could be used for releasing other resources
 * than memory. Nonetheless, protection at least against memory leaks is better
 * than no protection.
 * 
 * In NewDelete.cpp, we therefore redefine <tt>operator new()</tt> and
 * <tt>operator delete()</tt> to call \c palloc() and \c pfree().
 *
 * @see Allocator::internalAllocate, NewDelete.cpp
 *
 * @internal
 *     To avoid name conflicts, we do not import namespace dbal
 */
class Allocator {
public:
    Allocator(FunctionCallInfo inFCInfo) : fcinfo(inFCInfo) { }

    template <typename T, dbal::MemoryContext MC,
        dbal::ZeroMemory ZM, dbal::OnMemoryAllocationFailure F>
    MutableArrayHandle<T> allocateArray(size_t inNumElements) const;
    
    template <typename T>
    MutableArrayHandle<T> allocateArray(size_t inNumElements) const;

    template <dbal::MemoryContext MC, dbal::ZeroMemory ZM,
        dbal::OnMemoryAllocationFailure F>
    void *allocate(const size_t inSize) const;
    
    template <dbal::MemoryContext MC, dbal::ZeroMemory ZM,
        dbal::OnMemoryAllocationFailure F>
    void *reallocate(void *inPtr, const size_t inSize) const;
        
    template <dbal::MemoryContext MC>
    void free(void *inPtr) const;
    
protected:
    /**
     * @brief Template argument type for internalAllocate()
     */
    enum ReallocateMemory {
        NewAllocation,
        Reallocation
    };

    template <dbal::ZeroMemory ZM>
    void *internalPalloc(size_t inSize) const;
    
    template <dbal::ZeroMemory ZM>
    void *internalRePalloc(void *inPtr, size_t inSize) const;

    void *makeAligned(void *inPtr) const;
    void *unaligned(void *inPtr) const;

    template <dbal::MemoryContext MC, dbal::ZeroMemory ZM,
        dbal::OnMemoryAllocationFailure F, Allocator::ReallocateMemory R>
    void *internalAllocate(void *inPtr, const size_t inSize) const;

    /**
     * @brief The PostgreSQL FunctionCallInfo passed to the UDF
     *
     * @internal
     *     The name \c fcinfo is choosen on purpose because several PostgreSQL
     *     macros rely on it.
     */
    FunctionCallInfo fcinfo;
};

Allocator& defaultAllocator();

} // namespace postgres

} // namespace dbconnector

} // namespace madlib

#endif // defined(MADLIB_POSTGRES_ALLOCATOR_PROTO_HPP)
