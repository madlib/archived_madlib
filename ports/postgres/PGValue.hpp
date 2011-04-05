/* ----------------------------------------------------------------------- *//**
 *
 * @file PGValue.hpp
 *
 * @brief Header file for automatic conversion of PostgreSQL Datums into values
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRESQLVALUE_HPP
#define MADLIB_POSTGRESQLVALUE_HPP

#include <madlib/ports/postgres/postgres.hpp>
#include <madlib/ports/postgres/AbstractPGValue.hpp>

extern "C" {
    #include <fmgr.h>           // for FunctionCallInfo
    #include <access/htup.h>    // for HeapTupleHeader
}

namespace madlib {

namespace ports {

namespace postgres {

using dbal::AbstractValue;
using dbal::AbstractValueSPtr;


template <typename T>
class PGValue;

template <>
class PGValue<FunctionCallInfo> : public AbstractPGValue {
public:
    PGValue<FunctionCallInfo>(const FunctionCallInfo inFCinfo)
        : fcinfo(inFCinfo) { }
    
protected:
    AbstractValueSPtr getValueByID(unsigned int inID) const;
    
    AbstractValueSPtr clone() const {
        return AbstractValueSPtr( new PGValue<FunctionCallInfo>(*this) );
    }
    
private:
    /**
     * The name is chosen so that PostgreSQL macros like PG_NARGS can be
     * used.
     */
    const FunctionCallInfo fcinfo;
};

template <>
class PGValue<HeapTupleHeader> : public AbstractPGValue {
public:    
    PGValue<HeapTupleHeader>(HeapTupleHeader inTuple)
        : mTuple(inTuple) { }

protected:
    AbstractValueSPtr getValueByID(unsigned int inID) const;

    AbstractValueSPtr clone() const {
        return AbstractValueSPtr( new PGValue<HeapTupleHeader>(*this) );
    }

private:    
    const HeapTupleHeader mTuple;
};

} // namespace postgres

} // namespace ports

} // namespace madlib

#endif
