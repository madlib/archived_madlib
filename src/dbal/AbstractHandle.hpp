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
 * deallocated. A handle is said to own a memory block (indicated by
 * <tt>mMemoryController == kSelf</tt>), if:
 * -# One of the following two condition holds:
 *     - It allocated the memory block itself.
 *     - The memory block was allocated during the clone() operation that
 *       created the handle.
 * -# release() has not been called before. Implementations must detail how
 *    memory is to be deallocated manually if release() has been called before.
 *
 * @note There is a 1-to-1 relationship between a handle and the memory blocks
 *     it owns. There is \b no shared ownership.
 */
class AbstractHandle {
public:
    /**
     * @brief Options for memory ownership
     * 
     * - \c kGlobal indicates that the caller forsees that the memory
     *   block(s) pointed to by this handle will never go of scope while this
     *   memory handle is alive. Unless a distinct (mutable) copy of the memory
     *   block(s) is needed, it is gemerally preferable to copy shared pointers
     *   to this handle (i.e., make sa shallow copy of only the reference) than
     *   to do a clone (i.e., make a deep copy).
     * - \c kLocal indicates that the memory block pointed to by this memory
     *   handle is transient. There is no guarantee that the memory block(s)
     *   pointed to by this handle remain valid while this handle is alive.
     *   (For instance, this would be the case if the memory block is allocated
     *   on the stack.)
     *   Copying an object backed by this memory handle (e.g., an array or a
     *   matrix) typically involves a clone(), which creates a copy of the
     *   memory block(s) pointed to by this handle. The cloned memory handle
     *   will then own that memory block(s) and
     *   have \c mMemoryController set to \c kSelf.
     * - \c kSelf indicates that this memory handle owns its memory block(s) and
     *   will also take care of deallocating them at the end of its lifetime.
     */
    enum MemoryController { kGlobal, kLocal, kSelf };

    AbstractHandle(MemoryController inCtrl)
        : mMemoryController(inCtrl) { }
    
    virtual ~AbstractHandle() { }
    
    /**
     * @brief Return a pointer to the raw data this handle points to. 
     */
    virtual void *ptr() = 0;
    
    /**
     * @brief Relinquish ownership of the memory blocks owned by this handle.
     *
     * This sets mMemoryController to kLocal.
     */
    virtual void release() {
        if (mMemoryController == kSelf) 
            mMemoryController = kLocal;
    };
    
    /**
     * @brief Return a deep (and fully independent) copy of this memory handle
     */
    virtual MemHandleSPtr clone() const = 0;
    
    /**
     * @brief Return ownership of the memory block(s) pointed to by this memory
     *     handle
     */
    MemoryController memoryController() const {
        return mMemoryController;
    }
    
    static inline MemHandleSPtr cloneIfNotGlobal(const MemHandleSPtr &inHandle) {
        return inHandle->mMemoryController == kGlobal ?
            inHandle : inHandle->clone();
    }

private:
    MemoryController mMemoryController;
};
