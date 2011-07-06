/* ----------------------------------------------------------------------- *//**
 *
 * @file PGNewDelete.cpp
 *
 * @brief Overloading operator new and operator delete.
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/PGAllocator.hpp>

using madlib::dbconnector::PGAllocator;

/*
 * We override global storage allocation and deallocation functions. See header
 * file <new> and §18.4.1 of the C++ Standard.
 */

/**
 * @brief operator new for PostgreSQL. Throw on fail.
 *
 * The allocation function (3.7.3.1) called by a new-expression (5.3.4) to
 * allocate size bytes of storage suitably aligned to represent any object of
 * that size.
 */
void *operator new(std::size_t size) throw (std::bad_alloc) {
    return PGAllocator::defaultAllocator().allocate(size);
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
 * @brief operator delete for PostgreSQL. Throw on fail.
 *
 * The deallocation function (3.7.3.2) called by a delete-expression to render
 * the value of ptr invalid.
 */
void operator delete(void *ptr) throw() {
    PGAllocator::defaultAllocator().free(ptr);
}

/**
 * @brief operator new for PostgreSQL. Never throws.
 *
 * Same as above, except that it is called by a placement version of a
 * new-expression when a C++ program prefers a null pointer result as an error
 * indication, instead of a bad_alloc exception.
 */
void *operator new(std::size_t size, const std::nothrow_t &ignored) throw() {
    return PGAllocator::defaultAllocator().allocate(size, ignored);
}

/**
 * @brief operator delete for PostgreSQL. Never throws.
 */
void operator delete(void *ptr, const std::nothrow_t&) throw() {
    PGAllocator::defaultAllocator().free(ptr);
}
