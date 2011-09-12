/* ----------------------------------------------------------------------- *//**
 *
 * @file TransparentHandle.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief The most simple handle type -- a normal pointer
 *
 * A TransparentHandle is simply a pointer that never owns the memory it points
 * to. As such, the clone() operation is not supported. The main use for a
 * TransparentHandle is for referring to memory that is controlled below the
 * the DB abstraction layer (and any functionality implemented on top of the
 * DBAL).
 */
class TransparentHandle : public AbstractHandle {
public:
    using AbstractHandle::MemoryController;

    virtual ~TransparentHandle() {
        if (memoryController() == kSelf)
            defaultAllocator().free(mPtr);
    }

    /**
     * @brief Return a shared pointer to a new TransparentHandle
     *
     * @param inPtr Memory block to point to, or to copy if
     *     <tt>inCtrl == kSelf</tt>
     * @param inSize Size of the memory block. 0 if unknown.
     * @param inCtrl Ownership of the memory block
     */
    static inline MemHandleSPtr create(void *inPtr, size_t inSize = 0,
        MemoryController inCtrl = kLocal) {
        
        return MemHandleSPtr(new TransparentHandle(inPtr, inSize, inCtrl));
    }

    inline void *ptr() {
        return mPtr;
    }

    MemHandleSPtr clone() const {
        if (mSize == 0)
            throw std::logic_error("Internal error: TransparentHandle::clone() "
                "called on a handle with unknown size.");
        
        return MemHandleSPtr( new TransparentHandle(mPtr, mSize, kSelf) );
    }
        
protected:
    TransparentHandle(void *inPtr, size_t inSize, MemoryController inCtrl)
        : AbstractHandle(inCtrl), mPtr(inPtr), mSize(inSize) {
        
        if (inCtrl == kSelf) {
            mPtr = defaultAllocator().allocate(inSize);
            std::memcpy(mPtr, inPtr, inSize);
        }
    }

    void *mPtr;
    
    /**
     * @brief Size of the memory block. A size of zero indicates that the size
     *     is unknown.
     */
    size_t mSize;
};
