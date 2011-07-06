/* ----------------------------------------------------------------------- *//**
 *
 * @file TransparentHandle.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief The most simple handle type -- a normal pointer
 *
 * A transparent handle is used where a handle is required but a normal pointer
 * is sufficient. A transparent handle never owns the memory it points to.
 */
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
        
protected:
    TransparentHandle(void *inPtr)
        : mPtr(inPtr) { }

    void *mPtr;
};
