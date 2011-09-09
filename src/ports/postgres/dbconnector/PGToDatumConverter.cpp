/* ----------------------------------------------------------------------- *//**
 *
 * @file PGToDatumConverter.cpp
 *
 * @brief Automatic conversion of return values into PostgreSQL Datums
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/PGToDatumConverter.hpp>
#include <dbconnector/PGArrayHandle.hpp>

extern "C" {
    #include <utils/array.h>
    #include <catalog/pg_type.h>
    #include <funcapi.h>
    #include <utils/lsyscache.h>
    #include <utils/typcache.h>
}


namespace madlib {

namespace dbconnector {

/**
 * @brief Convert an arbitrary value to PostgreSQL Datum type
 */
Datum PGToDatumConverter::convertToDatum(const AbstractType &inValue) {
    if (!inValue.isCompound() && mTargetIsComposite)
        throw std::logic_error("Internal function does not provide compound "
            "type expected by SQL function");
    
    if (inValue.isCompound() && !mTargetIsComposite)
        throw std::logic_error("SQL function or context does not accept "
            "compound type");

    inValue.performCallback(*this);
    return mConvertedValue;
}

/**
 * @brief Constructor: Initialize conversion of function return value.
 *
 * @see PGInterface for information on necessary precautions when writing
 *      PostgreSQL plug-in code in C++.
 */
PGToDatumConverter::PGToDatumConverter(const FunctionCallInfo inFCInfo)
    : mTupleDesc(NULL), mTypeID(0) {
    
    bool exceptionOccurred = false;
    TypeFuncClass funcClass = TYPEFUNC_OTHER;
    
    PG_TRY(); {
        // FIXME: (Or Note:) get_call_result_type is tagged as expensive in funcapi.c
        funcClass = get_call_result_type(inFCInfo, &mTypeID, &mTupleDesc);
    } PG_CATCH(); {
        exceptionOccurred = true;
    } PG_END_TRY();
    
    BOOST_ASSERT_MSG(exceptionOccurred == false, "An exception occurred while "
        "converting a DBAL object to a PostgreSQL datum.");
    
    mTargetIsComposite = (funcClass == TYPEFUNC_COMPOSITE);
}

/**
 * @brief Constructor: Initialize conversion of some child element of function
 *        return value
 *
 * @see PGInterface for information on necessary precautions when writing
 *      PostgreSQL plug-in code in C++.
 */
PGToDatumConverter::PGToDatumConverter(Oid inTypeID)
    : mTupleDesc(NULL), mTypeID(inTypeID) {
    
    bool exceptionOccurred = false;
    
    PG_TRY(); {
        mTargetIsComposite = type_is_rowtype(inTypeID);
        
        if (mTargetIsComposite) {
            // Don't ereport errors. We set typmod < 0, and this should not cause
            // an error because compound types in another compund can never be
            // transient. (I think)

            mTupleDesc = lookup_rowtype_tupdesc_noerror(inTypeID, -1, true);
        }
    } PG_CATCH(); {
        exceptionOccurred = true;
    } PG_END_TRY();

    BOOST_ASSERT_MSG(exceptionOccurred == false, "An exception occurred while "
        "converting a DBAL object to a PostgreSQL datum.");
}

/**
 * @brief Convert DBAL compound type to PostgreSQL tuple
 *
 * @see PGInterface for information on necessary precautions when writing
 *      PostgreSQL plug-in code in C++.
 */
void PGToDatumConverter::callbackWithValue(const AnyTypeVector &inRecord) {
    if (!mTargetIsComposite)
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
        resultDatum.get()[i] = PGToDatumConverter(
            mTupleDesc->attrs[i]->atttypid).convertToDatum(inRecord[i]);
        resultDatumIsNull.get()[i] = inRecord[i].isNull();
    }
    
    bool exceptionOccurred = false;
    HeapTuple heapTuple;
    
    PG_TRY(); {
        heapTuple = heap_form_tuple(mTupleDesc, resultDatum.get(),
            resultDatumIsNull.get());
        
        mConvertedValue = HeapTupleGetDatum(heapTuple);
    } PG_CATCH(); {
        exceptionOccurred = true;
    } PG_END_TRY();

    BOOST_ASSERT_MSG(exceptionOccurred == false, "An exception occurred while "
        "converting a DBAL object to a PostgreSQL datum.");
}

/**
 * @brief Convert double-precision floating-point value to PostgreSQL datum
 *
 * @see PGInterface for information on necessary precautions when writing
 *      PostgreSQL plug-in code in C++.
 */
void PGToDatumConverter::callbackWithValue(const double &inValue) {
    bool exceptionOccurred = false;
    bool conversionErrorOccurred = false;

    PG_TRY(); {
        switch (mTypeID) {
            case FLOAT8OID: mConvertedValue = Float8GetDatum(inValue); break;
            default: conversionErrorOccurred = true;
        }
    } PG_CATCH(); {
        exceptionOccurred = true;
    } PG_END_TRY();

    BOOST_ASSERT_MSG(exceptionOccurred == false, "An exception occurred while "
        "converting a DBAL object to a PostgreSQL datum.");
    
    if (conversionErrorOccurred)
        throw std::logic_error(
            "Internal return type does not match SQL return type");
}

/**
 * @brief Convert single-precision floating-point value to PostgreSQL datum
 *
 * We do allow implicit lossless conversion to double precision.
 *
 * @see PGInterface for information on necessary precautions when writing
 *      PostgreSQL plug-in code in C++.
 */
void PGToDatumConverter::callbackWithValue(const float &inValue) {
    bool exceptionOccurred = false;
    bool conversionErrorOccurred = false;

    PG_TRY(); {
        switch (mTypeID) {
            case FLOAT8OID: mConvertedValue = Float8GetDatum(inValue); break;
            case FLOAT4OID: mConvertedValue = Float4GetDatum(inValue); break;
            default: conversionErrorOccurred = true;
        }
    } PG_CATCH(); {
        exceptionOccurred = true;
    } PG_END_TRY();

    BOOST_ASSERT_MSG(exceptionOccurred == false, "An exception occurred while "
        "converting a DBAL object to a PostgreSQL datum.");
    
    if (conversionErrorOccurred)
        throw std::logic_error(
            "Internal return type does not match SQL return type");
}

/**
 * @brief Convert 32-bit integer value to PostgreSQL datum
 *
 * We do allow implicit lossless conversion. Therefore, acceptable conversion
 * targets are:
 * - Integers of at least 32 bit
 * - Floating Point numbers with significand (mantissa) precision at least
 *   32 bit
 *
 * @see PGInterface for information on necessary precautions when writing
 *      PostgreSQL plug-in code in C++.
 */
void PGToDatumConverter::callbackWithValue(const int32_t &inValue) {
    bool exceptionOccurred = false;
    bool conversionErrorOccurred = false;

    PG_TRY(); {
        switch (mTypeID) {
            case INT8OID: mConvertedValue = Int64GetDatum(inValue); break;
            case INT4OID: mConvertedValue = Int32GetDatum(inValue); break;
            case FLOAT8OID: mConvertedValue = Float8GetDatum(inValue); break;
            default: conversionErrorOccurred = true;
        }
    } PG_CATCH(); {
        exceptionOccurred = true;
    } PG_END_TRY();

    BOOST_ASSERT_MSG(exceptionOccurred == false, "An exception occurred while "
        "converting a DBAL object to a PostgreSQL datum.");
    
    if (conversionErrorOccurred)
        throw std::logic_error(
            "Internal return type does not match SQL return type");
}


/**
 * @brief Internal function for converting a DBAL array type into a PostgreSQL
 *        array
 *
 * @see PGInterface for information on necessary precautions when writing
 *      PostgreSQL plug-in code in C++.
 */
void PGToDatumConverter::convertArray(const MemHandleSPtr &inHandle,
    uint32_t inNumElements) {

    bool exceptionOccurred = false;
    Oid elementTypeID = InvalidOid;
    TypeCacheEntry *elementTypeInfo;

    PG_TRY(); {
        elementTypeID = get_element_type(mTypeID);
        elementTypeInfo = lookup_type_cache(elementTypeID, /* flags */ 0);
    } PG_CATCH(); {
        exceptionOccurred = true;
    } PG_END_TRY();

    BOOST_ASSERT_MSG(exceptionOccurred == false, "An exception occurred while "
        "converting a DBAL object to a PostgreSQL datum.");

    if (elementTypeID == InvalidOid)
        throw std::logic_error(
            "Internal return type does not match SQL declaration");

    shared_ptr<PGArrayHandle> arrayHandle
        = dynamic_pointer_cast<PGArrayHandle>(inHandle);
    
    PG_TRY(); {
        if (arrayHandle) {
            // We will not deallocate the storage used by the Array
            // because we are returning a pointer to this storage!
            // We are guaranteed that backend code will take care of
            // deallocation. See MADLIB-250.
            arrayHandle->release();
            mConvertedValue = PointerGetDatum(arrayHandle->array());
        } else {
            // FIXME: Check whether this code is used at all.
            // If the Array does not use a PostgreSQL array
            // as its storage, we have to create a new PostgreSQL array
            // and copy the values (contruct_array() will do a copy).
            mConvertedValue =
                PointerGetDatum(
                    construct_array(
                        static_cast<Datum*>(inHandle->ptr()),
                        inNumElements,
                        elementTypeID,
                        elementTypeInfo->typlen,
                        elementTypeInfo->typbyval,
                        elementTypeInfo->typalign
                    )
                );
        }
    } PG_CATCH(); {
        exceptionOccurred = true;
    } PG_END_TRY();
    
    BOOST_ASSERT_MSG(exceptionOccurred == false, "An exception occurred while "
        "converting a DBAL object to a PostgreSQL datum.");
}

} // namespace dbconnector

} // namespace madlib
