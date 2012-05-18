/* ----------------------------------------------------------------------- *//**
 *
 * @file Reference.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_REFERENCE_HPP
#define MADLIB_REFERENCE_HPP

namespace madlib {

namespace utils {

/**
 * @brief A reference to type T that masquerades as being of type U.
 *
 * An an example, a masqueading reference could be used to use a double variable
 * to store an integer value (which might be the most efficient solution to
 * store a composite type of floating-point and integer values as a double
 * array).
 */
template <typename T, typename U = T>
class Reference {
public:
    Reference() : mPtr(NULL) { }

    // FIXME: Is it better to have the const_cast in the mutable class
    // and declare mPtr as const?
    Reference(const T *inPtr) : mPtr(const_cast<T*>(inPtr)) { }
    
    Reference &rebind(const T *inPtr) {
        mPtr = const_cast<T*>(inPtr);
        return *this;
    }

    /**
     * @brief Return the value pointed to by the reference as type U
     */
    operator U() const {
        return static_cast<U>(*mPtr);
    }
    
    const T* ptr() const {
        return mPtr;
    }
    
protected:
    /**
     * Defined but protected.
     */
    Reference &operator=(const Reference &inReference) {
        return *this;
    }

    T *mPtr;
};

template <typename T, typename U = T>
class MutableReference : public Reference<T, U> {
    typedef Reference<T, U> Base;
public:
    MutableReference() { }

    MutableReference(T *inPtr) : Base(inPtr) { }

    /**
     * @internal
     * It is important to define this operator because C++ will otherwise
     * perform an assignment as a bit-by-bit copy. Note that this default 
     * operator= would be used even though there is a conversion path
     * through dest.operator=(orig.operator U())
     */
    MutableReference &operator=(const Base &inReference) {
        *mPtr = *inReference.ptr();
        return *this;
    }
    
    MutableReference &operator=(const U &inValue) {
        *mPtr = inValue;
        return *this;
    }
    
    MutableReference &operator+=(const U &inValue) {
        *mPtr += inValue;
        return *this;
    }

    MutableReference &operator-=(const U &inValue) {
        *mPtr -= inValue;
        return *this;
    }
    
    U operator++(int) {
        U returnValue = static_cast<U>(*mPtr);
        *mPtr += 1;
        return returnValue;
    }
    
    MutableReference &operator++() {
        *mPtr += 1;
        return *this;
    }
    
    operator T&() {
        return *mPtr;
    }
    
protected:
    using Base::mPtr;
};


} // namespace modules

} // namespace regress

#endif
