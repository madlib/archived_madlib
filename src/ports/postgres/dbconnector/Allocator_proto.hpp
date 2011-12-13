/* ----------------------------------------------------------------------- *//**
 *
 * @file Allocator_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief PostgreSQL memory allocator
 *
 * The main issue consideration for memory allocation in PostgreSQL is:
 * -# Memory leaks are only guaranteed not to occur if PostgreSQL memory
 *    allocation functions are used\n
 *    .
 *    PostgreSQL knows the concept of "memory contexts" such as current function
 *    call, current aggregate function, or current transaction. Memory
 *    allocation using \c palloc() always occurs within a specific memory
 *    context -- and once a memory context goes out of scope all memory
 *    associated with it will be deallocated (garbage collected).\n
 *    .
 *    As a second safety measure we therefore redefine <tt>operator new</tt> and
 *    <tt>operator delete</tt> to call \c palloc() and \c pfree(). (This is
 *    essentially an \b additional protection against leaking C++ code. Given
 *    1., no C++ destructor call will ever be missed.)
 *
 * @see AbstractionLayer::Allocator::internalAllocate, NewDelete.cpp
 *
 * @internal
 *     To avoid name conflicts, we do not import namespace dbal
 */
class AbstractionLayer::Allocator {
public:
    enum ReallocateMemory {
        NewAllocation,
        Reallocation
    };    

    Allocator(FunctionCallInfo inFCInfo) : fcinfo(inFCInfo) { }

    template <typename T>
    MutableArrayHandle<T> allocateArray(size_t inNumElements) const;

    template <typename T, dbal::MemoryContext MC,
        dbal::ZeroMemory ZM, dbal::OnMemoryAllocationFailure F>
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
    template <dbal::ZeroMemory ZM>
    void *internalPalloc(size_t inSize) const;
    
    template <dbal::ZeroMemory ZM>
    void *internalRePalloc(void *inPtr, size_t inSize) const;

    void *makeAligned(void *inPtr) const;
    void *unaligned(void *inPtr) const;

    template <dbal::MemoryContext MC, dbal::ZeroMemory ZM,
        dbal::OnMemoryAllocationFailure F, Allocator::ReallocateMemory R>
    void *internalAllocate(void *inPtr, const size_t inSize) const;

    FunctionCallInfo fcinfo;
};
