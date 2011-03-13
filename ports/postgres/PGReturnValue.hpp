/* ----------------------------------------------------------------------- *//**
 *
 * @file PGReturnValue.hpp
 *
 * @brief Header file for automatic conversion of return values into PostgreSQL
 *        Datums
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_PGRETURNVALUE_HPP
#define MADLIB_PGRETURNVALUE_HPP

#include <madlib/madlib.hpp>

#include <madlib/dbal/AbstractValue.hpp>
#include <madlib/dbal/ConcreteValue.hpp>
#include <madlib/dbal/ValueConverter.hpp>

extern "C" {
    #include <postgres.h>
    #include <fmgr.h>
    #include <access/tupdesc.h> // TupleDesc
} // extern "C"


namespace madlib {

namespace ports {

namespace postgres {

using dbal::AbstractValue;
//using dbal::ConcreteValue;
//using dbal::ConcreteRecord;
//using dbal::AbstractValueConverter;
using dbal::AnyValueVector;
using dbal::ValueConverter;


class PGReturnValue : public ValueConverter<Datum> {
public:
    PGReturnValue(const FunctionCallInfo inFCInfo,
        const AbstractValue &inValue);
    
    PGReturnValue(Oid inTypeID, const AbstractValue &inValue);
    
    ~PGReturnValue() {
        if (mTupleDesc != NULL)
            ReleaseTupleDesc(mTupleDesc);
    }
    
    void convert(const double &inValue);
    void convert(const float &inValue);
    void convert(const int32_t &inValue);
    void convert(const AnyValueVector &inRecord);
    
//    operator Datum();

protected:
//    const AbstractValue &mValue;
    TupleDesc mTupleDesc;
//    Datum mDatum;
//    bool mDatumInitialized;
    Oid mTypeID;
};

} // namespace postgres

} // namespace ports

} // namespace madlib

#endif
