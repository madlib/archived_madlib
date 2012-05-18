/* ----------------------------------------------------------------------- *//**
 *
 * @file NewDelete.cpp
 *
 * @brief Global overrides of operator new and operator delete.
 *
 * We override the C++ global memory allocation and deallocation functions. We
 * map them to ultimately use the PostgreSQL memory routines to protect against
 * memory leaks.
 * See header file \c <new> and §18.4.1 of the C++ Standard.
 *
 * @note
 *     This is merely a precaution. C++ objects should still be properly
 *     deallocated. We still make the promise to user code that all destructors
 *     will be properly called.
 *
 * @internal
 *     Declaring operator new and delete as inline and making this file a
 *     header file caused crahes when compiled with GCC 4.6. So be careful if
 *     you want to revert the decision to put the global operator new/delete
 *     in its own translation unit.
 *
 *
 * The default behavior of
 *
 * - <tt>void *operator new[](std::size_t inSize) throw (std::bad_alloc)</tt>
 * - <tt>void *operator new[](std::size_t, const std::nothrow_t&) throw()</tt>
 * - <tt>void operator delete[](void *inPtr) throw()</tt>
 * - <tt>void operator delete[](void *inPtr, const std::nothrow_t&) throw()</tt>
 *
 * is to call the non-array variants (18.4.1.2). Hence no need to override.
 *
 *//* ----------------------------------------------------------------------- */

// We do not write #include "dbconnector.hpp" here because we want to rely on
// the search paths, which might point to a port-specific dbconnector.hpp
#include <dbconnector/dbconnector.hpp>

/**
 * @brief operator new for PostgreSQL. Throw on fail.
 *
 * The allocation function (3.7.3.1) called by a new-expression (5.3.4) to
 * allocate size bytes of storage suitably aligned to represent any object of
 * that size.
 */
void*
operator new(std::size_t size) throw (std::bad_alloc) {
    return madlib::defaultAllocator().allocate<
        madlib::dbal::FunctionContext,
        madlib::dbal::DoNotZero,
        madlib::dbal::ThrowBadAlloc>(size);
}

/**
 * @brief operator delete for PostgreSQL. Never throws.
 *
 * The deallocation function (3.7.3.2) called by a delete-expression to render
 * the value of ptr invalid.
 * The value of ptr is null or the value returned by an earlier call to
 * <tt>operator new(std::size_t)</tt>.
 */
void
operator delete(void *ptr) throw() {
    madlib::defaultAllocator().free<madlib::dbal::FunctionContext>(ptr);
}

/**
 * @brief operator new for PostgreSQL. Never throws.
 *
 * Same as above, except that it is called by a placement version of a
 * new-expression when a C++ program prefers a null pointer result as an error
 * indication, instead of a bad_alloc exception.
 */
void*
operator new(std::size_t size, const std::nothrow_t&) throw() {
    return madlib::defaultAllocator().allocate<
        madlib::dbal::FunctionContext,
        madlib::dbal::DoNotZero,
        madlib::dbal::ReturnNULL>(size);
}

/**
 * @brief operator delete for PostgreSQL. Never throws.
 *
 * The value of ptr is null or the value returned by an earlier call to
 * <tt>operator new(std::size_t, const std::nothrow_t&)</tt>.
 */
void
operator delete(void *ptr, const std::nothrow_t&) throw() {
    madlib::defaultAllocator().free<madlib::dbal::FunctionContext>(ptr);
}
