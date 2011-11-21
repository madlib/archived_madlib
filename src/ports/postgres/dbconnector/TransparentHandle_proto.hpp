template <typename T>
class AbstractionLayer::TransparentHandle {
public:
    enum { isMutable = false };

    TransparentHandle(const T* inPtr)
      : mPtr(const_cast<T*>(inPtr)) { }
    
    const T* ptr() const;
        
protected:
    T *mPtr;
};


template <typename T>
class AbstractionLayer::MutableTransparentHandle
  : public AbstractionLayer::TransparentHandle<T> {

    typedef TransparentHandle<T> Base;
    
public:
    enum { isMutable = true };

    MutableTransparentHandle(T* inPtr)
      : Base(inPtr) { }
    
    // Import the const version as well
    using Base::ptr;
    T* ptr();

protected:
    using Base::mPtr;
};
