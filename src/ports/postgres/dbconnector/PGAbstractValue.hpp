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

class PGAbstractValue : public AbstractValue {
protected:
    AbstractValueSPtr getValueByID(unsigned int inID) const = 0;
    AbstractValueSPtr DatumToValue(bool inMemoryIsWritable, Oid inTypeID, Datum inDatum) const;
};

} // namespace dbconnector

} // namespace madlib

#endif
