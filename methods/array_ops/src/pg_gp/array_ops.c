#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"
#include "libpq/pqformat.h"
#include "parser/parse_coerce.h"
#include "catalog/pg_type.h"
#include "utils/array.h"
#include "utils/numeric.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/int8.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"
#include "utils/typcache.h"
#include "access/hash.h"
#include <math.h>

#ifndef NO_PG_MODULE_MAGIC
    PG_MODULE_MAGIC;
#endif

/*
This module provides c implementations for several postgres array operations.

There is a 3 tier structure to each function calls. This is done to take
advantage of some common type checking. All the functions are classified into
4 types based on the input/output types:
function(array,array)->array (calls: General_2Array_to_Array)
function(array,scalar)->array (calls: General_Array_to_Array)
function(array,array)->scalar (calls: General_2Array_to_Element)
function(array,scalar)->scalar (calls: General_Array_to_Element)
function(array,scalar)->struct (calls: General_Array_to_Struct)

Assuming that this input is flexible enough for some new function, implementer
needs to provide 2 functions. First is the top level function that is being
exposed to SQL and takes the necessary parameters. This function makes a call
to one of the 4 intermediate functions, passing pointer to the low level
function (or functions) as the argument. In case of the single function, this
low level function, it will specify the operations to be executed against each
cell in the array. If two functions are to be passed the second is the
finalization function that takes the result of the execution on each cell and
produces final result. In case not final functions is necessary a generic
'noop_finalize' can be used - which does nothing to the intermediate result.
*/

static ArrayType *General_2Array_to_Array(ArrayType *v1, ArrayType *v2,
        Datum(*element_function)(Datum,Oid,Datum,Oid,Datum,Oid));
static ArrayType *General_Array_to_Array(ArrayType *v1, Datum value,
        Datum(*element_function)(Datum,Oid,Datum,Oid,Datum,Oid));
static Datum General_2Array_to_Element(ArrayType *v1, ArrayType *v2,
        Datum(*element_function)(Datum,Oid,Datum,Oid,Datum,Oid),
        Datum(*finalize_function)(Datum,int,Oid));
static Datum General_Array_to_Element(ArrayType *v, Datum exta_val, float8 init_val,
        Datum(*element_function)(Datum,Oid,Datum,Oid,Datum,Oid),
        Datum(*finalize_function)(Datum,int,Oid));
static Datum General_Array_to_Struct(ArrayType *v, void *init_val,
        void*(*element_function)(Datum,Oid,int,void*),
        Datum(*finalize_function)(void*,int,Oid));

static inline Datum noop_finalize(Datum elt,int size,Oid element_type);
static inline Datum average_finalize(Datum elt,int size,Oid element_type);
static inline Datum average_root_finalize(Datum elt,int size,Oid element_type);
static inline Datum value_index_finalize(void *mid_result,int size,Oid element_type);

static inline Datum element_cos(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type);
static inline Datum element_add(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type);
static inline Datum element_sub(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type);
static inline Datum element_mult(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type);
static inline Datum element_div(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type);
static inline Datum element_set(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type);
static inline Datum element_abs(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type);
static inline Datum element_sqrt(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type);
static inline Datum element_pow(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type);
static inline Datum element_square(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type);
static inline Datum element_dot(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type);
static inline Datum element_contains(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type);
static inline Datum element_max(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type);
static inline Datum element_min(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type);
static inline void* element_argmax(Datum element, Oid elt_type, int elt_index, void *result);
static inline void* element_argmin(Datum element, Oid elt_type, int elt_index, void *result);
static inline Datum element_sum(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type);
static inline Datum element_abs_sum(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type);
static inline Datum element_diff(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type);
static inline Datum element_sum_sqr(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type);

static ArrayType* array_to_float8_array(ArrayType *a);

static inline int64 datum_int64_cast(Datum elt, Oid element_type);
static inline Datum int64_datum_cast(int64 result, Oid result_type);
static inline float8 datum_float8_cast(Datum elt, Oid element_type);
static inline Datum float8_datum_cast(float8 result, Oid result_type);

static inline Datum element_op(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type,
        float8 (*op)(float8, float8, float8));

static inline float8 float8_cos(float8 op1, float8 op2, float8 opt_op);
static inline float8 float8_add(float8 op1, float8 op2, float8 opt_op);
static inline float8 float8_sub(float8 op1, float8 op2, float8 opt_op);
static inline float8 float8_mult(float8 op1, float8 op2, float8 opt_op);
static inline float8 float8_div(float8 op1, float8 op2, float8 opt_op);
static inline float8 float8_set(float8 op1, float8 op2, float8 opt_op);
static inline float8 float8_abs(float8 op1, float8 op2, float8 opt_op);
static inline float8 float8_sqrt(float8 op1, float8 op2, float8 opt_op);
static inline float8 float8_pow(float8 op1, float8 op2, float8 opt_op);
static inline float8 float8_square(float8 op1, float8 op2, float8 opt_op);
static inline float8 float8_dot(float8 op1, float8 op2, float8 opt_op);
static inline float8 float8_contains(float8 op1, float8 op2, float8 opt_op);
static inline float8 float8_max(float8 op1, float8 op2, float8 opt_op);
static inline float8 float8_min(float8 op1, float8 op2, float8 opt_op);
static inline float8 float8_sum(float8 op1, float8 op2, float8 opt_op);
static inline float8 float8_abs_sum(float8 op1, float8 op2, float8 opt_op);
static inline float8 float8_diff(float8 op1, float8 op2, float8 opt_op);
static inline float8 float8_sum_sqr(float8 op1, float8 op2, float8 opt_op);

/*
 * Implementation of operations on float8 type
 */
static
inline
float8
float8_cos(float8 op1, float8 op2, float8 opt_op){
    (void) op2;
    (void) opt_op;
    return cos(op1);
}

static
inline
float8
float8_add(float8 op1, float8 op2, float8 opt_op){
    (void) op2;
    return op1 + opt_op;
}

static
inline
float8
float8_sub(float8 op1, float8 op2, float8 opt_op){
    (void) op2;
    return op1 - opt_op;
}

static
inline
float8
float8_mult(float8 op1, float8 op2, float8 opt_op){
    (void) op2;
    return op1 * opt_op;
}

static
inline
int64
int64_div(int64 num, int64 denom){
    if (denom == 0) {
        ereport(ERROR, (errcode(ERRCODE_DIVISION_BY_ZERO),
            errmsg("division by zero is not allowed"),
            errdetail("Arrays with element 0 can not be use in the denominator")));
    }
    return num / denom;
}

static
inline
float8
float8_div(float8 op1, float8 op2, float8 opt_op){
    (void) op2;
    if (opt_op == 0) {
        ereport(ERROR, (errcode(ERRCODE_DIVISION_BY_ZERO),
            errmsg("division by zero is not allowed"),
            errdetail("Arrays with element 0 can not be use in the denominator")));
    }
    return op1 / opt_op;
}

static
inline
float8
float8_set(float8 op1, float8 op2, float8 opt_op){
    (void) op2;
    (void) op1;
    return opt_op;
}

static
inline
float8
float8_abs(float8 op1, float8 op2, float8 opt_op){
    (void) op2;
    (void) opt_op;
    return fabs(op1);
}

static
inline
float8
float8_sqrt(float8 op1, float8 op2, float8 opt_op){
    (void) op2;
    (void) opt_op;
    if (op1 < 0) {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
            errmsg("square root of negative values is not allowed"),
            errdetail("Arrays with negative values can not be input of array_sqrt")));
    }
    return sqrt(op1);
}

static
inline
float8
float8_pow(float8 op1, float8 op2, float8 opt_op){
	(void) op2;
	return pow(op1, opt_op);
}

static
inline
float8
float8_square(float8 op1, float8 op2, float8 opt_op){
    (void) op2;
    (void) opt_op;
    return op1*op1;
}

static
inline
float8
float8_dot(float8 op1, float8 op2, float8 opt_op){
    return op2 + op1 * opt_op;
}

static
inline
float8
float8_contains(float8 op1, float8 op2, float8 opt_op){
    return op2 + (((op1 == opt_op) || (opt_op == 0))? 0 : 1);
}

static
inline
float8
float8_max(float8 op1, float8 op2, float8 opt_op) {
    (void) opt_op;
    return (op1 > op2) ? op1:op2;
}

static
inline
float8
float8_min(float8 op1, float8 op2, float8 opt_op) {
     (void) opt_op;
     return (op1 < op2) ? op1: op2;
}

static
inline
float8
float8_sum(float8 op1, float8 op2, float8 opt_op){
    (void) opt_op;
    return op1 + op2;
}


static
inline
float8
float8_abs_sum(float8 op1, float8 op2, float8 opt_op){
    (void) opt_op;
    return fabs(op1) + op2;
}

static
inline
float8
float8_diff(float8 op1, float8 op2, float8 opt_op){
    return op2 + (op1 - opt_op) * (op1 - opt_op);
}

static
inline
float8
float8_sum_sqr(float8 op1, float8 op2, float8 opt_op){
    (void) opt_op;
    return op2 + op1 * op1;
}
/*
 * Assume the input array is of type numeric[].
 */
ArrayType*
array_to_float8_array(ArrayType *x) {
    Oid element_type = ARR_ELEMTYPE(x);
    if (element_type == FLOAT8OID) {
        // this does not returning a copy, the caller needs to pay attention
        return x;
    }

    // deconstruct
    TypeCacheEntry * TI = lookup_type_cache(element_type,
                                            TYPECACHE_CMP_PROC_FINFO);
    bool *nulls = NULL;
    int len = 0;
    Datum *array = NULL;
    deconstruct_array(x,
                      element_type,
                      TI->typlen,
                      TI->typbyval,
                      TI->typalign,
                      &array,
                      &nulls,
                      &len);

    // casting
    Datum *float8_array = (Datum *)palloc(len * sizeof(Datum));
    int i = 0;
    for (i = 0; i < len; i ++) {
        if (nulls[i]) { float8_array[i] = Float8GetDatum(0); }
        else {
            float8_array[i] = Float8GetDatum(
                    datum_float8_cast(array[i], element_type));
        }
    }

    // reconstruct
    TypeCacheEntry * FLOAT8TI = lookup_type_cache(FLOAT8OID,
                                                  TYPECACHE_CMP_PROC_FINFO);
    ArrayType *ret = construct_md_array(float8_array,
                                        nulls,
                                        ARR_NDIM(x),
                                        ARR_DIMS(x),
                                        ARR_LBOUND(x),
                                        FLOAT8OID,
                                        FLOAT8TI->typlen,
                                        FLOAT8TI->typbyval,
                                        FLOAT8TI->typalign);

    pfree(array);
    pfree(float8_array);
    pfree(nulls);

    return ret;
}

// ------------------------------------------------------------------------
// exported functions
// ------------------------------------------------------------------------
/*
 * This function returns an float array with specified size.
 */
PG_FUNCTION_INFO_V1(array_of_float);
Datum
array_of_float(PG_FUNCTION_ARGS){
    int size = PG_GETARG_INT32(0);
    if (size <= 0 || size > 10000000) {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("invalid array length"),
                 errdetail("array_of_bigint: Size should be in [1, 1e7], %d given", size)));
    }
    Datum* array = palloc (sizeof(Datum)*size);
    for(int i = 0; i < size; ++i) {
        array[i] = Float8GetDatum(0);
    }

    TypeCacheEntry *typentry = lookup_type_cache(FLOAT8OID,TYPECACHE_CMP_PROC_FINFO);
    int type_size = typentry->typlen;
    bool typbyval = typentry->typbyval;
    char typalign = typentry->typalign;

    ArrayType *pgarray = construct_array(array,size,FLOAT8OID,type_size,typbyval,typalign);
    PG_RETURN_ARRAYTYPE_P(pgarray);
}

/*
 * This function returns an integer array with specified size.
 */
PG_FUNCTION_INFO_V1(array_of_bigint);
Datum
array_of_bigint(PG_FUNCTION_ARGS){
    int size = PG_GETARG_INT32(0);
    if (size <= 0 || size > 10000000) {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("invalid array length"),
                 errdetail("array_of_bigint: Size should be in [1, 1e7], %d given", size)));
    }
    Datum* array = palloc (sizeof(Datum)*size);
    for(int i = 0; i < size; ++i) {
        array[i] = Int64GetDatum(0);
    }

    TypeCacheEntry *typentry = lookup_type_cache(INT8OID,TYPECACHE_CMP_PROC_FINFO);
    int type_size = typentry->typlen;
    bool typbyval = typentry->typbyval;
    char typalign = typentry->typalign;

    ArrayType *pgarray = construct_array(array,size,INT8OID,type_size,typbyval,typalign);
    PG_RETURN_ARRAYTYPE_P(pgarray);
}

/*
 * This function returns standard deviation of the array elements.
 */
PG_FUNCTION_INFO_V1(array_stddev);
Datum
array_stddev(PG_FUNCTION_ARGS){
    if (PG_ARGISNULL(0)) { PG_RETURN_NULL(); }

    ArrayType *x = PG_GETARG_ARRAYTYPE_P(0);

    Datum mean = General_Array_to_Element(x, Float8GetDatum(0), 0.0,
            element_sum, average_finalize);
    Datum res = General_Array_to_Element(x, mean, 0.0,
            element_diff, average_root_finalize);

    PG_FREE_IF_COPY(x, 0);

    return(res);
}

/*
 * This function returns mean of the array elements.
 */
PG_FUNCTION_INFO_V1(array_mean);
Datum
array_mean(PG_FUNCTION_ARGS){
    if (PG_ARGISNULL(0)) { PG_RETURN_NULL(); }

    ArrayType *v = PG_GETARG_ARRAYTYPE_P(0);

    Datum res = General_Array_to_Element(v, Float8GetDatum(0), 0.0,
            element_sum, average_finalize);

    PG_FREE_IF_COPY(v, 0);

    return(res);
}

/*
 * This function returns sum of the array elements, typed float8.
 */
PG_FUNCTION_INFO_V1(array_sum_big);
Datum
array_sum_big(PG_FUNCTION_ARGS){
    if (PG_ARGISNULL(0)) { PG_RETURN_NULL(); }

    ArrayType *v = PG_GETARG_ARRAYTYPE_P(0);

    Datum res = General_Array_to_Element(v, Float8GetDatum(0), 0.0,
            element_sum, noop_finalize);

    PG_FREE_IF_COPY(v, 0);

    return(res);
}

/*
 * This function returns sum of the array elements.
 */
PG_FUNCTION_INFO_V1(array_sum);
Datum
array_sum(PG_FUNCTION_ARGS){
    if (PG_ARGISNULL(0)) { PG_RETURN_NULL(); }

    ArrayType *v = PG_GETARG_ARRAYTYPE_P(0);
    Oid element_type = ARR_ELEMTYPE(v);

    Datum res = General_Array_to_Element(v, Float8GetDatum(0), 0.0,
            element_sum, noop_finalize);

    PG_FREE_IF_COPY(v, 0);

    return float8_datum_cast(DatumGetFloat8(res), element_type);
}

/*
 * This function returns sum of the array elements' absolute value.
 */
PG_FUNCTION_INFO_V1(array_abs_sum);
Datum
array_abs_sum(PG_FUNCTION_ARGS){
    if (PG_ARGISNULL(0)) { PG_RETURN_NULL(); }

    ArrayType *v = PG_GETARG_ARRAYTYPE_P(0);
    Oid element_type = ARR_ELEMTYPE(v);

    Datum res = General_Array_to_Element(v, Float8GetDatum(0), 0.0,
            element_abs_sum, noop_finalize);

    PG_FREE_IF_COPY(v, 0);

    return float8_datum_cast(DatumGetFloat8(res), element_type);
}


/*
 * This function returns minimum of the array elements.
 */
PG_FUNCTION_INFO_V1(array_min);
Datum
array_min(PG_FUNCTION_ARGS){
    if (PG_ARGISNULL(0)) { PG_RETURN_NULL(); }

    ArrayType *v = PG_GETARG_ARRAYTYPE_P(0);
    Oid element_type = ARR_ELEMTYPE(v);
    Datum res = General_Array_to_Element(v, Float8GetDatum(0), get_float8_infinity(),
            element_min, noop_finalize);

    PG_FREE_IF_COPY(v, 0);

    return float8_datum_cast(DatumGetFloat8(res), element_type);
}

/*
 * This function returns maximum of the array elements.
 */
PG_FUNCTION_INFO_V1(array_max);
Datum
array_max(PG_FUNCTION_ARGS){
    if (PG_ARGISNULL(0)) { PG_RETURN_NULL(); }

    ArrayType *v = PG_GETARG_ARRAYTYPE_P(0);
    Oid element_type = ARR_ELEMTYPE(v);

    Datum res = General_Array_to_Element(v, Float8GetDatum(0), -get_float8_infinity(),
            element_max, noop_finalize);

    PG_FREE_IF_COPY(v, 0);

    return float8_datum_cast(DatumGetFloat8(res), element_type);
}

typedef struct
{
    float8 value;
    int64  index;
} value_index;

/*
 * This function returns maximum and corresponding index of the array elements.
 */
PG_FUNCTION_INFO_V1(array_max_index);
Datum
array_max_index(PG_FUNCTION_ARGS) {
    if (PG_ARGISNULL(0)) { PG_RETURN_NULL(); }
    ArrayType *v = PG_GETARG_ARRAYTYPE_P(0);

    if (ARR_NDIM(v) != 1) {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                        errmsg("Input array with multiple dimensions is not allowed!")));
    }

    value_index *result = (value_index *)palloc(sizeof(value_index));
    result->value = -get_float8_infinity();
    result->index = 0;

    Datum res = General_Array_to_Struct(v, result, element_argmax, value_index_finalize);

    pfree(result);
    PG_FREE_IF_COPY(v, 0);
    return res;
}

/*
 * This function returns minimum and corresponding index of the array elements.
 */
PG_FUNCTION_INFO_V1(array_min_index);
Datum
array_min_index(PG_FUNCTION_ARGS) {
    if (PG_ARGISNULL(0)) { PG_RETURN_NULL(); }
    ArrayType *v = PG_GETARG_ARRAYTYPE_P(0);

    if (ARR_NDIM(v) != 1) {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                        errmsg("Input array with multiple dimensions is not allowed!")));
    }

    value_index *result = (value_index *)palloc(sizeof(value_index));
    result->value = get_float8_infinity();
    result->index = 0;

    Datum res = General_Array_to_Struct(v, result, element_argmin, value_index_finalize);

    PG_FREE_IF_COPY(v, 0);

    pfree(result);
    return res;
}

/*
 * This function returns dot product of two arrays as vectors.
 */
PG_FUNCTION_INFO_V1(array_dot);
Datum
array_dot(PG_FUNCTION_ARGS){
    if (PG_ARGISNULL(0)) { PG_RETURN_NULL(); }
    if (PG_ARGISNULL(1)) { PG_RETURN_NULL(); }

    ArrayType *v1 = PG_GETARG_ARRAYTYPE_P(0);
    ArrayType *v2 = PG_GETARG_ARRAYTYPE_P(1);

    Datum res = General_2Array_to_Element(v1, v2, element_dot, noop_finalize);

    PG_FREE_IF_COPY(v1, 0);
    PG_FREE_IF_COPY(v2, 1);

    return(res);
}

/*
 * This function checks if each non-zero element in the right array equals to
 * the element with the same index in the left array.
 */
PG_FUNCTION_INFO_V1(array_contains);
Datum
array_contains(PG_FUNCTION_ARGS){
    if (PG_ARGISNULL(0)) { PG_RETURN_NULL(); }
    if (PG_ARGISNULL(1)) { PG_RETURN_NULL(); }

    ArrayType *v1 = PG_GETARG_ARRAYTYPE_P(0);
    ArrayType *v2 = PG_GETARG_ARRAYTYPE_P(1);

    Datum res = General_2Array_to_Element(v1, v2, element_contains, noop_finalize);

    PG_FREE_IF_COPY(v1, 0);
    PG_FREE_IF_COPY(v2, 1);

    if (DatumGetFloat8(res) == 0.) {
        PG_RETURN_BOOL(TRUE);
    } else {
        PG_RETURN_BOOL(FALSE);
    }
}

/*
 * This function returns the element-wise sum of two arrays.
 */
PG_FUNCTION_INFO_V1(array_add);
Datum
array_add(PG_FUNCTION_ARGS){
    // special handling for madlib.sum()
    if (PG_ARGISNULL(0) && PG_ARGISNULL(1)) { PG_RETURN_NULL(); }
    if (PG_ARGISNULL(0)) {
        PG_RETURN_ARRAYTYPE_P(PG_GETARG_ARRAYTYPE_P(1));
    }
    if (PG_ARGISNULL(1)) {
        PG_RETURN_ARRAYTYPE_P(PG_GETARG_ARRAYTYPE_P(0));
    }

    ArrayType *v1 = PG_GETARG_ARRAYTYPE_P(0);
    ArrayType *v2 = PG_GETARG_ARRAYTYPE_P(1);

    ArrayType *res = General_2Array_to_Array(v1, v2, element_add);

    PG_FREE_IF_COPY(v1, 0);
    PG_FREE_IF_COPY(v2, 1);

    PG_RETURN_ARRAYTYPE_P(res);
}

/*
 * This function returns the element-wise difference of two arrays.
 */
PG_FUNCTION_INFO_V1(array_sub);
Datum
array_sub(PG_FUNCTION_ARGS){
    if (PG_ARGISNULL(0)) { PG_RETURN_NULL(); }
    if (PG_ARGISNULL(1)) { PG_RETURN_NULL(); }

    ArrayType *v1 = PG_GETARG_ARRAYTYPE_P(0);
    ArrayType *v2 = PG_GETARG_ARRAYTYPE_P(1);

    ArrayType *res = General_2Array_to_Array(v1, v2, element_sub);

    PG_FREE_IF_COPY(v1, 0);
    PG_FREE_IF_COPY(v2, 1);

    PG_RETURN_ARRAYTYPE_P(res);
}

/*
 * This function returns the element-wise product of two arrays.
 */
PG_FUNCTION_INFO_V1(array_mult);
Datum
array_mult(PG_FUNCTION_ARGS){
    if (PG_ARGISNULL(0)) { PG_RETURN_NULL(); }
    if (PG_ARGISNULL(1)) { PG_RETURN_NULL(); }

    ArrayType *v1 = PG_GETARG_ARRAYTYPE_P(0);
    ArrayType *v2 = PG_GETARG_ARRAYTYPE_P(1);

    ArrayType *res = General_2Array_to_Array(v1, v2, element_mult);

    PG_FREE_IF_COPY(v1, 0);
    PG_FREE_IF_COPY(v2, 1);

    PG_RETURN_ARRAYTYPE_P(res);
}

/*
 * This function returns result of element-wise division of two arrays.
 */
PG_FUNCTION_INFO_V1(array_div);
Datum
array_div(PG_FUNCTION_ARGS){
    if (PG_ARGISNULL(0)) { PG_RETURN_NULL(); }
    if (PG_ARGISNULL(1)) { PG_RETURN_NULL(); }

    ArrayType *v1 = PG_GETARG_ARRAYTYPE_P(0);
    ArrayType *v2 = PG_GETARG_ARRAYTYPE_P(1);

    ArrayType *res = General_2Array_to_Array(v1, v2, element_div);

    PG_FREE_IF_COPY(v1, 0);
    PG_FREE_IF_COPY(v2, 1);

    PG_RETURN_ARRAYTYPE_P(res);
}

/*
 * This function takes the absolute value for each element.
 */
PG_FUNCTION_INFO_V1(array_abs);
Datum
array_abs(PG_FUNCTION_ARGS){
    if (PG_ARGISNULL(0)) { PG_RETURN_NULL(); }

    ArrayType *v1 = PG_GETARG_ARRAYTYPE_P(0);

    Oid element_type = ARR_ELEMTYPE(v1);
    Datum v2 = float8_datum_cast(0, element_type);

    ArrayType *res = General_Array_to_Array(v1, v2, element_abs);

    PG_FREE_IF_COPY(v1, 0);

    PG_RETURN_ARRAYTYPE_P(res);
}


/*
 * This function takes the square root for each element.
 */
PG_FUNCTION_INFO_V1(array_sqrt);
Datum
array_sqrt(PG_FUNCTION_ARGS){
    if (PG_ARGISNULL(0)) { PG_RETURN_NULL(); }

    ArrayType *x = PG_GETARG_ARRAYTYPE_P(0);
    ArrayType *v1 = array_to_float8_array(x);
    Datum v2 = 0;

    ArrayType *res = General_Array_to_Array(v1, v2, element_sqrt);

    if (v1 != x) { pfree(v1); }
    PG_FREE_IF_COPY(x, 0);

    PG_RETURN_ARRAYTYPE_P(res);
}

/*
 * This function takes the power for each element.
 */
PG_FUNCTION_INFO_V1(array_pow);
Datum
array_pow(PG_FUNCTION_ARGS){
    if (PG_ARGISNULL(0)) { PG_RETURN_NULL(); }
    if (PG_ARGISNULL(1)) { PG_RETURN_NULL(); }

    ArrayType *v1 = PG_GETARG_ARRAYTYPE_P(0);
    Datum v2 = PG_GETARG_DATUM(1);

    ArrayType *res = General_Array_to_Array(v1, v2, element_pow);

    PG_FREE_IF_COPY(v1, 0);

    PG_RETURN_ARRAYTYPE_P(res);
}

/*
 * This function takes the square for each element.
 */
PG_FUNCTION_INFO_V1(array_square);
Datum
array_square(PG_FUNCTION_ARGS){
    if (PG_ARGISNULL(0)) { PG_RETURN_NULL(); }

    ArrayType *x = PG_GETARG_ARRAYTYPE_P(0);
    ArrayType *v1 = array_to_float8_array(x);
    Datum v2 = 0;

    ArrayType *res = General_Array_to_Array(v1, v2, element_square);

    if (v1 != x) { pfree(v1); }
    PG_FREE_IF_COPY(x, 0);

    PG_RETURN_ARRAYTYPE_P(res);
}

/*
 * This function sets all elements to be the specified value.
 */
PG_FUNCTION_INFO_V1(array_fill);
Datum
array_fill(PG_FUNCTION_ARGS){
    if (PG_ARGISNULL(0)) { PG_RETURN_NULL(); }
    if (PG_ARGISNULL(1)) { PG_RETURN_NULL(); }

    ArrayType *v1 = PG_GETARG_ARRAYTYPE_P(0);
    Datum v2 = PG_GETARG_DATUM(1);

    ArrayType *res = General_Array_to_Array(v1, v2, element_set);

    PG_FREE_IF_COPY(v1, 0);

    PG_RETURN_ARRAYTYPE_P(res);
}

/*
 * This function apply cos function to each element.
 */
PG_FUNCTION_INFO_V1(array_cos);
Datum
array_cos(PG_FUNCTION_ARGS){
    if (PG_ARGISNULL(0)) { PG_RETURN_NULL(); }

    ArrayType *v1 = PG_GETARG_ARRAYTYPE_P(0);
    Oid element_type = ARR_ELEMTYPE(v1);
    Datum v2 = float8_datum_cast(0, element_type);

    ArrayType *res = General_Array_to_Array(v1, v2, element_cos);

    PG_FREE_IF_COPY(v1, 0);

    PG_RETURN_ARRAYTYPE_P(res);
}

/*
 * This function multiplies the specified value to each element.
 */
PG_FUNCTION_INFO_V1(array_scalar_mult);
Datum
array_scalar_mult(PG_FUNCTION_ARGS){
    if (PG_ARGISNULL(0)) { PG_RETURN_NULL(); }
    if (PG_ARGISNULL(1)) { PG_RETURN_NULL(); }

    ArrayType *v1 = PG_GETARG_ARRAYTYPE_P(0);
    Datum v2 = PG_GETARG_DATUM(1);

    ArrayType *res = General_Array_to_Array(v1, v2, element_mult);

    PG_FREE_IF_COPY(v1, 0);

    PG_RETURN_ARRAYTYPE_P(res);
}

/*
 * This function addes the specified value to each element.
 */
PG_FUNCTION_INFO_V1(array_scalar_add);
Datum
array_scalar_add(PG_FUNCTION_ARGS){
    if (PG_ARGISNULL(0)) { PG_RETURN_NULL(); }
    if (PG_ARGISNULL(1)) { PG_RETURN_NULL(); }

    ArrayType *v1 = PG_GETARG_ARRAYTYPE_P(0);
    Datum v2 = PG_GETARG_DATUM(1);

    ArrayType *res = General_Array_to_Array(v1, v2, element_add);

    PG_FREE_IF_COPY(v1, 0);

    PG_RETURN_ARRAYTYPE_P(res);
}

/*
 * This function removes elements with specified value from an array.
 */
PG_FUNCTION_INFO_V1(array_filter);
Datum
array_filter(PG_FUNCTION_ARGS) {
    ArrayType * arr = PG_GETARG_ARRAYTYPE_P(0);
    if (ARR_NDIM(arr) != 1) {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                        errmsg("Input array with multiple dimensions is not allowed!")));
    }

    if (ARR_HASNULL(arr)) {
        ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
                        errmsg("Input array with nulls is not allowed!")));
    }

    // value to be filtered
    Oid element_type = ARR_ELEMTYPE(arr);
    Datum val = float8_datum_cast(0., element_type);
    char op[3] = "!=";
    if (PG_NARGS() > 1) { val = PG_GETARG_DATUM(1); }
    if (PG_NARGS() > 2) {
        text *op_text = PG_GETARG_TEXT_P(2);
        int op_len = VARSIZE(op_text) - VARHDRSZ;
        strncpy(op, VARDATA(op_text), VARSIZE(op_text) - VARHDRSZ);
        op[op_len] = 0;
    }

    // deconstruct
    TypeCacheEntry * TI = lookup_type_cache(element_type,
                                            TYPECACHE_CMP_PROC_FINFO);
    bool *nulls = NULL;
    int len = 0;
    Datum *array = NULL;
    deconstruct_array(arr,
                      element_type,
                      TI->typlen,
                      TI->typbyval,
                      TI->typalign,
                      &array,
                      &nulls,
                      &len);

    // filtering
    Datum *ret_array = (Datum *)palloc(len * sizeof(Datum));
    int count = 0;
    for (int i = 0; i < len; i ++) {
        float8 left  = datum_float8_cast(array[i], element_type);
        float8 right = datum_float8_cast(val     , element_type);
        bool pass = true;
        if (strcmp(op, "!=") == 0 || strcmp(op, "<>") == 0) {
            if (isnan(left) || isnan(right)) {
                pass = !(isnan(left) && isnan(right));
            } else { pass = (left != right); }
        } else if (strcmp(op, "=") == 0 || strcmp(op, "==") == 0) {
            if (isnan(left) || isnan(right)) {
                pass = (isnan(left) && isnan(right));
            } else { pass = (left == right); }
        } else if (strcmp(op, ">") == 0) {
            pass = (left > right);
        } else if (strcmp(op, ">=") == 0) {
            pass = (left >= right);
        } else if (strcmp(op, "<") == 0) {
            pass = (left < right);
        } else if (strcmp(op, "<=") == 0) {
            pass = (left <= right);
        } else {
            ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                            errmsg("operator is not supported"),
                            errdetail("Filtering operator %s is not supported.", op)));
        }
        if (pass) { ret_array[count ++] = array[i]; }
    }

    // reconstruct
    ArrayType *ret = NULL;
    if (count == 0) {
        elog(WARNING, "array_filter: Returning empty array.");
        ret = construct_empty_array(element_type);
    } else {
        ret = construct_array(ret_array,
                              count,
                              element_type,
                              TI->typlen,
                              TI->typbyval,
                              TI->typalign);
    }

    pfree(array);
    pfree(ret_array);
    pfree(nulls);

    PG_RETURN_ARRAYTYPE_P(ret);
}

/*
 * This function normalizes an array as sum of squares to be 1.
 */

PG_FUNCTION_INFO_V1(array_normalize);
Datum
array_normalize(PG_FUNCTION_ARGS){
    ArrayType * arg = PG_GETARG_ARRAYTYPE_P(0);

    if (ARR_NDIM(arg) != 1) {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                        errmsg("Input array with multiple dimensions is not allowed!")));
    }

    if (ARR_HASNULL(arg)) {
        ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
                        errmsg("Input array with nulls is not allowed!")));
    }

    ArrayType *v = array_to_float8_array(arg);

    Datum norm_sqr = General_Array_to_Element(v, Float8GetDatum(0), 0.0,
            element_sum_sqr, noop_finalize);

    if (DatumGetFloat8(norm_sqr) == 0.) {
        elog(WARNING, "No non-zero elements found, returning the input array.");
        PG_RETURN_ARRAYTYPE_P(arg);
    }
    Datum inverse_norm = Float8GetDatum(1.0/sqrt(DatumGetFloat8(norm_sqr)));
    ArrayType* res = General_Array_to_Array(v, inverse_norm, element_mult);

    if (v != arg) { pfree(v); }
    PG_FREE_IF_COPY(arg,0);

    PG_RETURN_ARRAYTYPE_P(res);

}
/*
 * This function checks if an array contains NULL values.
 */
PG_FUNCTION_INFO_V1(array_contains_null);
Datum array_contains_null(PG_FUNCTION_ARGS){
    ArrayType *arg = PG_GETARG_ARRAYTYPE_P(0);
    if(ARR_HASNULL(arg)){
        return BoolGetDatum(true);
    } else {
        return BoolGetDatum(false);
    }
}


// ------------------------------------------------------------------------
// finalize functions
// ------------------------------------------------------------------------
static
inline
Datum
noop_finalize(Datum elt,int size,Oid element_type){
    (void) size; /* avoid warning about unused parameter */
    (void) element_type; /* avoid warning about unused parameter */
    return elt;
}

static
inline
Datum
average_finalize(Datum elt,int size,Oid element_type){
    float8 value = datum_float8_cast(elt, element_type);
     if (size == 0) {
        elog(WARNING, "Input array only contains NULL or NaN, returning 0");
        return Float8GetDatum(0);
    }
    return Float8GetDatum(value/(float8)size);
}

static
inline
Datum
value_index_finalize(void *mid_result,int size,Oid element_type) {
    Assert(mid_result);
	(void) element_type;
    (void) size;

    value_index *vi = (value_index *)mid_result;
    TypeCacheEntry *typentry = lookup_type_cache(FLOAT8OID, TYPECACHE_CMP_PROC_FINFO);
    int type_size = typentry->typlen;
    bool typbyval = typentry->typbyval;
    char typalign = typentry->typalign;

    Datum result[2];

   // We just use float8 as returned value to avoid overflow.
        result[0] = Float8GetDatum(vi->value);
        result[1] = Float8GetDatum((float8)(vi->index));

    // construct return result
    ArrayType *pgarray = construct_array(result,
                                 2,
                                 FLOAT8OID,
                                 type_size,
                                 typbyval,
                                 typalign);

    PG_RETURN_ARRAYTYPE_P(pgarray);
}

static
inline
Datum
average_root_finalize(Datum elt,int size,Oid element_type){
    float8 value = datum_float8_cast(elt, element_type);
    if (size == 0) {
        return Float8GetDatum(0);
    } else if (size == 1) {
        return Float8GetDatum(0);
    } else {
        return Float8GetDatum(sqrt(value/((float8)size - 1)));
    }
}

/* -----------------------------------------------------------------------
 * Type cast functions
 * -----------------------------------------------------------------------
*/
static
inline
int64
datum_int64_cast(Datum elt, Oid element_type) {
    switch(element_type){
        case INT2OID:
            return (int64) DatumGetInt16(elt); break;
        case INT4OID:
            return (int64) DatumGetInt32(elt); break;
        case INT8OID:
            return elt; break;
        default:
            ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                            errmsg("type is not supported"),
                            errdetail("Arrays with element type %s are not supported.",
                                      format_type_be(element_type))));
            break;
    }
    return 0;
}

static
inline
Datum
int64_datum_cast(int64 res, Oid result_type) {
    Datum result = Int64GetDatum(res);
    switch(result_type){
        case INT2OID:
            return DirectFunctionCall1(int82, result); break;
        case INT4OID:
            return DirectFunctionCall1(int84, result); break;
        case INT8OID:
            return result; break;
        default:
            ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                            errmsg("type is not supported"),
                            errdetail("Arrays with element type %s are not supported.",
                                      format_type_be(result_type))));
            break;
    }
    return result;
}
/* -----------------------------------------------------------------------
 * Type cast functions
 * -----------------------------------------------------------------------
*/
static
inline
float8
datum_float8_cast(Datum elt, Oid element_type) {
    switch(element_type){
        case INT2OID:
            return (float8) DatumGetInt16(elt); break;
        case INT4OID:
            return (float8) DatumGetInt32(elt); break;
        case INT8OID:
            return (float8) DatumGetInt64(elt); break;
        case FLOAT4OID:
            return (float8) DatumGetFloat4(elt); break;
        case FLOAT8OID:
            return (float8) DatumGetFloat8(elt); break;
        case NUMERICOID:
            return DatumGetFloat8(
                    DirectFunctionCall1(numeric_float8_no_overflow, elt));
            break;
        default:
            ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                            errmsg("type is not supported"),
                            errdetail("Arrays with element type %s are not supported.",
                                      format_type_be(element_type))));
            break;
    }
    return 0.0;
}

static
inline
Datum
float8_datum_cast(float8 res, Oid result_type) {
    Datum result = Float8GetDatum(res);
    switch(result_type){
        case INT2OID:
            return DirectFunctionCall1(dtoi2, result); break;
        case INT4OID:
            return DirectFunctionCall1(dtoi4, result); break;
        case INT8OID:
            return DirectFunctionCall1(dtoi8, result); break;
        case FLOAT4OID:
            return DirectFunctionCall1(dtof, result); break;
        case FLOAT8OID:
            return result; break;
        case NUMERICOID:
            return DirectFunctionCall1(float8_numeric, result); break;
        default:
            ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                            errmsg("type is not supported"),
                            errdetail("Arrays with element type %s are not supported.",
                                      format_type_be(result_type))));
            break;
    }
    return result;
}

/*
 *    Template for element-wise help functions
 */
static
inline
Datum
element_op(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type,
        float8 (*op)(float8, float8, float8)) {
    if (op == float8_div &&
            (result_type == INT2OID ||
             result_type == INT4OID ||
             result_type == INT8OID)) {
        int64 num   = datum_int64_cast(element, elt_type);
        int64 denom = datum_int64_cast(opt_elt, opt_type);
        return int64_datum_cast(int64_div(num, denom), result_type);
    }
    float8 elt = datum_float8_cast(element, elt_type   );
    float8 res = datum_float8_cast(result , result_type);
    float8 opt = datum_float8_cast(opt_elt, opt_type   );
    return float8_datum_cast((*op)(elt, res, opt), result_type);
}


// ------------------------------------------------------------------------
// element-wise helper functions
// ------------------------------------------------------------------------
/*
 * Add (elt1-flag)^2 to result.
 */

static
inline
Datum
element_diff(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type){
    return element_op(element, elt_type, result, result_type, opt_elt, opt_type, float8_diff);
}

/*
 * Add abs(elt1) to result.
 */
static
inline
Datum
element_abs_sum(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type){
    return element_op(element, elt_type, result, result_type, opt_elt, opt_type, float8_abs_sum);
}

/*
 * Add elt1 to result.
 */
static
inline
Datum
element_sum(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type){
    return element_op(element, elt_type, result, result_type, opt_elt, opt_type, float8_sum);
}

/*
 * Return min2(elt1, result).
 * First element if flag == 0
 */
static
inline
Datum
element_min(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type){
    return element_op(element, elt_type, result, result_type, opt_elt, opt_type, float8_min);
}

/*
 * Return max(element, index).
 */
static
inline
void*
element_argmax(Datum element, Oid elt_type, int elt_index, void *result) {
    Assert(result);
    value_index *vi = (value_index *)result;
    float8 elt = datum_float8_cast(element, elt_type);
    if (elt > vi->value) {
        vi->value = elt;
        vi->index = elt_index;
    }

    return vi;
}

/*
 * Return min(element, index).
 */
static
inline
void*
element_argmin(Datum element, Oid elt_type, int elt_index, void *result) {
    Assert(result);
    value_index *vi = (value_index *)result;
    float8 elt = datum_float8_cast(element, elt_type);
    if (elt < vi->value) {
        vi->value = elt;
        vi->index = elt_index;
    }

    return vi;
}

/*
 * Return max2(elt1, result).
 * First element if flag == 0
 */
static
inline
Datum
element_max(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type){
    return element_op(element, elt_type, result, result_type, opt_elt, opt_type, float8_max);
}

/*
 * Add elt1*elt2 to result.
 */
static
inline
Datum
element_dot(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type){
    return element_op(element, elt_type, result, result_type, opt_elt, opt_type, float8_dot);
}

/*
 * result++ if elt1 == elt2 or elt2 == 0.
 */
static
inline
Datum
element_contains(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type){
    return element_op(element, elt_type, result, result_type, opt_elt, opt_type, float8_contains);
}

/*
 * Assign result to be elt2.
 */
static
inline
Datum
element_set(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type){
    return element_op(element, elt_type, result, result_type, opt_elt, opt_type, float8_set);
}

/*
 * Assign result to be cos(elt1).
 */
static
inline
Datum
element_cos(Datum element, Oid elt_type, Datum result,
            Oid result_type, Datum opt_elt, Oid opt_type){
    return element_op(element, elt_type, result, result_type, opt_elt, opt_type, float8_cos);
}

/*
 * Assign result to be (elt1 + elt2).
 */
static
inline
Datum
element_add(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type){
    return element_op(element, elt_type, result, result_type, opt_elt, opt_type, float8_add);
}

/*
 * Assign result to be (elt1 - elt2).
 */
static
inline
Datum
element_sub(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type){
    return element_op(element, elt_type, result, result_type, opt_elt, opt_type, float8_sub);
}

/*
 * Assign result to be (elt1 * elt2).
 */
static
inline
Datum
element_mult(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type){
    return element_op(element, elt_type, result, result_type, opt_elt, opt_type, float8_mult);
}

/*
 * Assign result to be (elt1 / elt2).
 */
static
inline
Datum
element_div(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type){
    return element_op(element, elt_type, result, result_type, opt_elt, opt_type, float8_div);
}

/*
 * Assign result to be fabs(elt1).
 */
static
inline
Datum
element_abs(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type){
    return element_op(element, elt_type, result, result_type, opt_elt, opt_type, float8_abs);
}

/*
 * Assign result to be sqrt(elt1).
 */
static
inline
Datum
element_sqrt(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type){
    return element_op(element, elt_type, result, result_type, opt_elt, opt_type, float8_sqrt);
}

/*
 * Assign result to be power(elt1).
 */
static
inline
Datum
element_pow(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type){
    return element_op(element, elt_type, result, result_type, opt_elt, opt_type, float8_pow);
}

/*
 * Assign result to be square(elt1).
 */
static
inline
Datum
element_square(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type){
    return element_op(element, elt_type, result, result_type, opt_elt, opt_type, float8_square);
}

/*
 * add elt1 * elt1 to result.
 */
static
inline
Datum
element_sum_sqr(Datum element, Oid elt_type, Datum result,
        Oid result_type, Datum opt_elt, Oid opt_type){
    return element_op(element, elt_type, result, result_type, opt_elt, opt_type, float8_sum_sqr);
}
// ------------------------------------------------------------------------
// general helper functions
// ------------------------------------------------------------------------
/*
 * @brief Aggregates an array to a scalar.
 *
 * @param v Array.
 * @param exta_val An extra rgument for element_function.
 * @param element_function Transition function.
 * @param finalize_function Final function.
 * @returns Whatever finalize_function returns.
 */
Datum
General_Array_to_Element(
        ArrayType *v,
        Datum exta_val,
        float8 init_val,
        Datum(*element_function)(Datum,Oid,Datum,Oid,Datum,Oid),
        Datum(*finalize_function)(Datum,int,Oid)) {

    // dimensions
    int ndims = ARR_NDIM(v);
    if (ndims == 0) {
        elog(WARNING, "input are empty arrays.");
        return Float8GetDatum(0);
    }
    int *dims = ARR_DIMS(v);
    int nitems = ArrayGetNItems(ndims, dims);

    // type
    Oid element_type = ARR_ELEMTYPE(v);
    TypeCacheEntry *typentry = lookup_type_cache(element_type,TYPECACHE_CMP_PROC_FINFO);
    int type_size = typentry->typlen;
    bool typbyval = typentry->typbyval;
    char typalign = typentry->typalign;

    // iterate
    Datum result = Float8GetDatum(init_val);
    char *dat = ARR_DATA_PTR(v);
    int i = 0;
    int null_count = 0;
    if(ARR_HASNULL(v)){
        bits8 *bitmap = ARR_NULLBITMAP(v);
        int bitmask = 1;
        for (i = 0; i < nitems; i++) {
            /* Get elements, checking for NULL */
            if (!(bitmap && (*bitmap & bitmask) == 0)) {
                Datum elt = fetch_att(dat, typbyval, type_size);
                dat = att_addlength_pointer(dat, type_size, dat);
                dat = (char *) att_align_nominal(dat, typalign);

                // treating NaN as NULL
                if (!isnan(datum_float8_cast(elt,element_type))) {
                    result = element_function(elt,
                                              element_type,
                                              result,
                                              FLOAT8OID,
                                              exta_val,
                                              FLOAT8OID);
                } else {
                    null_count++;
                }
            }else{
                null_count++;
            }

            /* advance bitmap pointers if any */
            if (bitmap) {
                bitmask <<= 1;
                if (bitmask == 0x100) {
                    bitmap++;
                    bitmask = 1;
                }
            }
        }
    } else {
        for (i = 0; i < nitems; i++) {
            Datum elt = fetch_att(dat, typbyval, type_size);
            dat = att_addlength_pointer(dat, type_size, dat);
            dat = (char *) att_align_nominal(dat, typalign);

            // treating NaN as NULL
            if (!isnan(datum_float8_cast(elt,element_type))) {
                result = element_function(elt,
                                          element_type,
                                          result,
                                          FLOAT8OID,
                                          exta_val,
                                          FLOAT8OID);
            } else {
                null_count++;
            }
        }
    }

    return finalize_function(result,(nitems-null_count), FLOAT8OID);
}

// ------------------------------------------------------------------------
// general helper functions
// ------------------------------------------------------------------------
/*
 * @brief Aggregates an array to a composite struct.
 *
 * @param v Array.
 * @param init_val An initial value for element_function.
 * @param element_function Transition function.
 * @param finalize_function Final function.
 * @returns Whatever finalize_function returns.
 */
static Datum General_Array_to_Struct(ArrayType *v, void *init_val,
        void*(*element_function)(Datum,Oid,int,void*),
        Datum(*finalize_function)(void*,int,Oid)) {

    // dimensions
    int ndims = ARR_NDIM(v);
    if (ndims == 0) {
        elog(WARNING, "input are empty arrays.");
        return Float8GetDatum(0);
    }
    int *dims = ARR_DIMS(v);
    int nitems = ArrayGetNItems(ndims, dims);

    // type
    Oid element_type = ARR_ELEMTYPE(v);
    TypeCacheEntry *typentry = lookup_type_cache(element_type,TYPECACHE_CMP_PROC_FINFO);
    int type_size = typentry->typlen;
    bool typbyval = typentry->typbyval;
    char typalign = typentry->typalign;

    // iterate
    void* result = init_val;
    char *dat = ARR_DATA_PTR(v);
    int i = 0;
    int null_count = 0;
    int *lbs = ARR_LBOUND(v);
    if(ARR_HASNULL(v)){
        bits8 *bitmap = ARR_NULLBITMAP(v);
        int bitmask = 1;
        for (i = 0; i < nitems; i++) {
            /* Get elements, checking for NULL */
            if (!(bitmap && (*bitmap & bitmask) == 0)) {
                Datum elt = fetch_att(dat, typbyval, type_size);
                dat = att_addlength_pointer(dat, type_size, dat);
                dat = (char *) att_align_nominal(dat, typalign);

                // treating NaN as NULL
                if (!isnan(datum_float8_cast(elt,element_type))) {
                    result = element_function(elt,
                                              element_type,
                                              lbs[0] + i,
                                              result);
                } else {
                    null_count++;
                }
            }else{
                null_count++;
            }

            /* advance bitmap pointers if any */
            if (bitmap) {
                bitmask <<= 1;
                if (bitmask == 0x100) {
                    bitmap++;
                    bitmask = 1;
                }
            }
        }
    } else {
        for (i = 0; i < nitems; i++) {
            Datum elt = fetch_att(dat, typbyval, type_size);
            dat = att_addlength_pointer(dat, type_size, dat);
            dat = (char *) att_align_nominal(dat, typalign);

            // treating NaN as NULL
            if (!isnan(datum_float8_cast(elt,element_type))) {
                result = element_function(elt,
                                          element_type,
                                          lbs[0] + i,
                                          result);
            } else {
                null_count++;
            }
        }
    }

    return finalize_function(result,(nitems-null_count), element_type);
}

/*
 * @brief Aggregates two arrays to a scalar.
 *
 * @param v1 Array.
 * @param v2 Array.
 * @param element_function Transition function.
 * @param finalize_function Final function.
 * @returns Whatever finalize_function returns.
 */
Datum
General_2Array_to_Element(
        ArrayType *v1,
        ArrayType *v2,
        Datum(*element_function)(Datum,Oid,Datum,Oid,Datum,Oid),
        Datum(*finalize_function)(Datum,int,Oid)) {

    // dimensions
    int ndims1 = ARR_NDIM(v1);
    int ndims2 = ARR_NDIM(v2);
    if (ndims1 != ndims2) {
        ereport(ERROR, (errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR),
                        errmsg("cannot perform operation arrays of different number of dimensions"),
                        errdetail("Arrays with %d and %d dimensions are not compatible for this opertation.",
                                  ndims1, ndims2)));
    }
    if (ndims2 == 0) {
        elog(WARNING, "input are empty arrays.");
        return Float8GetDatum(0);
    }
    int *lbs1 = ARR_LBOUND(v1);
    int *lbs2 = ARR_LBOUND(v2);
    int *dims1 = ARR_DIMS(v1);
    int *dims2 = ARR_DIMS(v2);
    int i = 0;
    for (i = 0; i < ndims1; i++) {
        if (dims1[i] != dims2[i] || lbs1[i] != lbs2[i]) {
            ereport(ERROR, (errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR),
                            errmsg("cannot operate on arrays of different ranges of dimensions"),
                            errdetail("Arrays with range [%d,%d] and [%d,%d] for dimension %d are not compatible for operations.",
                                      lbs1[i], lbs1[i] + dims1[i], lbs2[i], lbs2[i] + dims2[i], i)));
        }
    }
    int nitems = ArrayGetNItems(ndims1, dims1);

    // nulls
    if (ARR_HASNULL(v1) || ARR_HASNULL(v2)) {
        ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
                        errmsg("arrays cannot contain nulls"),
                        errdetail("Arrays with element value NULL are not allowed.")));
    }

    // type
    // the function signature guarantees v1 and v2 are of same type
    Oid element_type = ARR_ELEMTYPE(v1);
    TypeCacheEntry *typentry = lookup_type_cache(element_type,TYPECACHE_CMP_PROC_FINFO);
    int type_size = typentry->typlen;
    bool typbyval = typentry->typbyval;
    char typalign = typentry->typalign;

    // iterate
    Datum result = Float8GetDatum(0);
    char *dat1 = ARR_DATA_PTR(v1);
    char *dat2 = ARR_DATA_PTR(v2);
    for (i = 0; i < nitems; ++i) {
        Datum elt1 = fetch_att(dat1, typbyval, type_size);
        dat1 = att_addlength_pointer(dat1, type_size, dat1);
        dat1 = (char *) att_align_nominal(dat1, typalign);
        Datum elt2 = fetch_att(dat2, typbyval, type_size);
        dat2 = att_addlength_pointer(dat2, type_size, dat2);
        dat2 = (char *) att_align_nominal(dat2, typalign);

        result = element_function(elt1,
                                  element_type,
                                  result,
                                  FLOAT8OID,
                                  elt2,
                                  element_type);
    }

    return finalize_function(result, nitems, FLOAT8OID);
}

/*
 * @brief Maps two arrays to a single array.
 *
 * @param v1 Array.
 * @param v2 Array.
 * @param element_function Map function.
 * @returns Mapped array.
 */
ArrayType*
General_2Array_to_Array(
        ArrayType *v1,
        ArrayType *v2,
        Datum(*element_function)(Datum,Oid,Datum,Oid,Datum,Oid)) {

    // dimensions
    int ndims1 = ARR_NDIM(v1);
    int ndims2 = ARR_NDIM(v2);
    if (ndims1 != ndims2) {
        ereport(ERROR, (errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR),
                        errmsg("cannot perform operation arrays of different number of dimensions"),
                        errdetail("Arrays with %d and %d dimensions are not compatible for this opertation.",
                                  ndims1, ndims2)));
    }
    if (ndims2 == 0) {
        elog(WARNING, "input are empty arrays.");
        return v1;
    }
    int *lbs1 = ARR_LBOUND(v1);
    int *lbs2 = ARR_LBOUND(v2);
    int *dims1 = ARR_DIMS(v1);
    int *dims2 = ARR_DIMS(v2);
    // for output array
    int ndims = ndims1;
    int *dims = (int *) palloc(ndims * sizeof(int));
    int *lbs = (int *) palloc(ndims * sizeof(int));
    int i = 0;
    for (i = 0; i < ndims; i++) {
        if (dims1[i] != dims2[i] || lbs1[i] != lbs2[i]) {
            ereport(ERROR, (errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR),
                            errmsg("cannot operate on arrays of different ranges of dimensions"),
                            errdetail("Arrays with range [%d,%d] and [%d,%d] for dimension %d are not compatible for operations.",
                                      lbs1[i], lbs1[i] + dims1[i], lbs2[i], lbs2[i] + dims2[i], i)));
        }
        dims[i] = dims1[i];
        lbs[i] = lbs1[i];
    }
    int nitems = ArrayGetNItems(ndims, dims);

    // nulls
    if (ARR_HASNULL(v1) || ARR_HASNULL(v2)) {
        ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
                        errmsg("arrays cannot contain nulls"),
                        errdetail("Arrays with element value NULL are not allowed.")));
    }

    // type
    // the function signature guarantees v1 and v2 are of same type
    Oid element_type = ARR_ELEMTYPE(v1);
    TypeCacheEntry *typentry = lookup_type_cache(element_type,TYPECACHE_CMP_PROC_FINFO);
    int type_size = typentry->typlen;
    bool typbyval = typentry->typbyval;
    char typalign = typentry->typalign;

    // allocate
    Datum *result = NULL;
    switch (element_type) {
        case INT2OID:
        case INT4OID:
        case INT8OID:
        case FLOAT4OID:
        case FLOAT8OID:
        case NUMERICOID:
            result = (Datum *)palloc(nitems * sizeof(Datum));break;
        default:
            ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                            errmsg("type is not supported"),
                            errdetail("Arrays with element type %s are not supported.",
                                      format_type_be(element_type))));
            break;
    }

    // iterate
    Datum *resultp = result;
    char *dat1 = ARR_DATA_PTR(v1);
    char *dat2 = ARR_DATA_PTR(v2);
    for (i = 0; i < nitems; ++i) {
        Datum elt1 = fetch_att(dat1, typbyval, type_size);
        dat1 = att_addlength_pointer(dat1, type_size, dat1);
        dat1 = (char *) att_align_nominal(dat1, typalign);

        Datum elt2 = fetch_att(dat2, typbyval, type_size);
        dat2 = att_addlength_pointer(dat2, type_size, dat2);
        dat2 = (char *) att_align_nominal(dat2, typalign);

        *resultp = element_function(elt1,
                                    element_type,
                                    elt1,         /* placeholder */
                                    element_type, /* placeholder */
                                    elt2,
                                    element_type);
        resultp ++;
    }


    // construct return result
    ArrayType *pgarray = construct_md_array(result,
                                 NULL,
                                 ndims,
                                 dims,
                                 lbs,
                                 element_type,
                                 type_size,
                                 typbyval,
                                 typalign);

    pfree(result);
    pfree(dims);
    pfree(lbs);

    return pgarray;
}

/*
 * @brief Transforms an array.
 *
 * @param v1 Array.
 * @param elt2 Parameter.
 * @param element_function Map function.
 * @returns Transformed array.
 */
ArrayType*
General_Array_to_Array(
        ArrayType *v1,
        Datum elt2,
        Datum(*element_function)(Datum,Oid,Datum,Oid,Datum,Oid)) {

    // dimensions
    int ndims1 = ARR_NDIM(v1);
    if (ndims1 == 0) {
        elog(WARNING, "input are empty arrays.");
        return v1;
    }
    int ndims = ndims1;
    int *lbs1 = ARR_LBOUND(v1);
    int *dims1 = ARR_DIMS(v1);
    int *dims = (int *) palloc(ndims * sizeof(int));
    int *lbs = (int *) palloc(ndims * sizeof(int));
    int i = 0;
    for (i = 0; i < ndims; i ++) {
        dims[i] = dims1[i];
        lbs[i] = lbs1[i];
    }
    int nitems = ArrayGetNItems(ndims, dims);

    // nulls
    if (ARR_HASNULL(v1)) {
        ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
                        errmsg("arrays cannot contain nulls"),
                        errdetail("Arrays with element value NULL are not allowed.")));
    }

    // type
    Oid element_type = ARR_ELEMTYPE(v1);
    TypeCacheEntry *typentry = lookup_type_cache(element_type,TYPECACHE_CMP_PROC_FINFO);
    int type_size = typentry->typlen;
    bool typbyval = typentry->typbyval;
    char typalign = typentry->typalign;

    // allocate
    Datum *result = NULL;
    switch (element_type) {
        case INT2OID:
        case INT4OID:
        case INT8OID:
        case FLOAT4OID:
        case FLOAT8OID:
        case NUMERICOID:
            result = (Datum *)palloc(nitems * sizeof(Datum));break;
        default:
            ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                            errmsg("type is not supported"),
                            errdetail("Arrays with element type %s are not supported.",
                                      format_type_be(element_type))));
            break;
    }

    // iterate
    Datum *resultp = result;
    char *dat1 = ARR_DATA_PTR(v1);
    for (i = 0; i < nitems; i ++) {
        // iterate elt1
        Datum elt1 = fetch_att(dat1, typbyval, type_size);
        dat1 = att_addlength_pointer(dat1, type_size, dat1);
        dat1 = (char *) att_align_nominal(dat1, typalign);

        *resultp = element_function(elt1,
                                    element_type,
                                    elt1,         /* placeholder */
                                    element_type, /* placeholder */
                                    elt2,
                                    element_type);
        resultp ++;
    }

    // construct return result
    ArrayType *pgarray = construct_md_array(result,
                                            NULL,
                                            ndims,
                                            dims,
                                            lbs,
                                            element_type,
                                            type_size,
                                            typbyval,
                                            typalign);

    pfree(result);
    pfree(dims);
    pfree(lbs);

    return pgarray;
}

/******* internal functions ********/
