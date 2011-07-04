/* ----------------------------------------------------------------------- *//**
 *
 * @file AbstractHandle.hpp
 *
 *//* ----------------------------------------------------------------------- */


/**
 * @brief Abstract base class for a memory handle
 *
 * A handle consists of a pointer to a block of memory, plus opaque meta data.
 * For example, a handle for a DBMS-native array would point to the contiguous
 * block of memory containing the raw data. The handle would also keep track of
 * the meta data (array size, element type, etc.), which is DBMS-specific.
 * 
 * Any implementation-independent code must only use the interface provided by
 * this abstract base class.
 *
 * In a handle's destructor, all memory blocks owned by the handle must be
 * deallocated. A handle is said to own a memory block, if
 * (a) it allocated the memory block itself or
 * (b) the memory block was allocated during the clone() operation that created
 *     the handle.
 */
class AbstractHandle {
public:
    virtual ~AbstractHandle() { }
    
    /**
     * @brief Return a pointer to the raw data this handle points to. 
     */
    virtual void *ptr() = 0;
    
    /**
     * @brief Return a copy of this memory handle.
     */
    virtual MemHandleSPtr clone() const = 0;
};
