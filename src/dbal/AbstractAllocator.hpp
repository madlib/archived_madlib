/* ----------------------------------------------------------------------- *//**
 *
 * @file AbstractAllocator.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Abstract base class a memory allocator
 *
 * This class provides the interface that MADlib modules use for memory
 * management.
 */
class AbstractAllocator {
public:
    /**
     * @brief Context for memory allocations.
     * 
     * Two contexts are available in the abstraction layer: Current
     * (user-defined) function call, and aggregate context. The main difference
     * will be when automatic garbage collection takes place (if the DBMS
     * supports it).
     */
    enum Context { kFunction, kAggregate };

    virtual ~AbstractAllocator() { }

    /**
     * @brief Allocate memory for raw data and the DBMS-specific meta data.
     * 
     * Subclasses need to override this function to allocate an array in the
     * DBMS-specific way. The purpose of this function is to enable efficient
     * implementations of user-defined functions that return arrays:
     * In this case, the UDF could create the array during its execution, fill
     * it with data, and eventually return the result array by reference.
     * 
     * Note: The second argument is purely a type parameter.
     */
    virtual MemHandleSPtr allocateArray(uint32_t inNumElements,
        double * = NULL /* ignored */) const = 0;
    
    /**
     * @brief Deallocate a memory handle
     */
    virtual void deallocateHandle(MemHandleSPtr inMemoryHandle) const = 0;

    /**
     * @brief Allocate a block of memory. Throw if allocation fails.
     */
    virtual void *allocate(const uint32_t inSize) const throw(std::bad_alloc) = 0;

    /**
     * @brief Allocate a block of memory. Return NULL if allocation fails.
     */
    virtual void *allocate(const uint32_t inSize, const std::nothrow_t&) const
        throw() = 0;
    
    /**
     * @brief Free a block of memory previously allocated with allocate()
     */
    virtual void free(void *inPtr) const throw() = 0;
};
