/* ----------------------------------------------------------------------- *//**
 *
 * @file PGValue.cpp
 *
 * @brief Automatic conversion of PostgreSQL Datums into values
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/PGCompatibility.hpp>
#include <dbconnector/PGValue.hpp>

#include <stdexcept>

extern "C" {
    #include <utils/typcache.h>
    #include <executor/executor.h>
}


namespace madlib {

namespace dbconnector {

using namespace dbal;

/**
 * @brief Convert the <tt>inID</tt>-th function argument to a DBAL object
 */
AbstractTypeSPtr PGValue<FunctionCallInfo>::getValueByID(unsigned int inID) const {
    if (fcinfo == NULL)
        throw std::invalid_argument("fcinfo is NULL");

    if (inID >= size_t(PG_NARGS()))
        throw std::out_of_range("Access behind end of argument list");

    if (PG_ARGISNULL(inID))
        return AbstractTypeSPtr(new AnyType(Null()));
    
    bool exceptionOccurred = false;
    Oid typeID;
    bool writable;
    
    PG_TRY(); {
        typeID = get_fn_expr_argtype(fcinfo->flinfo, inID);

        // If we are called as an aggregate function, the first argument is the
        // transition state. In that case, we are free to modify the data.
        // In fact, for performance reasons, we *should* even do all modifications
        // in-place. In all other cases, directly modifying memory is dangerous.
        // See warning at:
        // http://www.postgresql.org/docs/current/static/xfunc-c.html#XFUNC-C-BASETYPE
        writable = (inID == 0 && AggCheckCallContext(fcinfo, NULL));
    } PG_CATCH(); {
        exceptionOccurred = true;
    } PG_END_TRY();
    
    BOOST_ASSERT_MSG(exceptionOccurred == false, "An exception occurred while "
        "gathering inormation about PostgreSQL function arguments");
    
    if (typeID == InvalidOid)
        throw std::invalid_argument("Cannot determine function argument type");

    AbstractTypeSPtr value = DatumToValue(writable, typeID, PG_GETARG_DATUM(inID));
    if (!value)
        throw std::invalid_argument(
            "Internal argument type does not match SQL argument type");
    
    return value;
}

/**
 * @brief Convert the <tt>inID</tt>-th tuple element to a DBAL object
 */
AbstractTypeSPtr PGValue<HeapTupleHeader>::getValueByID(unsigned int inID) const {
    if (mTuple == NULL)
        throw std::invalid_argument("Pointer to tuple data is invalid");
    
    if (inID >= HeapTupleHeaderGetNatts(mTuple))
        throw std::out_of_range("Access behind end of tuple");
    
    bool exceptionOccurred = false;
    Oid tupType;
	int32 tupTypmod;
    TupleDesc tupDesc;
    Oid typeID;
    bool isNull = false;
    Datum datum;
    
    PG_TRY(); {
        tupType = HeapTupleHeaderGetTypeId(mTuple);
        tupTypmod = HeapTupleHeaderGetTypMod(mTuple);
        tupDesc = lookup_rowtype_tupdesc(tupType, tupTypmod);
        typeID = tupDesc->attrs[inID]->atttypid;
        ReleaseTupleDesc(tupDesc);
        datum = GetAttributeByNum(mTuple, inID, &isNull);
    } PG_CATCH(); {
        exceptionOccurred = true;
    } PG_END_TRY();
    
    BOOST_ASSERT_MSG(exceptionOccurred == false, "An exception occurred while "
        "gathering inormation about a PostgreSQL tuple value");

    if (isNull)
        throw std::invalid_argument("Tuple item is NULL");
    
    AbstractTypeSPtr value = DatumToValue(false /* memory is not writable */,
        typeID, datum);
    if (!value)
        throw std::invalid_argument(
            "Internal argument type does not match SQL argument type");
    
    return value;
}

} // namespace dbconnector

} // namespace madlib
