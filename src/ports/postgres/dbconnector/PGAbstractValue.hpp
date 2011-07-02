/* ----------------------------------------------------------------------- *//**
 *
 * @file PGAbstractValue.cpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_PGABSTRACTVALUE_HPP
#define MADLIB_POSTGRES_PGABSTRACTVALUE_HPP

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
 * PGAbstractValue is the common superclass that contains common parts.
 */
class PGAbstractValue : public AbstractValue {
protected:
    AbstractValueSPtr getValueByID(unsigned int inID) const = 0;
    AbstractValueSPtr DatumToValue(bool inMemoryIsWritable, Oid inTypeID, Datum inDatum) const;
};

} // namespace dbconnector

} // namespace madlib

#endif
