/* ----------------------------------------------------------------------- *//**
 *
 * @file PGValue.cpp
 *
 * @brief Automatic conversion of PostgreSQL Datums into values
 *
 *//* ----------------------------------------------------------------------- */

#include <madlib/dbal/AbstractValue.hpp>
#include <madlib/dbal/Null.hpp>
#include <madlib/ports/postgres/PGValue.hpp>

#include <stdexcept>

extern "C" {
    #include <utils/typcache.h>
    #include <executor/executor.h>
}


namespace madlib {

namespace ports {

namespace postgres {

using dbal::AbstractValueSPtr;
using dbal::Null;


AbstractValueSPtr PGValue<FunctionCallInfo>::getValueByID(unsigned int inID) const {
    if (fcinfo == NULL)
        throw std::invalid_argument("fcinfo is NULL");

    if (inID >= size_t(PG_NARGS()))
        throw std::out_of_range("Access behind end of argument list");

    if (PG_ARGISNULL(inID))
        return Null::sptr();
    
    Oid typeID = get_fn_expr_argtype(fcinfo->flinfo, inID);
    if (typeID == InvalidOid)
        throw std::invalid_argument("Cannot determine argument type");

    AbstractValueSPtr value = DatumToValue(typeID, PG_GETARG_DATUM(inID));
    if (!value)
        throw std::invalid_argument(
            "Internal argument type does not match SQL argument type");
    
    return value;
}

AbstractValueSPtr PGValue<HeapTupleHeader>::getValueByID(unsigned int inID) const {
    if (mTuple == NULL)
        throw std::invalid_argument("Pointer to tuple data is invalid");
    
    if (inID >= HeapTupleHeaderGetNatts(mTuple))
        throw std::out_of_range("Access behind end of tuple");
    
//    if (att_isnull(inID, mTuple->t_bits))
//        throw std::invalid_argument("Tuple item is NULL");

    Oid tupType = HeapTupleHeaderGetTypeId(mTuple);
	int32 tupTypmod = HeapTupleHeaderGetTypMod(mTuple);
    TupleDesc tupDesc = lookup_rowtype_tupdesc(tupType, tupTypmod);
    Oid typeID = tupDesc->attrs[inID]->atttypid;

    bool isNull;
    Datum datum = GetAttributeByNum(mTuple, inID, &isNull);

    if (isNull)
        throw std::invalid_argument("Tuple item is NULL");
    
    ReleaseTupleDesc(tupDesc);

    AbstractValueSPtr value = DatumToValue(typeID, datum);
    if (!value)
        throw std::invalid_argument(
            "Internal argument type does not match SQL argument type");
    
    return value;
}

} // namespace postgres

} // namespace ports

} // namespace madlib
