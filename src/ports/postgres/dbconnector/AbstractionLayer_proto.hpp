/* ----------------------------------------------------------------------- *//**
 *
 * @file AbstractionLayer.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief PostgreSQL database interface
 *
 * The main issue when writing plug-in code for PostgreSQL in C++ is that
 * exceptions in PostgreSQL are implemented using longjmp.
 *
 * Since we must not leave the confines of C++'s defined behavior, we insist
 * on proper stack unwinding and thus surround any access of the database
 * backend with \c PG_TRY()/\c PG_CATCH() macros.\n
 * 
 * We never leave a \c PG_TRY()-block through:
 * - A return call
 * - A C++ exception
 * 
 * Moreover, in a \c PG_TRY()-block we do not
 * - Declare or allocate variables
 * - Call functions that might violate one of the above rules
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
