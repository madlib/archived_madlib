/* ----------------------------------------------------------------------- *//**
 *
 * @file TransparentHandle_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

// Workaround for Doxygen: Ignore if not included by dbconnector.hpp
#ifdef MADLIB_DBCONNECTOR_HPP

/**
 * @brief Handle without any meta data (essentially, a constant pointer)
 *
 * A TransparentHandle is simply a (constant) pointer. It is used whenever we
 * need a type that conforms to the handle policy, but no meta data is required.
 */
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

/**
 * @brief Mutable handle without any meta data (essentially, a pointer)
 */
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

#endif // MADLIB_DBCONNECTOR_HPP (workaround for Doxygen)
