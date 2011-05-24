/* ----------------------------------------------------------------------- *//**
 *
 * @file Array_const.hpp
 *
 * @brief The MADlib Array class -- a thin wrapper around
 *        boost::const_multi_array_ref
 *
 * @internal Unfortunately, we have some duplicate code from Array.hpp. 
 *      A clean solution would to have Array_const a superclass of Array.
 *      However, \c const_multi_array_ref is not a virtual superclass of
 *      \c multi_array_ref, so this is not feasible.
 *
 *//* ----------------------------------------------------------------------- */

template <typename T, std::size_t NumDims>
class Array_const : public const_multi_array_ref<T, NumDims> {
public:
    typedef boost::multi_array_types::index index;
    typedef boost::multi_array_types::size_type size_type;
    
    typedef boost::array<index, NumDims> extent_list;
    typedef boost::detail::multi_array::extent_gen<NumDims> extent_gen;

    inline Array_const(
        const Array_const<T, NumDims> &inArray)
        : const_multi_array_ref<T, NumDims>(
            inArray),
          mMemoryHandle(inArray.mMemoryHandle)
        { }

    inline Array_const(
        const Array<T, NumDims> &inArray)
        : const_multi_array_ref<T, NumDims>(
            inArray),
          mMemoryHandle(inArray.memoryHandle())
        { }
        
    inline Array_const(
        const MemHandleSPtr inHandle,
        const extent_gen &ranges)
        : const_multi_array_ref<T, NumDims>(
            static_cast<T*>(inHandle->ptr()),
            ranges),
          mMemoryHandle(inHandle)
        { }
    
    inline Array_const(
        AllocatorSPtr inAllocator,
        const extent_gen &ranges)
        : const_multi_array_ref<T, NumDims>(
            NULL,
            ranges),
          mMemoryHandle(
            inAllocator->allocateArray(getNumElements(ranges),
            static_cast<T*>(NULL) /* pure type parameter */)) {
            
        this->set_base_ptr(mMemoryHandle->ptr());
    }
    
    inline Array_const &rebind(
        const MemHandleSPtr inHandle,
        const extent_gen &ranges) {
        
        mMemoryHandle = inHandle;
        return internalRebind(ranges);
    }
    
    inline Array_const &rebind(
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
    
    inline Array_const &internalRebind(const extent_gen &ranges) {
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
