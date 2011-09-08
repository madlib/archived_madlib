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
    static MemHandleSPtr create(void *inPtr) {
        return MemHandleSPtr(new TransparentHandle(inPtr));
    }

    void *ptr() {
        return mPtr;
    }

    void release() { };
   
    /**
     * @brief Report an error because a TransparentHandle cannot be cloned.
     */
    MemHandleSPtr clone() const {
        throw std::logic_error("Internal error: TransparentHandle::clone() "
            "called.");
    }
        
protected:
    TransparentHandle(void *inPtr)
        : mPtr(inPtr) { }

    void *mPtr;
};
