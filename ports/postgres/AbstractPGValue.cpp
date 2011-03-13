/* ----------------------------------------------------------------------- *//**
 *
 * @file AbstractPGValue.cpp
 *
 *//* ----------------------------------------------------------------------- */

#include <madlib/dbal/ConcreteValue.hpp>
#include <madlib/ports/postgres/AbstractPGValue.hpp>
#include <madlib/ports/postgres/PGValue.hpp>

extern "C" {
    #include "catalog/pg_type.h"
    #include "utils/typcache.h"
    #include "utils/lsyscache.h"
}

namespace madlib {

namespace ports {

namespace postgres {

using dbal::AbstractValueSPtr;
using dbal::ConcreteValue;


AbstractValueSPtr AbstractPGValue::DatumToValue(Oid inTypeID, Datum inDatum) const {
    // First check if datum is rowtype
    if (type_is_rowtype(inTypeID)) {
        HeapTupleHeader pgTuple = DatumGetHeapTupleHeader(inDatum);
        return AbstractValueSPtr(new PGValue<HeapTupleHeader>(pgTuple));
    }

    switch (inTypeID) {
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
