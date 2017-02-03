/* ----------------------------------------------------------------------- *//**
 *
 * @file MallocAllocator.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MALLOC_ALLOCATOR_HPP
#define MADLIB_MALLOC_ALLOCATOR_HPP

namespace madlib {

namespace utils {

/**
 * @brief An STL allocator that uses malloc for its allocations
 *
 * The declaration of this class is taken from C++ Standard §20.4.1. The
 * allocate() and deallocate() members have been implemented using std::malloc()
 * and std::free().
 */
template <class T>
class MallocAllocator;

// specialize for void:
template <>
class MallocAllocator<void> {
public:
    typedef void* pointer;
    typedef const void* const_pointer;
    // reference-to-void members are impossible.
    typedef void value_type;
    template <class U> struct rebind { typedef MallocAllocator<U> other; };
};
 
template <class T>
class MallocAllocator {
public:
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;
    typedef T* pointer;
    typedef const T* const_pointer;
    typedef T& reference;
    typedef const T& const_reference;
    typedef T value_type;
    template <class U> struct rebind { typedef MallocAllocator<U> other; };
    
    MallocAllocator() throw () { }
    MallocAllocator(const MallocAllocator &) throw () { }
    template <class U> MallocAllocator(const MallocAllocator<U>&) throw () { }
    ~MallocAllocator() throw () { }
    
    pointer address(reference x) const {
        return &x;
    }
    
    const_pointer address(const_reference x) const {
        return &x;
    }
    
    pointer allocate(size_type n,
        MallocAllocator<void>::const_pointer /* hint */ = 0) {
        
        if (n > this->max_size())
            throw std::bad_alloc();
        pointer ptr = static_cast<pointer>(std::malloc(n * sizeof(T)));
        if (!ptr)
            throw std::bad_alloc();
        return ptr;
    }
    
    void deallocate(pointer p, size_type /* n */) {
        std::free(static_cast<void*>(p));
    }
    
    size_type max_size() const throw () {
        return size_type(-1) / sizeof(T);
    }
    
    void construct(pointer p, const T& val) {
        ::new (static_cast<void*>(p)) value_type(val);
    }
    
    void destroy(pointer p) {
        p->~T();
    }
};

template<typename T>
inline bool
operator==(const MallocAllocator<T>&, const MallocAllocator<T>&)
{ return true; }
  
template<typename T>
inline bool
operator!=(const MallocAllocator<T>&, const MallocAllocator<T>&)
{ return false; }

} // namespace utils

} // namespace madlib

#endif
