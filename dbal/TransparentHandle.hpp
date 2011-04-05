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
        
protected:
    TransparentHandle(void *inPtr)
        : mPtr(inPtr) { }

    void *mPtr;
};
