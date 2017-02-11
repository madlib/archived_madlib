#include <postgres.h>
#include <nodes/memnodes.h>
#if PG_VERSION_NUM >= 100000
#include <utils/regproc.h>
#else
#include <utils/builtins.h>
#endif
#include <utils/memutils.h>
#include <math.h>
#include "../../../svec/src/pg_gp/sparse_vector.h"
#include "../../../svec_util/src/pg_gp/svec_util.h"

typedef enum {
    L1NORM = 1,
    L2NORM,
    COSINE,
    TANIMOTO
} KMeansMetric;

static
inline
int
verify_arg_nonnull(PG_FUNCTION_ARGS, int inArgNo)
{
    if (PG_ARGISNULL(inArgNo))
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("function \"%s\" called with NULL argument",
                    format_procedure(fcinfo->flinfo->fn_oid))));
    return inArgNo;
}

static
inline
void
get_svec_array_elms(ArrayType *inArrayType, Datum **outSvecArr, int *outLen)
{
    deconstruct_array(inArrayType,           /* array */
                  ARR_ELEMTYPE(inArrayType), /* elmtype */
                  -1,                        /* elmlen */
                  false,                     /* elmbyval */
                  'd',                       /* elmalign */
                  outSvecArr,                /* elemsp */
                  NULL,                      /* nullsp -- pass NULL, because we 
                                                don't support NULLs */
                  outLen);                   /* nelemsp */
}


static
inline
PGFunction
get_metric_fn(KMeansMetric inMetric)
{
    PGFunction metrics[] = {
            svec_svec_l1norm,
            svec_svec_l2norm,
            svec_svec_angle,
            svec_svec_tanimoto_distance
        };
    
    if (inMetric < 1 || inMetric > sizeof(metrics)/sizeof(PGFunction))
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("invalid metric")));
    return metrics[inMetric - 1];
}

static
inline
double
compute_metric(PGFunction inMetricFn, MemoryContext inMemContext, Datum inVec1,
    Datum inVec2) {
    
    float8          distance;
    MemoryContext   oldContext;
    
    oldContext = MemoryContextSwitchTo(inMemContext);
    
    distance = DatumGetFloat8(DirectFunctionCall2(inMetricFn, inVec1, inVec2));
    
#ifdef GP_VERSION_NUM
    /*
     * Once the direct function calls have leaked enough memory, let's do some
     * garbage collection...
     * The 50k bound here is arbitrary, and motivated by ResetExprContext()
     * in execUtils.c
     */
    if(inMemContext->allBytesAlloc - inMemContext->allBytesFreed > 50000)
        MemoryContextReset(inMemContext);
#else
    /* PostgreSQL does not have the allBytesAlloc and allBytesFreed fields */
    MemoryContextReset(inMemContext);
#endif

    MemoryContextSwitchTo(oldContext);
    return distance;
}

static
MemoryContext
setup_mem_context_for_functional_calls() {
    MemoryContext ctxt = AllocSetContextCreate(CurrentMemoryContext,
        "kMeansMetricFnCalls",
        ALLOCSET_DEFAULT_MINSIZE,
        ALLOCSET_DEFAULT_INITSIZE,
        ALLOCSET_DEFAULT_MAXSIZE);
    return ctxt;
}

PG_FUNCTION_INFO_V1(internal_get_array_of_close_canopies);
Datum
internal_get_array_of_close_canopies(PG_FUNCTION_ARGS)
{
    SvecType       *svec;
    Datum          *all_canopies;
    int             num_all_canopies;
    float8          threshold;
    PGFunction      metric_fn;

    ArrayType      *close_canopies_arr;
    int32          *close_canopies;
    int             num_close_canopies;
    size_t          bytes;
    MemoryContext   mem_context_for_function_calls;

    svec = PG_GETARG_SVECTYPE_P(verify_arg_nonnull(fcinfo, 0));
    get_svec_array_elms(PG_GETARG_ARRAYTYPE_P(verify_arg_nonnull(fcinfo, 1)),
        &all_canopies, &num_all_canopies);
    threshold = PG_GETARG_FLOAT8(verify_arg_nonnull(fcinfo, 2));
    metric_fn = get_metric_fn(PG_GETARG_INT32(verify_arg_nonnull(fcinfo, 3)));

    mem_context_for_function_calls = setup_mem_context_for_functional_calls();
    close_canopies = (int32 *) palloc(sizeof(int32) * num_all_canopies);
    num_close_canopies = 0;
    for (int i = 0; i < num_all_canopies; i++) {
        if (compute_metric(metric_fn, mem_context_for_function_calls,
                PointerGetDatum(svec), all_canopies[i]) < threshold)
            close_canopies[num_close_canopies++] = i + 1 /* lower bound */;
    }
    MemoryContextDelete(mem_context_for_function_calls);

    /* If we cannot find any close canopy, return NULL. Note that the result
     * we return will be passed to internal_kmeans_closest_centroid() and if the
     * array of close canopies is NULL, then internal_kmeans_closest_centroid()
     * will consider and compute the distance to all centroids. */
    if (num_close_canopies == 0)
        PG_RETURN_NULL();

    bytes = ARR_OVERHEAD_NONULLS(1) + sizeof(int32) * num_close_canopies;
    close_canopies_arr = (ArrayType *) palloc0(bytes);
    SET_VARSIZE(close_canopies_arr, bytes);
    ARR_ELEMTYPE(close_canopies_arr) = INT4OID;
    ARR_NDIM(close_canopies_arr) = 1;
    ARR_DIMS(close_canopies_arr)[0] = num_close_canopies;
    ARR_LBOUND(close_canopies_arr)[0] = 1;
    memcpy(ARR_DATA_PTR(close_canopies_arr), close_canopies,
        sizeof(int32) * num_close_canopies);

    PG_RETURN_ARRAYTYPE_P(close_canopies_arr);
}

static float8 calc_l2norm_distance(float8* array1, float8* array2, int32 dimension)
{
    if( array1 == NULL || array2 == NULL )
    {
        elog(ERROR, "In %s, arrays should not be NULL", __func__);
    }

    float8 distance =0;
    for(int index=0; index<dimension; index++)
    {
        float8 temp_val = array1[index]-array2[index];
        distance += temp_val*temp_val;
    }
    distance = sqrt(distance);
    return distance;
}


static float8 calc_l1norm_distance(float8* array1, float8* array2, int32 dimension)
{
    if( array1 == NULL || array2 == NULL )
    {
        elog(ERROR, "In %s, arrays should not be NULL", __func__);
    }

    float8 distance =0;
    for(int index=0; index<dimension; index++)
    {
        float8 temp_val = array1[index]-array2[index];
        distance += fabs(temp_val);
    }
    return distance;
}


static float8 calc_dot_product(float8* array1, float8* array2, int32 dimension)
{
    float8 val =0;
    for(int index=0; index<dimension; index++)
    {
        val += array1[index]*array2[index];
    }
    return val;
}

static float8 calc_l2norm_val(float8* array, int32 dimension)
{
    float8 val =0;
    for(int index=0; index<dimension; index++)
    {
        val += array[index]*array[index];
    }
    return sqrt(val);
}

static float8 calc_cosine_distance(float8* array1, float8* array2, int32 dimension)
{
    if( array1 == NULL || array2 == NULL )
    {
        elog(ERROR, "In %s, arrays should not be NULL", __func__);
    }

    float8 dot_product = calc_dot_product(array1, array2, dimension);
    float8 array1_l2norm = calc_l2norm_val(array1, dimension);
    float8 array2_l2norm = calc_l2norm_val(array2, dimension);

    float8 distance = dot_product/(array1_l2norm*array2_l2norm);

    if (distance > 1.0)
    {
        distance = 1.0;
    }
    else if (distance < -1.0)
    {
        distance = -1.0;
    }

    return acos(distance);
}


static float8 calc_tanimoto_distance(float8* array1, float8* array2, int32 dimension)
{
    if( array1 == NULL || array2 == NULL )
    {
        elog(ERROR, "In %s, arrays should not be NULL", __func__);
    }

    float8 dot_product = calc_dot_product(array1, array2, dimension);
    float8 array1_l2norm = calc_l2norm_val(array1, dimension);
    float8 array2_l2norm = calc_l2norm_val(array2, dimension);

    float8 denominator = array1_l2norm*array1_l2norm+
        array2_l2norm*array2_l2norm-dot_product;
    float8 distance = dot_product/denominator;

    if (distance > 1.0)
    {
        distance = 1.0;
    }
    else if (distance < 0)
    {
        distance = 0;
    }

    return 1. - distance;
}

typedef float8  (*MetricFunc)(float8*, float8*, int32);
static
inline
MetricFunc
get_metric_fn_for_array(KMeansMetric inMetric)
{
    MetricFunc metrics[] = {
            calc_l1norm_distance,
            calc_l2norm_distance,
            calc_cosine_distance,
            calc_tanimoto_distance
        };

    if (inMetric < 1 || inMetric > sizeof(metrics)/sizeof(PGFunction)) {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("invalid metric")));
        return NULL;
    } else {
        return metrics[inMetric - 1];
    }
}

PG_FUNCTION_INFO_V1(internal_kmeans_closest_centroid);
Datum
internal_kmeans_closest_centroid(PG_FUNCTION_ARGS) {
    ArrayType      *point_array;
    ArrayType      *centroids_array;

    float8          distance, min_distance = INFINITY;
    int             closest_centroid = 0;
    int             cid;

    point_array = PG_GETARG_ARRAYTYPE_P(verify_arg_nonnull(fcinfo, 0));
    float8* c_point_array = (float8 *)ARR_DATA_PTR(point_array);
    centroids_array = PG_GETARG_ARRAYTYPE_P(verify_arg_nonnull(fcinfo, 1));
    float8* c_centroids_array = (float8 *)ARR_DATA_PTR(centroids_array);

    int dimension = PG_GETARG_INT32(verify_arg_nonnull(fcinfo, 2));
    int num_of_centroids = PG_GETARG_INT32(verify_arg_nonnull(fcinfo, 3));
    int centroids_array_len = num_of_centroids*dimension;
    int dist_metric = PG_GETARG_INT32(verify_arg_nonnull(fcinfo, 4));

    ArrayType      *canopy_ids_arr = NULL;
    int32          *canopy_ids = NULL;
    bool            indirect;
    if (PG_ARGISNULL(5))
    {
        indirect = false;
    }
    else
    {
        indirect = true;
        canopy_ids_arr = PG_GETARG_ARRAYTYPE_P(5);
        /* There should always be a close canopy, but let's be on the safe side. */
        if (ARR_NDIM(canopy_ids_arr) == 0)
            ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("internal error: array of close canopies cannot be empty")));
        canopy_ids = (int32*) ARR_DATA_PTR(canopy_ids_arr);
        num_of_centroids = ARR_DIMS(canopy_ids_arr)[0];
    }

    if (dimension < 1)
    {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("function \"%s\", Invalid dimension:%d",
                    format_procedure(fcinfo->flinfo->fn_oid),
                    dimension)));
    }

    if (num_of_centroids < 1)
    {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("function \"%s\", Invalid num_of_centroids:%d",
                    format_procedure(fcinfo->flinfo->fn_oid),
                    num_of_centroids)));
    }

    int array_dim = ARR_NDIM(point_array);
    int *p_array_dim = ARR_DIMS(point_array);
    int array_length = ArrayGetNItems(array_dim, p_array_dim);

    if (array_length != dimension)
    {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("function \"%s\", Invalid point array length. "
                    "Expected: %d, Actual:%d",
                    format_procedure(fcinfo->flinfo->fn_oid),
                    dimension, array_length)));
    }

    array_dim = ARR_NDIM(centroids_array);
    p_array_dim = ARR_DIMS(centroids_array);
    array_length = ArrayGetNItems(array_dim, p_array_dim);

    if (array_length != centroids_array_len)
    {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("function \"%s\", Invalid centroids array length. "
                    "Expected: %d, Actual:%d",
                    format_procedure(fcinfo->flinfo->fn_oid),
                    centroids_array_len, array_length)));
    }

    for (int i = 0; i< num_of_centroids; i++)
    {
        cid = indirect ? canopy_ids[i] - ARR_LBOUND(canopy_ids_arr)[0] : i;
	    double * centroid = c_centroids_array+cid*dimension;

        MetricFunc func = get_metric_fn_for_array(dist_metric);
        distance = (*func)(centroid, c_point_array, dimension);

        if (distance < min_distance) {
            closest_centroid = cid;
            min_distance = distance;
        }
    }

    PG_RETURN_INT32(closest_centroid+ARR_LBOUND(centroids_array)[0]);
}


PG_FUNCTION_INFO_V1(internal_kmeans_agg_centroid_trans);
Datum
internal_kmeans_agg_centroid_trans(PG_FUNCTION_ARGS) {
    ArrayType       *array = NULL;
    ArrayType       *cent_array = NULL;
    int32           dimension;
    int32           num_of_centroids;
    int32           centroid_index;
    bool            rebuild_array = false;
    int32           expected_array_len;

    float8          *c_array = NULL;
    cent_array = PG_GETARG_ARRAYTYPE_P(verify_arg_nonnull(fcinfo, 1));

    int array_dim = ARR_NDIM(cent_array);
    int *p_array_dim = ARR_DIMS(cent_array);
    int array_length = ArrayGetNItems(array_dim, p_array_dim);
    float8* c_cent_array = (float8 *)ARR_DATA_PTR(cent_array);

    dimension = PG_GETARG_INT32(verify_arg_nonnull(fcinfo, 2));
    num_of_centroids = PG_GETARG_INT32(verify_arg_nonnull(fcinfo, 3));
    centroid_index = PG_GETARG_INT32(verify_arg_nonnull(fcinfo, 4));

    expected_array_len = num_of_centroids*dimension;
    if (dimension < 1)
    {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("function \"%s\", Invalid dimension:%d",
                    format_procedure(fcinfo->flinfo->fn_oid),
                    dimension)));
    }

    if (array_length != dimension)
    {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("function \"%s\", Inconsistent Dimension. "
                     "Expected:%d, Actual:%d",
                    format_procedure(fcinfo->flinfo->fn_oid),
                    dimension, array_length)));

    }

    if (num_of_centroids < 1)
    {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("function \"%s\", Invalid num_of_centroids:%d",
                    format_procedure(fcinfo->flinfo->fn_oid),
                    num_of_centroids)));
    }

    if (centroid_index < 1 || centroid_index>num_of_centroids)
    {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("function \"%s\", Invalid centroid_index:%d",
                    format_procedure(fcinfo->flinfo->fn_oid),
                    centroid_index)));
    }

    if (PG_ARGISNULL(0))
    {
        c_array = palloc0(expected_array_len*sizeof(float8));
        rebuild_array = true;
    }
    else
    {
        if (fcinfo->context && IsA(fcinfo->context, AggState))
            array = PG_GETARG_ARRAYTYPE_P(0);
        else
            array = PG_GETARG_ARRAYTYPE_P_COPY(0);

        array_dim = ARR_NDIM(array);
        p_array_dim = ARR_DIMS(array);
        array_length = ArrayGetNItems(array_dim, p_array_dim);

        if (array_length != expected_array_len)
        {
            ereport(ERROR,
                    (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                     errmsg("function \"%s\", Invalid array length. "
                        "Expected: %d, Actual:%d",
                        format_procedure(fcinfo->flinfo->fn_oid),
                        expected_array_len, array_length)));
        }
        c_array = (float8 *)ARR_DATA_PTR(array);
    }


    float8 * data_ptr = c_array+(centroid_index-1)*dimension;
    for(int index=0; index<dimension; index++)
    {
        data_ptr[index] = c_cent_array[index];
    }

    if (rebuild_array)
    {
        /* construct a new array to keep the aggr states. */
        array =
        	construct_array(
        		(Datum *)c_array,
                expected_array_len,
                FLOAT8OID,
                sizeof(float8),
                true,
                'd'
                );
    }
    PG_RETURN_ARRAYTYPE_P(array);
}


PG_FUNCTION_INFO_V1(internal_kmeans_agg_centroid_merge);
Datum
internal_kmeans_agg_centroid_merge(PG_FUNCTION_ARGS) {
    /* This function is declared as strict. No checking null here. */
    ArrayType       *array = NULL;
    ArrayType       *array2 = NULL;
    if (fcinfo->context && IsA(fcinfo->context, AggState))
        array = PG_GETARG_ARRAYTYPE_P(0);
    else
        array = PG_GETARG_ARRAYTYPE_P_COPY(0);

    int array_dim = ARR_NDIM(array);
    int *p_array_dim = ARR_DIMS(array);
    int array_length = ArrayGetNItems(array_dim, p_array_dim);

    array2 = PG_GETARG_ARRAYTYPE_P(1);
    array_dim = ARR_NDIM(array2);
    p_array_dim = ARR_DIMS(array2);
    int array2_length = ArrayGetNItems(array_dim, p_array_dim);

    if (array_length != array2_length)
    {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("function \"%s\", Inconsistent array length. "
                    "first: %d, second:%d",
                    format_procedure(fcinfo->flinfo->fn_oid),
                    array_length, array2_length)));
    }

    float8* c_array = (float8 *)ARR_DATA_PTR(array);
    float8* c_array2 = (float8 *)ARR_DATA_PTR(array2);

    for(int i=0; i<array_length; i++)
    {
        c_array[i]+= c_array2[i];
    }
    PG_RETURN_ARRAYTYPE_P(array);
}


PG_FUNCTION_INFO_V1(internal_kmeans_canopy_transition);
Datum
internal_kmeans_canopy_transition(PG_FUNCTION_ARGS) {
    ArrayType      *canopies_arr;
    Datum          *canopies;
    int             num_canopies;
    SvecType       *point;
    PGFunction      metric_fn;
    float8          threshold;

    MemoryContext   mem_context_for_function_calls;

    canopies_arr = PG_GETARG_ARRAYTYPE_P(verify_arg_nonnull(fcinfo, 0));
    get_svec_array_elms(canopies_arr, &canopies, &num_canopies);
    point = PG_GETARG_SVECTYPE_P(verify_arg_nonnull(fcinfo, 1));
    metric_fn = get_metric_fn(PG_GETARG_INT32(verify_arg_nonnull(fcinfo, 2)));
    threshold = PG_GETARG_FLOAT8(verify_arg_nonnull(fcinfo, 3));

    mem_context_for_function_calls = setup_mem_context_for_functional_calls();
    for (int i = 0; i < num_canopies; i++) {
        if (compute_metric(metric_fn, mem_context_for_function_calls,
            PointerGetDatum(point), canopies[i]) < threshold)
            PG_RETURN_ARRAYTYPE_P(canopies_arr);
    }
    MemoryContextDelete(mem_context_for_function_calls);

    int idx = (ARR_NDIM(canopies_arr) == 0)
        ? 1
        : ARR_LBOUND(canopies_arr)[0] + ARR_DIMS(canopies_arr)[0];
    return PointerGetDatum(
        array_set(
            canopies_arr, /* array: the initial array object (mustn't be NULL) */
            1, /* nSubscripts: number of subscripts supplied */
            &idx, /* indx[]: the subscript values */
            PointerGetDatum(point), /* dataValue: the datum to be inserted at the given position */
            false, /* isNull: whether dataValue is NULL */
            -1, /* arraytyplen: pg_type.typlen for the array type */
            -1, /* elmlen: pg_type.typlen for the array's element type */
            false, /* elmbyval: pg_type.typbyval for the array's element type */
            'd') /* elmalign: pg_type.typalign for the array's element type */
        );
}

PG_FUNCTION_INFO_V1(internal_remove_close_canopies);
Datum
internal_remove_close_canopies(PG_FUNCTION_ARGS) {
    ArrayType      *all_canopies_arr;
    Datum          *all_canopies;
    int             num_all_canopies;
    PGFunction      metric_fn;
    float8          threshold;

    Datum          *close_canopies;
    int             num_close_canopies;
    bool            addIndexI;
    MemoryContext   mem_context_for_function_calls;

    all_canopies_arr = PG_GETARG_ARRAYTYPE_P(verify_arg_nonnull(fcinfo, 0));
    get_svec_array_elms(all_canopies_arr, &all_canopies, &num_all_canopies);
    metric_fn = get_metric_fn(PG_GETARG_INT32(verify_arg_nonnull(fcinfo, 1)));
    threshold = PG_GETARG_FLOAT8(verify_arg_nonnull(fcinfo, 2));

    mem_context_for_function_calls = setup_mem_context_for_functional_calls();
    close_canopies = (Datum *) palloc(sizeof(Datum) * num_all_canopies);
    num_close_canopies = 0;
    for (int i = 0; i < num_all_canopies; i++) {
        addIndexI = true;
        for (int j = 0; j < num_close_canopies; j++) {
            if (compute_metric(metric_fn, mem_context_for_function_calls,
                all_canopies[i], close_canopies[j]) < threshold) {

                addIndexI = false;
                break;
            }
        }
        if (addIndexI)
            close_canopies[num_close_canopies++] = all_canopies[i];
    }
    MemoryContextDelete(mem_context_for_function_calls);

    PG_RETURN_ARRAYTYPE_P(
        construct_array(
            close_canopies, /* elems */
            num_close_canopies, /* nelems */
            ARR_ELEMTYPE(all_canopies_arr), /* elmtype */
            -1, /* elmlen */
            false, /* elmbyval */
            'd') /* elmalign */
        );
}
