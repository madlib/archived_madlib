/* ----------------------------------------------------------------------- *//**
 *
 * @file PGAbstractType.cpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_PGABSTRACTTYPE_HPP
#define MADLIB_POSTGRES_PGABSTRACTTYPE_HPP

#include <dbconnector/PGCommon.hpp>

namespace madlib {

namespace dbconnector {

/**
 * @brief PostgreSQL abstract value class
 * 
 * PGValue<FunctionCallInfo> objects are instantiated for the "virtual"
 * composite value consisting of all function arguments.
 * PGvalue<HeapTupleHeader> objects are instantiated for "normal" composite
 * values.
 * PGAbstractType is the common superclass that contains common parts.
 */
class PGAbstractType : public AbstractType {
protected:
    AbstractTypeSPtr DatumToValue(bool inMemoryIsWritable, Oid inTypeID, Datum inDatum) const;
};

} // namespace dbconnector

} // namespace madlib

#endif
