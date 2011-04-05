/* ----------------------------------------------------------------------- *//**
 *
 * @file AbstractAllocator.hpp
 *
 *//* ----------------------------------------------------------------------- */

class AbstractAllocator {
public:
    enum Context { kFunction, kAggregate };

    virtual MemHandleSPtr allocateArray(uint32_t inNumElements,
        double * = NULL /* ignored */) const = 0;
    
    virtual void deallocate(MemHandleSPtr inMemoryHandle) const = 0;
};
