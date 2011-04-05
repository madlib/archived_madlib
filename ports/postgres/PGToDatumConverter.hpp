/* ----------------------------------------------------------------------- *//**
 *
 * @file PGToDatumConverter.hpp
 *
 * @brief Header file for automatic conversion of return values into PostgreSQL
 *        Datums
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_PGTODATUMCONVERTER_HPP
#define MADLIB_PGTODATUMCONVERTER_HPP

#include <madlib/ports/postgres/postgres.hpp>

extern "C" {
    #include <postgres.h>
    #include <fmgr.h>
    #include <access/tupdesc.h> // TupleDesc
} // extern "C"


namespace madlib {

namespace ports {

namespace postgres {

class PGToDatumConverter : public ValueConverter<Datum> {
public:
    PGToDatumConverter(const FunctionCallInfo inFCInfo,
        const AbstractValue &inValue);
    
    PGToDatumConverter(Oid inTypeID, const AbstractValue &inValue);
    
    ~PGToDatumConverter() {
        if (mTupleDesc != NULL)
            ReleaseTupleDesc(mTupleDesc);
    }
    
    void convert(const double &inValue);
    void convert(const float &inValue);
    void convert(const int32_t &inValue);
    
    void convert(const Array<double> &inValue);
    void convert(const DoubleCol &inValue);
    
    void convert(const AnyValueVector &inRecord);
    
protected:
    TupleDesc mTupleDesc;
    Oid mTypeID;
};

} // namespace postgres

} // namespace ports

} // namespace madlib

#endif
