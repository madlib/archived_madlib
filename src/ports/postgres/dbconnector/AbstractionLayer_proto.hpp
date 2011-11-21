/* ----------------------------------------------------------------------- *//**
 *
 * @file AbstractionLayer.hpp
 *
 * @brief Header file for automatic conversion of PostgreSQL Datums into values
 *
 *//* ----------------------------------------------------------------------- */

class AbstractionLayer {
public:
    class Allocator;
    class AnyType;
    template <typename T> class ArrayHandle;
    template <typename T> class MutableArrayHandle;
    template <typename T> class MutableTransparentHandle;
    template <typename T> class TransparentHandle;

protected:
    class PGException;
    template <typename T> struct TypeBridge;
    template <Oid O> struct TypeForOid;
};
