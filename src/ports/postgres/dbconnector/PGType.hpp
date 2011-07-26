/* ----------------------------------------------------------------------- *//**
 *
 * @file PGValue.hpp
 *
 * @brief Header file for automatic conversion of PostgreSQL Datums into values
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_PGVALUE_HPP
#define MADLIB_POSTGRES_PGVALUE_HPP

#include <dbconnector/PGCommon.hpp>
#include <dbconnector/PGAbstractType.hpp>

extern "C" {
    #include <fmgr.h>           // for FunctionCallInfo
    #include <access/htup.h>    // for HeapTupleHeader
}

namespace madlib {

namespace dbconnector {

using dbal::AbstractType;
using dbal::AbstractTypeSPtr;


template <typename T>
class PGValue;

/**
 * @brief PostgreSQL function-argument value class
 *
 * Implements PGAbstractType for the "virtual" composite value consisting of
 * all function arguments (as opposed "normal" composite values).
 */
template <>
class PGValue<FunctionCallInfo> : public PGAbstractType {
public:
    PGValue<FunctionCallInfo>(const FunctionCallInfo inFCinfo)
        : fcinfo(inFCinfo) { }
    
protected:
    AbstractTypeSPtr getValueByID(unsigned int inID) const;
    
    AbstractTypeSPtr clone() const {
        return AbstractTypeSPtr( new PGValue<FunctionCallInfo>(*this) );
    }
    
private:
    /**
     * The name is chosen so that PostgreSQL macros like PG_NARGS can be
     * used.
     */
    const FunctionCallInfo fcinfo;
};

/**
 * @brief PostgreSQL tuple-element value class
 *
 * Implements PGAbstractType for "normal" composite values (as opposed to the
 * "virtual" composite value consisting of all function arguments).
 */
template <>
class PGValue<HeapTupleHeader> : public PGAbstractType {
public:    
    PGValue<HeapTupleHeader>(HeapTupleHeader inTuple)
        : mTuple(inTuple) { }

protected:
    AbstractTypeSPtr getValueByID(unsigned int inID) const;

    AbstractTypeSPtr clone() const {
        return AbstractTypeSPtr( new PGValue<HeapTupleHeader>(*this) );
    }

private:    
    const HeapTupleHeader mTuple;
};

} // namespace dbconnector

} // namespace madlib

#endif
