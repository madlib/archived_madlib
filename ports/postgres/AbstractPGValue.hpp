/* ----------------------------------------------------------------------- *//**
 *
 * @file AbstractPGValue.cpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_ABSTRACTPGVALUE_HPP
#define MADLIB_ABSTRACTPGVALUE_HPP

#include <madlib/dbal/AbstractValue.hpp>

extern "C" {
    #include <postgres.h>   // for Oid, Datum
}

namespace madlib {

namespace ports {

namespace postgres {

using dbal::AbstractValue;
using dbal::AbstractValueSPtr;


class AbstractPGValue : public AbstractValue {
protected:
    AbstractValueSPtr getValueByID(unsigned int inID) const = 0;
    AbstractValueSPtr DatumToValue(Oid inTypeID, Datum inDatum) const;
};

} // namespace postgres

} // namespace ports

} // namespace madlib

#endif
