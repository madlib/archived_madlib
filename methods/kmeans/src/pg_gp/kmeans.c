#include <postgres.h>
#include <utils/builtins.h>
#include "../../../svec/src/pg_gp/sparse_vector.h"
#include "../../../svec/src/pg_gp/operators.h"

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
double
svec_svec_distance(Datum inVec1, Datum inVec2, KMeansMetric inMetric)
{
    PGFunction metric_fn = NULL;

    switch (inMetric)
    {
        case L1NORM: metric_fn = svec_svec_l1norm; break;
        case L2NORM: metric_fn = svec_svec_l2norm; break;
        case COSINE: metric_fn = svec_svec_angle; break;
        case TANIMOTO: metric_fn = svec_svec_tanimoto_distance; break;
        default:
            ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("invalid metric")));
    }
    
    return DatumGetFloat8(DirectFunctionCall2(metric_fn, inVec1, inVec2));    
}

PG_FUNCTION_INFO_V1(internal_get_array_of_close_canopies);
Datum
internal_get_array_of_close_canopies(PG_FUNCTION_ARGS)
{
    SvecType       *svec;
    Datum          *all_canopies;
    int             num_all_canopies;
    float8          threshold;
    KMeansMetric    metric;
    
    ArrayType      *close_canopies_arr;
    int4           *close_canopies;
    int             num_close_canopies;
    size_t          bytes;
    
    svec = PG_GETARG_SVECTYPE_P(verify_arg_nonnull(fcinfo, 0));
    get_svec_array_elms(PG_GETARG_ARRAYTYPE_P(verify_arg_nonnull(fcinfo, 1)),
        &all_canopies, &num_all_canopies);
    threshold = PG_GETARG_FLOAT8(verify_arg_nonnull(fcinfo, 2));
    metric = PG_GETARG_INT32(verify_arg_nonnull(fcinfo, 3));
    
    close_canopies = (int4 *) palloc(sizeof(int4) * num_all_canopies);
    num_close_canopies = 0;
    for (int i = 0; i < num_all_canopies; i++) {
        if (svec_svec_distance(PointerGetDatum(svec), all_canopies[i], metric)
            < threshold)
            close_canopies[num_close_canopies++] = i + 1 /* lower bound */;
    }

    bytes = ARR_OVERHEAD_NONULLS(1) + sizeof(int4) * num_close_canopies;
    close_canopies_arr = (ArrayType *) palloc0(bytes);
    SET_VARSIZE(close_canopies_arr, bytes);
    ARR_ELEMTYPE(close_canopies_arr) = INT4OID;
    ARR_NDIM(close_canopies_arr) = 1;
    ARR_DIMS(close_canopies_arr)[0] = num_close_canopies;
    ARR_LBOUND(close_canopies_arr)[0] = 1;
    memcpy(ARR_DATA_PTR(close_canopies_arr), close_canopies,
        sizeof(int4) * num_close_canopies);

    PG_RETURN_ARRAYTYPE_P(close_canopies_arr);
}

PG_FUNCTION_INFO_V1(internal_kmeans_closest_centroid);
Datum
internal_kmeans_closest_centroid(PG_FUNCTION_ARGS) {
    SvecType       *svec;
    ArrayType      *canopy_ids_arr = NULL;
    int4           *canopy_ids = NULL;
    ArrayType      *centroids_arr;
    Datum          *centroids;
    int             num_centroids;
    KMeansMetric    metric;

    bool            indirect;
    float8          distance, min_distance = INFINITY;
    int             closest_centroid = 0;
    int             cid;

    svec = PG_GETARG_SVECTYPE_P(verify_arg_nonnull(fcinfo, 0));
    if (PG_ARGISNULL(1)) {
        indirect = false;
    } else {
        indirect = true;
        canopy_ids_arr = PG_GETARG_ARRAYTYPE_P(1);
        canopy_ids = (int4*) ARR_DATA_PTR(canopy_ids_arr);
    }
    centroids_arr = PG_GETARG_ARRAYTYPE_P(verify_arg_nonnull(fcinfo, 2));
    get_svec_array_elms(centroids_arr, &centroids, &num_centroids);
    if (!PG_ARGISNULL(1))
        num_centroids = ARR_DIMS(canopy_ids_arr)[0];
    metric = PG_GETARG_INT32(verify_arg_nonnull(fcinfo, 3));

    for (int i = 0; i < num_centroids; i++) {
        cid = indirect ? canopy_ids[i] - ARR_LBOUND(canopy_ids_arr)[0] : i;
        distance = svec_svec_distance(PointerGetDatum(svec), centroids[cid],
            metric);
        if (distance < min_distance) {
            closest_centroid = cid;
            min_distance = distance;
        }
    }
    
    PG_RETURN_INT32(closest_centroid + ARR_LBOUND(centroids_arr)[0]);
}

PG_FUNCTION_INFO_V1(internal_kmeans_canopy_transition);
Datum
internal_kmeans_canopy_transition(PG_FUNCTION_ARGS) {
    ArrayType      *canopies_arr;
    Datum          *canopies;
    int             num_canopies;
    SvecType       *point;
    KMeansMetric    metric;
    float8          threshold;
    
    canopies_arr = PG_GETARG_ARRAYTYPE_P(verify_arg_nonnull(fcinfo, 0));
    get_svec_array_elms(canopies_arr, &canopies, &num_canopies);
    point = PG_GETARG_SVECTYPE_P(verify_arg_nonnull(fcinfo, 1));
    metric = PG_GETARG_INT32(verify_arg_nonnull(fcinfo, 2));
    threshold = PG_GETARG_FLOAT8(verify_arg_nonnull(fcinfo, 3));
    
    for (int i = 0; i < num_canopies; i++) {
        if (svec_svec_distance(PointerGetDatum(point), canopies[i], metric)
            < threshold)
            PG_RETURN_ARRAYTYPE_P(canopies_arr);
    }
    
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
