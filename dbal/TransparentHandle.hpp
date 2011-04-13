/* ----------------------------------------------------------------------- *//**
 *
 * @file TransparentHandle.hpp
 *
 *//* ----------------------------------------------------------------------- */

class TransparentHandle : public AbstractHandle {
public:
    static MemHandleSPtr create(void *inPtr) {
        return MemHandleSPtr(new TransparentHandle(inPtr));
    }

    void *ptr() {
        return mPtr;
    }
    
    MemHandleSPtr clone() const {
        return MemHandleSPtr( new TransparentHandle(*this) );
    }
    
    /**
     * Do nothing.
     */
    void retain() {
    }
    
    /**
     * Do nothing.
     */
    void release() {
    }
    
protected:
    TransparentHandle(void *inPtr)
        : mPtr(inPtr) { }

    void *mPtr;
};
