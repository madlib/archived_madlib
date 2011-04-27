/* ----------------------------------------------------------------------- *//**
 *
 * @file Reference.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_REFERENCE_HPP
#define MADLIB_REFERENCE_HPP

namespace madlib {

namespace utils {

template <typename T, typename U = T>
class Reference {
public:
    Reference(T *inPtr) {
        mPtr = inPtr;
    }
    
    Reference &rebind(T *inPtr) {
        mPtr = inPtr;
        return *this;
    }

    operator U() const {
        return static_cast<U>(*mPtr);
    }
    
    Reference &operator=(const U &inValue) {
        *mPtr = inValue;
        return *this;
    }
    
    Reference &operator+=(const U &inValue) {
        *mPtr += inValue;
        return *this;
    }

    Reference &operator-=(const U &inValue) {
        *mPtr -= inValue;
        return *this;
    }
    
    U operator++(int) {
        U returnValue = static_cast<U>(*mPtr);
        *mPtr += 1;
        return returnValue;
    }
    
    Reference &operator++() {
        *mPtr += 1;
        return *this;
    }
        
protected:
    T *mPtr;
};


} // namespace modules

} // namespace regress

#endif
