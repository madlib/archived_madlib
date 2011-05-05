/* ----------------------------------------------------------------------- *//**
 *
 * @file AbstractPGValue.cpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_ABSTRACTPGVALUE_HPP
#define MADLIB_ABSTRACTPGVALUE_HPP

#include <madlib/ports/postgres/postgres.hpp>

namespace madlib {

namespace ports {

namespace postgres {

class AbstractPGValue : public AbstractValue {
protected:
    AbstractValueSPtr getValueByID(unsigned int inID) const = 0;
    AbstractValueSPtr DatumToValue(bool inMemoryIsWritable, Oid inTypeID, Datum inDatum) const;
};

} // namespace postgres

} // namespace ports

} // namespace madlib

#endif
