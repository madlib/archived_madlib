/* ----------------------------------------------------------------------- *//**
 *
 * @file PGToDatumConverter.hpp
 *
 * @brief Header file for automatic conversion of return values into PostgreSQL
 *        Datums
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_PGTODATUMCONVERTER_HPP
#define MADLIB_POSTGRES_PGTODATUMCONVERTER_HPP

#include <dbconnector/PGCommon.hpp>

extern "C" {
    #include <postgres.h>
    #include <fmgr.h>
    #include <access/tupdesc.h> // TupleDesc
} // extern "C"


namespace madlib {

namespace dbconnector {

/**
 * @brief Convert DBAL types into PostgreSQL Datum
 *
 * @internal This class makes frequent calls into the PostgreSQL backend.
 *     This class is not thread-safe. This should never be an issue, though,
 *     because PostgreSQL does not use threads.
 */
class PGToDatumConverter : public AbstractValueConverter {
public:
    PGToDatumConverter(const FunctionCallInfo inFCInfo);
    
    PGToDatumConverter(Oid inTypeID);
    
    ~PGToDatumConverter() {
        if (mTupleDesc != NULL)
            ReleaseTupleDesc(mTupleDesc);
    }
    
    Datumm convertToDatum(const AbstractValue &inValue);
    
    void convert(const double &inValue);
    void convert(const float &inValue);
    void convert(const int32_t &inValue);
    
    void convert(const Array<double> &inValue) {
        convertArray(inValue.memoryHandle(), inValue.num_elements());
    }
    
    void convert(const DoubleCol &inValue) {
        convertArray(inValue.memoryHandle(), inValue.n_elem);
    }
    
    void convert(const AnyValueVector &inRecord);
    
protected:
    TupleDesc mTupleDesc;
    Oid mTypeID;
    
    /**
     * @brief PostgreSQL Datum value used during the convertToDatum call
     */
    Datum mConvertedValue;
    
    void convertArray(const MemHandleSPtr &inHandle, uint32_t inNumElements);
};

} // namespace dbconnector

} // namespace madlib

#endif
