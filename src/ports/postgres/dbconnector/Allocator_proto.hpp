/**
 * @brief The PostgreSQL memory allocator
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
