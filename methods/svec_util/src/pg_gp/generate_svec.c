#include <postgres.h>


#if PG_VERSION_NUM >= 90100
#include "catalog/pg_collation.h"
#endif
#include "utils/array.h"
#include "utils/lsyscache.h"

#include "../../../svec/src/pg_gp/sparse_vector.h"

PG_FUNCTION_INFO_V1(generate_sparse_vector);
Datum generate_sparse_vector(PG_FUNCTION_ARGS)
{
    SvecType    *output_sfv;
    int16_t     typlen;
	bool        typbyval;
	char        typalign;
	bool        *nulls;

	if (PG_NARGS() != 3)
		elog(ERROR, "Invalid number of arguments.");

	ArrayType   *term_index = PG_GETARG_ARRAYTYPE_P(0);
	ArrayType   *term_count = PG_GETARG_ARRAYTYPE_P(1);

	int64_t dict_size = PG_GETARG_INT64(2);

	/* Check if arrays have null entries */
	if (ARR_HASNULL(term_index) || ARR_HASNULL(term_count))
		elog(ERROR, "One or both of the argument arrays has one or more null entries.");

	if (dict_size <= 0)
		elog(ERROR, "Dictionary size cannot be zero or negative.");

	/* Check if any of the argument arrays is empty */
	if ((ARR_NDIM(term_index) == 0) || (ARR_NDIM(term_count) == 0))
		elog(ERROR, "One or more argument arrays is empty.");

	int term_index_nelems = ARR_DIMS(term_index)[0];
	int term_count_nelems = ARR_DIMS(term_count)[0];


	/* If no. of elements in the arrays are not equal, throw an error */
	if (term_index_nelems != term_count_nelems)
		elog(ERROR, "No. of elements in the argument arrays are not equal.");

	Datum *term_index_data;
	Datum *term_count_data;

	/* Deconstruct the arrays */
	get_typlenbyvalalign(INT8OID, &typlen, &typbyval, &typalign);
	deconstruct_array(term_index, INT8OID, typlen, typbyval, typalign,
	                  &term_index_data, &nulls, &term_index_nelems);

	get_typlenbyvalalign(FLOAT8OID, &typlen, &typbyval, &typalign);
	deconstruct_array(term_count, FLOAT8OID, typlen, typbyval, typalign,
	                  &term_count_data, &nulls, &term_count_nelems);

	/* Check if term index array has indexes in proper order or not */
	for(int i = 0; i < term_index_nelems; i++)
	{
		if (DatumGetInt64(term_index_data[i]) < 0 ||
                DatumGetInt64(term_index_data[i]) >= dict_size)
			elog(ERROR, "Term indexes must range from 0 to total number of elements in the dictonary - 1.");
	}
        

    float8 *histogram = (float8 *)palloc0(sizeof(float8) * dict_size);
    for (int k = 0; k < dict_size; k++)
    {
        histogram[k] = 0;
    }
    for (int i = 0; i < term_index_nelems; i++)
    {
        uint64_t idx = DatumGetInt64(term_index_data[i]);
        histogram[idx] += DatumGetFloat8(term_count_data[i]);
    }

    output_sfv = svec_from_float8arr(histogram, dict_size);
    pfree(histogram);

    PG_RETURN_POINTER(output_sfv);
}
