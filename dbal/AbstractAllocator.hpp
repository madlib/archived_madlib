/* ----------------------------------------------------------------------- *//**
 *
 * @file AbstractAllocator.hpp
 *
 *//* ----------------------------------------------------------------------- */

class AbstractAllocator {
public:
    enum Context { kFunction, kAggregate };

    virtual ~AbstractAllocator() { }

    virtual MemHandleSPtr allocateArray(uint32_t inNumElements,
        double * = NULL /* ignored */) const = 0;
    
    virtual void deallocateHandle(MemHandleSPtr inMemoryHandle) const = 0;

    virtual void *allocate(const uint32_t inSize) const throw(std::bad_alloc) = 0;

    virtual void *allocate(const uint32_t inSize, const std::nothrow_t&) const
        throw() = 0;
    
    virtual void free(void *inPtr) const throw() = 0;
};
