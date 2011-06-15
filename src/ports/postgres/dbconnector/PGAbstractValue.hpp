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
 * A PGAbstractValue can either be the list of function arguments
 * (PGValue<FunctionCallInfo>) or a record type (PGValue<HeapTupleHeader>).
 * By only using the interface presented by PGAbstractValue, it is possible to
 * treat the list of function arguments as just one tuple value (which is a
 * recursive structure and its elements can again be tuples).
 */
class PGAbstractValue : public AbstractValue {
protected:
    AbstractValueSPtr getValueByID(unsigned int inID) const = 0;
    AbstractValueSPtr DatumToValue(bool inMemoryIsWritable, Oid inTypeID, Datum inDatum) const;
};

} // namespace dbconnector

} // namespace madlib

#endif
