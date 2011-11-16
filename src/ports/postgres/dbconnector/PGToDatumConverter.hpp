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
 * This class is responsible for converting DBAL types into Datum suitable to
 * return to the PostgreSQL backend. This may involve reliquishing ownership
 * of memory (provided it is guaranteed that the memory will be cleaned up by
 * the backend), copying data that would otherwise go out of scope, etc.
 * The assumption is that after convertToDatum(const AbstractType &) has been
 * called, the respective DBAL object will not be used any more.
 *
 * @note This class makes frequent calls into the PostgreSQL backend.
 *     It is not thread-safe. This should never be an issue, though,
 *     because PostgreSQL does not use threads.
 */
class PGToDatumConverter : public AbstractTypeConverter {
public:
    PGToDatumConverter(const FunctionCallInfo inFCInfo);
    
    PGToDatumConverter(Oid inTypeID);
    
    ~PGToDatumConverter() {
        if (mTupleDesc != NULL)
            ReleaseTupleDesc(mTupleDesc);
    }
    
    Datum convertToDatum(const AbstractType &inValue);

    // We do not override all overloads. So we want to unhide the
    // inherited non-overridden overloads
    using AbstractTypeConverter::callbackWithValue;

    void callbackWithValue(const AnyTypeVector &inRecord);

    void callbackWithValue(const double &inValue);
    void callbackWithValue(const float &inValue);
    void callbackWithValue(const int32_t &inValue);
    
    void callbackWithValue(const Array<double> &inValue) {
        convertArray(inValue.memoryHandle(), inValue.num_elements());
    }
    
    void callbackWithValue(const ArmadilloTypes::DoubleCol &inValue) {
        convertArray(inValue.memoryHandle(), inValue.n_elem);
    }
        
protected:
    TupleDesc mTupleDesc;
    Oid mTypeID;
    
    bool mTargetIsComposite;
        
    /**
     * @brief PostgreSQL Datum value used during the convertToDatum call
     */
    Datum mConvertedValue;
    
    void convertArray(const MemHandleSPtr &inHandle, uint32_t inNumElements);
};

} // namespace dbconnector

} // namespace madlib

#endif
