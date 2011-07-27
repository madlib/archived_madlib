/* ----------------------------------------------------------------------- *//**
 *
 * @file PGType.hpp
 *
 * @brief Header file for automatic conversion of PostgreSQL Datums into values
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_PGTYPE_HPP
#define MADLIB_POSTGRES_PGTYPE_HPP

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
class PGType;

/**
 * @brief PostgreSQL function-argument value class
 *
 * Implements PGAbstractType for the "virtual" composite value consisting of
 * all function arguments (as opposed "normal" composite values).
 */
template <>
class PGType<FunctionCallInfo> : public PGAbstractType {
public:
    PGType<FunctionCallInfo>(const FunctionCallInfo inFCinfo)
        : fcinfo(inFCinfo) { }
    
protected:
    AbstractTypeSPtr getValueByID(uint16_t inID) const;
    
    AbstractTypeSPtr clone() const {
        return AbstractTypeSPtr( new PGType<FunctionCallInfo>(*this) );
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
class PGType<HeapTupleHeader> : public PGAbstractType {
public:    
    PGType<HeapTupleHeader>(HeapTupleHeader inTuple)
        : mTuple(inTuple) { }

protected:
    AbstractTypeSPtr getValueByID(uint16_t inID) const;

    AbstractTypeSPtr clone() const {
        return AbstractTypeSPtr( new PGType<HeapTupleHeader>(*this) );
    }

private:    
    const HeapTupleHeader mTuple;
};

} // namespace dbconnector

} // namespace madlib

#endif
