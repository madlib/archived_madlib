/* ----------------------------------------------------------------------- *//**
 *
 * @file AbstractPGValue.cpp
 *
 *//* ----------------------------------------------------------------------- */

#include <madlib/ports/postgres/compatibility.hpp>
#include <madlib/ports/postgres/AbstractPGValue.hpp>
#include <madlib/ports/postgres/PGArrayHandle.hpp>
#include <madlib/ports/postgres/PGValue.hpp>

extern "C" {
    #include <catalog/pg_type.h>
    #include <utils/array.h>
    #include <utils/typcache.h>
    #include <utils/lsyscache.h>
}

namespace madlib {

namespace ports {

namespace postgres {

/**
 * Convert postgres Datum into a ConcreteValue object.
 */
AbstractValueSPtr AbstractPGValue::DatumToValue(bool inMemoryIsWritable,
    Oid inTypeID, Datum inDatum) const {
        
    // First check if datum is rowtype
    if (type_is_rowtype(inTypeID)) {
        HeapTupleHeader pgTuple = DatumGetHeapTupleHeader(inDatum);
        return AbstractValueSPtr(new PGValue<HeapTupleHeader>(pgTuple));
    } else if (type_is_array(inTypeID)) {
        ArrayType *pgArray = DatumGetArrayTypeP(inDatum);
        
        if (ARR_NDIM(pgArray) != 1)
            throw std::invalid_argument("Multidimensional arrays not yet supported");
        
        if (ARR_HASNULL(pgArray))
            throw std::invalid_argument("Arrays with NULLs not yet supported");
        
        switch (ARR_ELEMTYPE(pgArray)) {
            case FLOAT8OID: {
                MemHandleSPtr memoryHandle(new PGArrayHandle(pgArray));
                
                if (inMemoryIsWritable) {
                    return AbstractValueSPtr(
                        new ConcreteValue<Array<double> >(
                            Array<double>(memoryHandle,
                                boost::extents[ ARR_DIMS(pgArray)[0] ])
                            )
                        );
                } else {
                    return AbstractValueSPtr(
                        new ConcreteValue<Array_const<double> >(
                            Array_const<double>(memoryHandle,
                                boost::extents[ ARR_DIMS(pgArray)[0] ])
                            )
                        );
                }
            }
        }
    }

    switch (inTypeID) {
        case BOOLOID: return AbstractValueSPtr(
            new ConcreteValue<bool>( DatumGetBool(inDatum) ));
        case INT2OID: return AbstractValueSPtr(
            new ConcreteValue<int16_t>( DatumGetInt16(inDatum) ));
        case INT4OID: return AbstractValueSPtr(
            new ConcreteValue<int32_t>( DatumGetInt32(inDatum) ));
        case INT8OID: return AbstractValueSPtr(
            new ConcreteValue<int64_t>( DatumGetInt64(inDatum) ));
        case FLOAT4OID: return AbstractValueSPtr(
            new ConcreteValue<float>( DatumGetFloat4(inDatum) ));
        case FLOAT8OID: return AbstractValueSPtr(
            new ConcreteValue<double>( DatumGetFloat8(inDatum) ));
    }
    
    return AbstractValueSPtr();
}

} // namespace postgres

} // namespace ports

} // namespace madlib
