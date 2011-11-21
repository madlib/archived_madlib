/* ----------------------------------------------------------------------- *//**
 *
 * @file PGMain.cpp
 *
 * @brief PostgreSQL database abstraction layer 
 *
 *//* -------------------------------------------------------------------- *//**
 *
 * Postgres is a platform where the C interface supports reflection, so all we
 * need to do is to include the PostgreSQL database abstraction layer and the
 * default declarations.
 *
 * @par Platform notes
 * We can be sure that all code in the core library will use our overloads:
 * - With the normal POSIX linking (e.g., on Linux), there is only one namespace
 *   for symbols. Since the connector library is loaded before the core library.
 * - On Mac OS X, operator new and delete are exempt from the usual two-level
 *   namespace. See:
 *   http://developer.apple.com/library/mac/#documentation/DeveloperTools/Conceptual/CppRuntimeEnv/Articles/LibCPPDeployment.html
 *   We can therefore be sure that all code in the core library will use our
 *   overloads.
 * - FIXME: Check!
 *   On Solaris, we use direct binding in general, but mark
 *   <tt>operator new</tt> and <tt>delete</tt> as exempt.
 *   http://download.oracle.com/docs/cd/E19253-01/817-1984/aehzq/index.html
 */


#include "dbconnector.hpp"

extern "C" {
    PG_MODULE_MAGIC;
} // extern "C"

#include <modules/regress/regress.hpp>

#undef DECLARE_UDF
#define DECLARE_UDF DECLARE_UDF_EXTERNAL
#include <modules/regress/regress.hpp>

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
    return madlib::defaultAllocator().allocate<
        madlib::dbal::FunctionContext,
        madlib::dbal::DoNotZero,
        madlib::dbal::ThrowBadAlloc>(size);
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
 * @brief operator delete for PostgreSQL. Never throws.
 *
 * The deallocation function (3.7.3.2) called by a delete-expression to render
 * the value of ptr invalid.
 * The value of ptr is null or the value returned by an earlier call to
 * <tt>operator new(std::size_t)</tt>.
 */
void operator delete(void *ptr) throw() {
    madlib::defaultAllocator().free<madlib::dbal::FunctionContext>(ptr);
}

/**
 * @brief operator new for PostgreSQL. Never throws.
 *
 * Same as above, except that it is called by a placement version of a
 * new-expression when a C++ program prefers a null pointer result as an error
 * indication, instead of a bad_alloc exception.
 */
void *operator new(std::size_t size, const std::nothrow_t&) throw() {
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
void operator delete(void *ptr, const std::nothrow_t&) throw() {
    madlib::defaultAllocator().free<madlib::dbal::FunctionContext>(ptr);
}
