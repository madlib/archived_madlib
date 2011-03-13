/* ----------------------------------------------------------------------- *//**
 *
 * @file PGReturnValue.cpp
 *
 * @brief Automatic conversion of return values into PostgreSQL Datums
 *
 *//* ----------------------------------------------------------------------- */

#include <madlib/madlib.hpp>

#include <madlib/ports/postgres/PGReturnValue.hpp>
#include <madlib/utils/memory.hpp>

extern "C" {
    #include <catalog/pg_type.h>
    #include <funcapi.h>
    #include <utils/lsyscache.h>
    #include <utils/typcache.h>
}

namespace madlib {

namespace ports {

namespace postgres {

using dbal::AbstractValue;
using dbal::AbstractValueSPtr;
using utils::memory::ArrayDeleter;
using utils::memory::NoDeleter;

PGReturnValue::PGReturnValue(const FunctionCallInfo inFCInfo,
    const AbstractValue &inValue)
    : ValueConverter<Datum>(inValue), mTupleDesc(NULL), mTypeID(0) {
    
    TypeFuncClass funcClass = get_call_result_type(inFCInfo, &mTypeID,
        &mTupleDesc);
    
    if (!mValue.isCompound() && funcClass == TYPEFUNC_COMPOSITE)
        throw std::logic_error("Internal function does not provide compound "
            "type expected by SQL function");
    
    if (mValue.isCompound() && funcClass != TYPEFUNC_COMPOSITE)
        throw std::logic_error("SQL function or context does not accept "
            "compound type");
}

PGReturnValue::PGReturnValue(Oid inTypeID,
    const AbstractValue &inValue)
    : ValueConverter<Datum>(inValue), mTupleDesc(NULL), mTypeID(inTypeID) {
    
    if (type_is_rowtype(inTypeID)) {
        if (!mValue.isCompound())
            throw std::logic_error("Internal function does not provide "
                "compound type expected by SQL function");
        
        // Don't ereport errors. We set typmod < 0, and this should not cause
        // an error because compound types in another compund can never be
        // transient. (I think)
        mTupleDesc = lookup_rowtype_tupdesc_noerror(inTypeID, -1, true);
    } else {
        if (mValue.isCompound())
            throw std::logic_error("SQL function or context does not accept "
                "compound type");
    }
}
/*
PGReturnValue::operator Datum() {
    if (!mDatumInitialized) {
        // This does a call-back to our convert(), overloaded for all
        // ConcreteValue<...> subclasses
        mValue.convert(*this);
        mDatumInitialized = true;
    }
        
    return mDatum;
}*/

void PGReturnValue::convert(const AnyValueVector &inRecord) {
    if (!mValue.isCompound())
        throw std::logic_error("Internal MADlib error, got internal compound "
            "type where not expected");

    if (size_t(mTupleDesc->natts) != inRecord.size())
        throw std::logic_error("Number of elements in record expected by SQL "
            "function does not match number of elements provided internally");
    
    shared_ptr<Datum> resultDatum(
        new Datum[mTupleDesc->natts], ArrayDeleter<Datum>());
    shared_ptr<bool> resultDatumIsNull(
        new bool[mTupleDesc->natts], ArrayDeleter<bool>());

    for (int i = 0; i < mTupleDesc->natts; i++) {
        resultDatum.get()[i] = PGReturnValue(
            mTupleDesc->attrs[i]->atttypid, inRecord[i]);
        resultDatumIsNull.get()[i] = inRecord[i].isNull();
    }
    
    HeapTuple heapTuple = heap_form_tuple(mTupleDesc, resultDatum.get(),
        resultDatumIsNull.get());
    
    mConvertedValue = HeapTupleGetDatum(heapTuple);
    mDatumInitialized = true;
}

void PGReturnValue::convert(const double &inValue) {
    switch (mTypeID) {
        case FLOAT8OID: mConvertedValue = Float8GetDatum(inValue); break;
        default: throw std::logic_error(
            "Internal return type does not match SQL return type");
    }
}

void PGReturnValue::convert(const float &inValue) {
    switch (mTypeID) {
        case FLOAT8OID: mConvertedValue = Float8GetDatum(inValue); break;
        case FLOAT4OID: mConvertedValue = Float4GetDatum(inValue); break;
        default: throw std::logic_error(
            "Internal return type does not match SQL return type");
    }
}

/**
 * @brief Convert from int32_t to postgres Datum
 *
 * We only accept lossless conversion. Therefore, acceptable conversion targets
 * are:
 * - Integers of at least 32 bit
 * - Floating Point numbers with significand (mantissa) precision at least
 *   32 bit
 */
void PGReturnValue::convert(const int32_t &inValue) {
    switch (mTypeID) {
        case INT8OID: mConvertedValue = Int64GetDatum(inValue); break;
        case INT4OID: mConvertedValue = Int32GetDatum(inValue); break;
        case FLOAT8OID: mConvertedValue = Float8GetDatum(inValue); break;
        default: throw std::logic_error(
            "Internal return type does not match SQL return type");
    }
}

} // namespace postgres

} // namespace ports

} // namespace madlib
