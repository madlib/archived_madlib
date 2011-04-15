/* ----------------------------------------------------------------------- *//**
 *
 * @file Array.hpp
 *
 * @brief The MADlib Array class -- a thin wrapper around boost::multi_array_ref
 *
 *//* ----------------------------------------------------------------------- */

template <typename T, std::size_t NumDims>
class Array : public multi_array_ref<T, NumDims> {
public:
    typedef boost::multi_array_types::index index;
    typedef boost::multi_array_types::size_type size_type;

    typedef boost::array<index, NumDims> extent_list;
    typedef boost::detail::multi_array::extent_gen<NumDims> extent_gen;

    inline Array(
        const Array<T, NumDims> &inArray)
        : multi_array_ref<T, NumDims>(
            inArray),
          mMemoryHandle(inArray.mMemoryHandle)
        { }
        
    inline Array(
        const MemHandleSPtr inHandle,
        const extent_gen &ranges)
        : multi_array_ref<T, NumDims>(
            static_cast<T*>(inHandle->ptr()),
            ranges),
          mMemoryHandle(inHandle)
        { }
    
    inline Array(
        AllocatorSPtr inAllocator,
        const extent_gen &ranges)
        : multi_array_ref<T, NumDims>(
            NULL,
            ranges),
          mMemoryHandle(
            inAllocator->allocateArray(getNumElements(ranges),
            static_cast<T*>(NULL) /* pure type parameter */)) {
            
        this->set_base_ptr(mMemoryHandle->ptr());
    }
    
    /**
     * @brief Perform a deep copy
     *
     * @internal It is important to define this function. Otherwise, C++ will
     *      provide a default implementation that calls
     *      multi_array_ref::operator=() but copies mMemoryHandle bitwise.
     *      Hence, the mMemoryHandle and the array are out of sync.
     *      The correct thing to do os leave mMemoryHandle untouched!
     */
    inline const Array &operator=(const Array &inOtherArray) {
        multi_array_ref<T, NumDims>::operator=(inOtherArray);
        return *this;
    }

    /**
     * @brief Perform a deep copy from Array_const
     */
    inline const Array &operator=(const Array_const<T, NumDims> &inOtherArray) {
        multi_array_ref<T, NumDims>::operator=(inOtherArray);
        return *this;
    }
    
    // FIXME: We might "own" the cloned memory handle, so we should release it.
    // This is not a problem for PostgreSQL, but we need to implement reference
    // counting. Either based on shared_ptr or our own.
/*    inline Array(
        const Array_const<T, NumDims> &inArray)
        : multi_array_ref<T, NumDims>(
            NULL,
            utils::shapeToExtents<NumDims>(inArray.shape())),
          mMemoryHandle(
            inArray.memoryHandle().clone()) {
        
        this->set_base_ptr(mMemoryHandle->ptr());
    }
*/    
    inline Array &rebind(
        const MemHandleSPtr inHandle,
        const extent_gen &ranges) {
        
        mMemoryHandle = inHandle;
        return internalRebind(ranges);
    }
    
    inline Array &rebind(
        AllocatorSPtr inAllocator,
        const extent_gen &ranges) {
        
        mMemoryHandle = inAllocator->allocateArray(getNumElements(ranges),
            static_cast<T*>(NULL) /* pure type parameter */);
        return internalRebind(ranges);
    }
    
    inline MemHandleSPtr memoryHandle() const {
        return mMemoryHandle;
    }

protected:
    static inline extent_list getExtentList(const extent_gen &ranges) {        
        // calculate the extents
        extent_list extents;
        std::transform(ranges.ranges_.begin(),ranges.ranges_.end(),
            extents.begin(),
            boost::mem_fun_ref(&multi_array_ref<T, NumDims>::extent_range::size));
        
        return extents;
    }
    
    static inline size_type getNumElements(const extent_gen &ranges) {
        extent_list extents = getExtentList(ranges);
        return std::accumulate(extents.begin(), extents.end(),
            size_type(1), std::multiplies<size_type>());
    }
    
    inline Array &internalRebind(const extent_gen &ranges) {
        this->set_base_ptr(static_cast<T*>(mMemoryHandle->ptr()));

        // See init_from_extent_gen()
        // get the index_base values
        std::transform(ranges.ranges_.begin(),ranges.ranges_.end(),
                  multi_array_ref<T, NumDims>::index_base_list_.begin(),
                  boost::mem_fun_ref(&multi_array_ref<T, NumDims>::extent_range::start));

        // calculate the extents
        init_multi_array_ref(getExtentList(ranges).begin());
        return *this;
    }
    
protected:
    MemHandleSPtr mMemoryHandle;
};
