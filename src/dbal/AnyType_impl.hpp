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
 */
// FIXME: Think whether we need to use a safe conversion idiom like
// http://www.artima.com/cppsource/safebool.html
template <class T>
AnyType::operator T() const {
    AbstractTypeSPtr lastDelegate;
    
    if (!mDelegate)
        throw std::logic_error("Cannot typecast Null.");
    
    return mDelegate->getAs( static_cast<T*>(NULL) /* ignored type parameter */ );
}
