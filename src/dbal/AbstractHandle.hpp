/* ----------------------------------------------------------------------- *//**
 *
 * @file AbstractHandle.hpp
 *
 *//* ----------------------------------------------------------------------- */


/**
 * @brief Abstract base class for a memory handle
 *
 * A handle is an opaque reference to a block of memory. An example might
 * be an array handle: It points to a contiguous block of memory containing
 * raw data. The meta data (array size, element type, etc.), however,
 * might be stored separately, in a port-specific fashion. The
 * platform-independent code will only work with the interface provided by
 * this acstract base class.
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
