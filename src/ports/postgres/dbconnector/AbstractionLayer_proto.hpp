/* ----------------------------------------------------------------------- *//**
 *
 * @file AbstractionLayer.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief PostgreSQL database interface
 *
 * There are two main issues when writing plug-in code for PostgreSQL in C++:
 * -# Exceptions in PostgreSQL are implemented using longjmp.\n
 *    .
 *    Since we must not leave the confines of C++'s defined behavior, we insist
 *    on proper stack unwinding and thus surround any access of the database
 *    backend with \c PG_TRY()/\c PG_CATCH() macros.\n
 *    .
 *    We never leave a \c PG_TRY()-block through:
 *    - A return call
 *    - A C++ exception
 *    .
 *    Moreover, in a \c PG_TRY()-block we do not
 *    - Declare or allocate variables
 *    - Call functions that might violate one of the above rules
 *    .
 * -# Memory leaks are only guaranteed not to occur if PostgreSQL memory
 *    allocation functions are used\n
 *    .
 *    PostgreSQL knows the concept of "memory contexts" such as current function
 *    call, current aggregate function, or current transaction. Memory
 *    allocation using \c palloc() always occurs within a specific memory
 *    context -- and once a memory context goes out of scope all memory
 *    associated with it will be deallocated (garbage collected).\n
 *    .
 *    As a second safety measure we therefore redefine <tt>operator new</tt> and
 *    <tt>operator delete</tt> to call \c palloc() and \c pfree(). (This is
 *    essentially an \b additional protection against leaking C++ code. Given
 *    1., no C++ destructor call will ever be missed.)
 *
 * @see AbstractionLayer::Allocator::internalAllocate, NewDelete.cpp
 *
 *//* ----------------------------------------------------------------------- */
class AbstractionLayer {
public:
    class Allocator;
    class AnyType;
    template <int ErrorLevel> class OutputStream;
    template <typename T> class ArrayHandle;
    template <typename T> class MutableArrayHandle;
    template <typename T> class MutableTransparentHandle;
    template <typename T> class TransparentHandle;

protected:
    class PGException;
    template <typename T> struct TypeTraits;
    template <Oid O> struct TypeForOid;
};
