/* ----------------------------------------------------------------------- *//**
 *
 * @file operatorNewDelete.cpp
 *
 * @brief Overloading operator new and operator delete.
 *
 *//* ----------------------------------------------------------------------- */

#include <madlib/ports/postgres/PGAllocator.hpp>

using madlib::ports::postgres::PGAllocator;

/**
 * The default allocator used by operator new and operator delete. It is not
 * supposed to be used directly or by any other function.
 */
static PGAllocator sDefaultAllocator;

/*
 * We override global storage allocation and deallocation functions. See header
 * file <new> and §18.4.1 of the C++ Standard.
 */

/**
 * The allocation function (3.7.3.1) called by a new-expression (5.3.4) to
 * allocate size bytes of storage suitably aligned to represent any object of
 * that size.
 */
void *operator new(std::size_t size) throw (std::bad_alloc) {
    return sDefaultAllocator.allocate(size);
}

/*
 * The default behavior of
 *
 *   void *operator new[](std::size_t inSize) throw (std::bad_alloc)
 *   void *operator new[](std::size_t, const std::nothrow_t&) throw()
 *   void operator delete[](void *inPtr) throw()
 *   void operator delete[](void *inPtr, const std::nothrow_t&) throw()
 *
 * is to call the non-array variants (18.4.1.2). Hence no need to override.
 */

/**
 * The deallocation function (3.7.3.2) called by a delete-expression to render
 * the value of ptr invalid.
 */
void operator delete(void *ptr) throw() {
    sDefaultAllocator.free(ptr);
}

/**
 * Same as above, except that it is called by a placement version of a
 * new-expression when a C++ program prefers a null pointer result as an error
 * indication, instead of a bad_alloc exception.
 */
void *operator new(std::size_t size, const std::nothrow_t &ignored) throw() {
    return sDefaultAllocator.allocate(size, ignored);
}

/**
 * Same as above.
 */
void operator delete(void *ptr, const std::nothrow_t&) throw() {
    sDefaultAllocator.free(ptr);
}
