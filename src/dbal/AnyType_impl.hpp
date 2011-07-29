/* ----------------------------------------------------------------------- *//**
 *
 * @file AnyType_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Iterator for walking through the elements of a compund AnyType.
 */
class AnyType::iterator : public std::iterator<std::input_iterator_tag, AnyType> {
public:
    iterator(const AnyType &inCompositeValue)
        : mCompositeValue(inCompositeValue),
          mCurrentID(0),
          mLastValue(),
          mLastID(-1) { }
    
    iterator &operator++() {
        ++mCurrentID;
        return *this;
    }
    
    // Post-increment operator
    iterator operator++(int) {
        iterator temp(*this);
        operator++();
        return temp;
    }
    
    bool operator==(const iterator &inRHS) {
        return mCurrentID == inRHS.mCurrentID;
    }
    
    bool operator!=(const iterator &inRHS) {
        return mCurrentID != inRHS.mCurrentID;
    }
    
    bool operator<(const iterator &inRHS) {
        return mCurrentID < inRHS.mCurrentID;
    }
    
    AnyType operator*() {
        updateLastValue();
        return mLastValue;
    }
    
    AnyType *operator->() {
        updateLastValue();
        return &mLastValue;
    }
        
private:
    void updateLastValue() {
        if (mLastID != mCurrentID) {
            mLastValue = mCompositeValue[mCurrentID];
            mLastID = mCurrentID;
        }
    };

    const AnyType &mCompositeValue;
    int mCurrentID;
    
    AnyType mLastValue;
    int mLastID;
};


/**
 * @brief Try to convert this variable into whatever type is requested
 *
 * @note A templated conversion operator is not without issues, and makes it
 *     possible to use AnyType values in abusive ways. For instance,
 *     <tt>if(anyValue)</tt> will convert anyValue into bool, which is probably
 *     not the intended semantic. See, e.g.,
 *     http://www.artima.com/cppsource/safebool.html for how this problem is
 *     usually dealt with when only <tt>operator bool</tt> is needed. Here,
 *     we assume the benefit of a universal conversion operator is higher than
 *     the danger of misuse. In general, AnyType should only be used for
 *     retrieving function arguments and return values. They are not designed
 *     to be used within algorithms.
 */
template <class T>
AnyType::operator T() const {
    if (!mDelegate)
        throw std::logic_error("Cannot typecast Null.");
    
    return mDelegate->getAs( static_cast<T*>(NULL) /* ignored type parameter */ );
}
