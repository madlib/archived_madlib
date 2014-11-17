#include <postgres.h>

#include <stdio.h>
#include <string.h>
#include <search.h>
#include <stdlib.h>
#include <math.h>

#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "catalog/pg_type.h"
#include "libpq/pqformat.h"
#include "fmgr.h"
#include "funcapi.h"
#include "utils/fmgroids.h"
#include "lib/stringinfo.h"
#include "utils/memutils.h"
#include "access/hash.h"

#include "../../../svec/src/pg_gp/sparse_vector.h"

#ifndef NO_PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

/*
 * Basic floating point operators like MIN,MAX
 */
PG_FUNCTION_INFO_V1( float8_min );
Datum float8_min(PG_FUNCTION_ARGS);
Datum float8_min(PG_FUNCTION_ARGS)
{
	if (PG_ARGISNULL(0)) {
		if (PG_ARGISNULL(1)) PG_RETURN_NULL();
		else PG_RETURN_FLOAT8(PG_GETARG_FLOAT8(1));
	} else if (PG_ARGISNULL(1))
		PG_RETURN_FLOAT8(PG_GETARG_FLOAT8(0));

	float8 left = PG_GETARG_FLOAT8(0);
	float8 right = PG_GETARG_FLOAT8(1);
	PG_RETURN_FLOAT8(Min(left,right));
}

PG_FUNCTION_INFO_V1( float8_max );
Datum float8_max(PG_FUNCTION_ARGS);
Datum float8_max(PG_FUNCTION_ARGS)
{
	if (PG_ARGISNULL(0)) {
		if (PG_ARGISNULL(1)) PG_RETURN_NULL();
		else PG_RETURN_FLOAT8(PG_GETARG_FLOAT8(1));
	} else if (PG_ARGISNULL(1))
		PG_RETURN_FLOAT8(PG_GETARG_FLOAT8(0));

	float8 left = PG_GETARG_FLOAT8(0);
	float8 right = PG_GETARG_FLOAT8(1);
	PG_RETURN_FLOAT8(Max(left,right));
}

PG_FUNCTION_INFO_V1(svec_to_string);
/**
 *  svec_to_string - converts a sparse vector to a text
 */
Datum svec_to_string(PG_FUNCTION_ARGS)
{
	SvecType *svec = PG_GETARG_SVECTYPE_P(0);
	char *result = svec_out_internal(svec);
	PG_RETURN_TEXT_P(cstring_to_text(result));
}

PG_FUNCTION_INFO_V1(svec_from_string);
/**
 *  svec_from_string - converts a text into an svec
 */
Datum svec_from_string(PG_FUNCTION_ARGS)
{
	char *str = pstrdup(text_to_cstring(PG_GETARG_TEXT_P(0)));
	SvecType *result = svec_in_internal(str);
	PG_RETURN_SVECTYPE_P(result);
}

typedef struct
{
	SvecType *svec;
	SparseData sdata;
	int dimension;
	int absolute_value_position;
	int unique_value_position;
	int run_position;
	char *index_position;
}	svec_unnest_fctx;

/**
 * Turns an svec into a table of values
 */
PG_FUNCTION_INFO_V1(svec_unnest);
Datum svec_unnest(PG_FUNCTION_ARGS)
{
        FuncCallContext *funcctx;
        svec_unnest_fctx *fctx;
        MemoryContext oldcontext;
	float8 result;
	SvecType *svec;
	int run_length=0;

        /* stuff done only on the first call of the function */
        if (SRF_IS_FIRSTCALL())
        {
                /* create a function context for cross-call persistence */
                funcctx = SRF_FIRSTCALL_INIT();

                /* switch to memory context appropriate for multiple
		 * function calls
                 */
                oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

                /* allocate memory for user context */
                fctx = (svec_unnest_fctx *)
			palloc(sizeof(svec_unnest_fctx));

		svec   = PG_GETARG_SVECTYPE_P(0);
		fctx->sdata = sdata_from_svec(svec);

		/* set initial index into the sparse vector argument */
		fctx->dimension               = svec->dimension;
		if (fctx->dimension == -1) fctx->dimension = 1;
		fctx->absolute_value_position = 0;
		fctx->unique_value_position   = 0;
		fctx->index_position          = fctx->sdata->index->data;
		fctx->run_position            = 1;

                funcctx->user_fctx = fctx;

                MemoryContextSwitchTo(oldcontext);
        }

        /* stuff done on every call of the function */
	funcctx = SRF_PERCALL_SETUP();

        fctx = funcctx->user_fctx;

	run_length = compword_to_int8(fctx->index_position);

	if (fctx->dimension > fctx->absolute_value_position)
	{
		result = ((float8 *)(fctx->sdata->vals->data))
			[fctx->unique_value_position];

		fctx->absolute_value_position++;
		fctx->run_position++;
		if (fctx->run_position > run_length)
		{
			fctx->run_position = 1;
			fctx->unique_value_position++;
			fctx->index_position += int8compstoragesize(fctx->index_position);
		}

                /* send the result */
		if (IS_NVP(result)) {
			/* Here we simply copied the SRF_RETURN_NEXT macro, but
			   return null in the last line */
			do {
				ReturnSetInfo * rsi;
				funcctx->call_cntr++;
				rsi = (ReturnSetInfo *) fcinfo->resultinfo;
				rsi->isDone = ExprMultipleResult;
				PG_RETURN_NULL();
			} while (0);
		}
                else SRF_RETURN_NEXT(funcctx, Float8GetDatum(result));
        }
        else
	{
                /* do when there is no more left */
                SRF_RETURN_DONE(funcctx);
	}

}


///////////////////////////////////////////
// The following code are from operator.c//
///////////////////////////////////////////

/**
 *  svec_dimension - returns the number of elements in an svec
 */
Datum svec_dimension(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1( svec_dimension );

Datum svec_dimension(PG_FUNCTION_ARGS)
{
	SvecType *svec = PG_GETARG_SVECTYPE_P(0);
	if (svec->dimension == -1) PG_RETURN_INT32(1);
	else PG_RETURN_INT32(svec->dimension);
}

/**
 *  svec_lapply - applies a function to every element of an svec
 */
Datum svec_lapply(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(svec_lapply);

Datum svec_lapply(PG_FUNCTION_ARGS)
{
	if (PG_ARGISNULL(0) || PG_ARGISNULL(1))
		PG_RETURN_NULL();

	text *func = PG_GETARG_TEXT_P(0);
	SvecType *svec = PG_GETARG_SVECTYPE_P(1);
	SparseData in = sdata_from_svec(svec);
	PG_RETURN_SVECTYPE_P(svec_from_sparsedata(lapply(func,in),true));
}


/**
 *  svec_append - appends a block (count,value) to the end of an svec
 */
Datum svec_append(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(svec_append);
Datum svec_append(PG_FUNCTION_ARGS)
{
	float8 newele;
	int64 run_len;
	SvecType *svec;
	SparseData sdata;

	if (PG_ARGISNULL(2))
		ereport(ERROR,
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 errmsg("count argument cannot be null")));

	run_len = PG_GETARG_INT64(2);

	if (PG_ARGISNULL(1))
		newele = NVP;
	else newele = PG_GETARG_FLOAT8(1);

	if (PG_ARGISNULL(0))
		sdata = makeSparseData();
	else {
		svec = PG_GETARG_SVECTYPE_P(0);
		sdata = makeSparseDataCopy(sdata_from_svec(svec));
	}

	add_run_to_sdata((char *)(&newele), run_len, sizeof(float8), sdata);
	PG_RETURN_SVECTYPE_P(svec_from_sparsedata(sdata, true));
}


/**
 *  svec_proj - projects onto an element of an svec
 */
Datum svec_proj(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1( svec_proj );
Datum svec_proj(PG_FUNCTION_ARGS)
{
	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();

	SvecType * sv = PG_GETARG_SVECTYPE_P(0);
	int idx = PG_GETARG_INT32(1);

	SparseData in = sdata_from_svec(sv);
	double ret = sd_proj(in,idx);

	if (IS_NVP(ret)) PG_RETURN_NULL();

	PG_RETURN_FLOAT8(sd_proj(in,idx));
}

/**
 *  svec_subvec - computes a subvector of an svec
 */
Datum svec_subvec(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1( svec_subvec );
Datum svec_subvec(PG_FUNCTION_ARGS)
{
	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();

	SvecType * sv = PG_GETARG_SVECTYPE_P(0);
	int start = PG_GETARG_INT32(1);
	int end   = PG_GETARG_INT32(2);

	SparseData in = sdata_from_svec(sv);
	PG_RETURN_SVECTYPE_P(svec_from_sparsedata(subarr(in,start,end),true));
}

/**
 *  svec_reverse - makes a copy of the input svec, with the order of
 *                 the elements reversed
 */
Datum svec_reverse(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1( svec_reverse );
Datum svec_reverse(PG_FUNCTION_ARGS)
{
	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();

	SvecType * sv = PG_GETARG_SVECTYPE_P(0);
	SparseData in = sdata_from_svec(sv);
	PG_RETURN_SVECTYPE_P(svec_from_sparsedata(reverse(in),true));
}

/**
 *  svec_change - makes a copy of the input svec, with the subvector
 *                starting at a given location changed to another input svec.
 */
Datum svec_change(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(svec_change);
Datum svec_change(PG_FUNCTION_ARGS)
{
	SvecType * in = PG_GETARG_SVECTYPE_P(0);
	int idx = PG_GETARG_INT32(1);
	SvecType * changed = PG_GETARG_SVECTYPE_P(2);
	SparseData indata = sdata_from_svec(in);
	SparseData middle = sdata_from_svec(changed);
	int inlen = indata->total_value_count;
	int midlen = middle->total_value_count;
	SparseData head = NULL, tail = NULL, ret = NULL;

	Assert((IS_SCALAR(changed) && midlen == 1) ||
		   (midlen == changed->dimension));

	if (idx < 1 || idx > inlen)
		ereport(ERROR,
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 errmsg("Invalid start index")));

	if (idx+midlen-1 > inlen)
		ereport(ERROR,
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 errmsg("Change vector is too long")));

	if (idx >= 2) head = subarr(indata, 1, idx-1);
	if (idx + midlen <= inlen) tail = subarr(indata,idx + midlen, inlen);

	if (head == NULL && tail == NULL)
		ret = makeSparseDataCopy(middle);
	else if (head == NULL)
		ret = concat(middle, tail);
	else if (tail == NULL)
		ret = concat(head, middle);
	else {
		ret = concat(head, middle);
		ret = concat(ret, tail);
	}
	PG_RETURN_SVECTYPE_P(svec_from_sparsedata(ret,true));
}

/**
 *  svec_eq - returns the equality of two svecs if their
 *  non-zero elments are equal. Zero is viewed as a missing data
 *  and hence equals to any other value.
 */
Datum svec_eq_non_zero(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1( svec_eq_non_zero );
Datum svec_eq_non_zero(PG_FUNCTION_ARGS)
{
	SvecType *svec1 = PG_GETARG_SVECTYPE_P(0);
	SvecType *svec2 = PG_GETARG_SVECTYPE_P(1);
	SparseData left  = sdata_from_svec(svec1);
	SparseData right = sdata_from_svec(svec2);
	PG_RETURN_BOOL(sparsedata_eq_zero_is_equal(left,right));
}

/**
 *  svec_eq - returns the equality of two svecs if their
 *  non-zero elments are equal. Zero is viewed as a missing data
 *  and hence equals to any other value.
 */
Datum svec_contains(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1( svec_contains );
Datum svec_contains(PG_FUNCTION_ARGS)
{
	SvecType *svec1 = PG_GETARG_SVECTYPE_P(0);
	SvecType *svec2 = PG_GETARG_SVECTYPE_P(1);
	SparseData left  = sdata_from_svec(svec1);
	SparseData right = sdata_from_svec(svec2);
	PG_RETURN_BOOL(sparsedata_contains(left,right));
}

PG_FUNCTION_INFO_V1( svec_count );
/**
 *  svec_count - Count the number of non-zero entries in the input vector
 *               Right argument is capped at 1, then added to the left
 */
Datum svec_count(PG_FUNCTION_ARGS)
{
	SvecType * svec1 = PG_GETARG_SVECTYPE_P(0);
	SvecType * svec2 = PG_GETARG_SVECTYPE_P(1);
	SparseData left = sdata_from_svec(svec1);
	SparseData right = sdata_from_svec(svec2);

	if (IS_SCALAR(svec1)) {
		/*
		 * If the left argument is {1}:{0}, this is the first call to
		 * the routine, and we need a zero vector for the beginning
		 * of the accumulation of the correct dimension.
		 */
		double * left_vals = (double *)(left->vals->data);
		if (left_vals[0] == 0)
			left = makeSparseDataFromDouble(0.,right->total_value_count);
	}
	double *right_vals=(double *)(right->vals->data);
	SvecType *result;
	double *clamped_vals;
	SparseData right_clamped,sdata_result;

	if (left->total_value_count != right->total_value_count)
		ereport(ERROR,
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 errmsg("Array dimension of inputs are not the same: dim1=%d, dim2=%d\n",
				left->total_value_count, right->total_value_count)));

	/* Create an array of values either 1 or 0 depending on whether
	 * the right vector has a non-zero value in it
	 */
	clamped_vals =
		(double *)palloc0(sizeof(double)*(right->unique_value_count));

	for (int i=0;i<(right->unique_value_count);i++)
	{
		if (right_vals[i] != 0. && !IS_NVP(right_vals[i]))
			clamped_vals[i] = 1.;
	}
	right_clamped = makeInplaceSparseData(
			     (char *)clamped_vals,right->index->data,
			     right->vals->len,right->index->len,FLOAT8OID,
			     right->unique_value_count,
			     right->total_value_count);

	/* Create the output SVEC */
	sdata_result = op_sdata_by_sdata(add,left,right_clamped);
	result = svec_from_sparsedata(sdata_result,true);

	pfree(clamped_vals);
	pfree(right_clamped);

	PG_RETURN_SVECTYPE_P(result);
}

PG_FUNCTION_INFO_V1( svec_l2norm );
/**
 *  svec_l2norm - computes the l2 norm of an svec
 */
Datum svec_l2norm(PG_FUNCTION_ARGS)
{
	SvecType *svec = PG_GETARG_SVECTYPE_P(0);
	SparseData sdata  = sdata_from_svec(svec);
	double accum;
	accum = l2norm_sdata_values_double(sdata);

	if (IS_NVP(accum)) PG_RETURN_NULL();

	PG_RETURN_FLOAT8(accum);
}

PG_FUNCTION_INFO_V1( svec_svec_l2norm );
/**
 *  svec_svec_l2norm - Computes the l2norm distance between two SVECs.
 */
Datum svec_svec_l2norm(PG_FUNCTION_ARGS)
{
	SvecType *svec1 = PG_GETARG_SVECTYPE_P(0);
	SvecType *svec2 = PG_GETARG_SVECTYPE_P(1);

	check_dimension(svec1,svec2,"l2norm");
	SvecType *result = op_svec_by_svec_internal(subtract,svec1,svec2);

	SparseData sdata  = sdata_from_svec(result);
	double accum;
	accum = l2norm_sdata_values_double(sdata);

	if (IS_NVP(accum)) PG_RETURN_NULL();

	PG_RETURN_FLOAT8(accum);
}

PG_FUNCTION_INFO_V1( svec_svec_l1norm );
/**
 *  svec_svec_l1norm - Computes the l1norm distance between two SVECs.
 */
Datum svec_svec_l1norm(PG_FUNCTION_ARGS)
{
	SvecType *svec1 = PG_GETARG_SVECTYPE_P(0);
	SvecType *svec2 = PG_GETARG_SVECTYPE_P(1);

	check_dimension(svec1,svec2,"l1norm");
	SvecType *result = op_svec_by_svec_internal(subtract,svec1,svec2);

	SparseData sdata = sdata_from_svec(result);
	double accum;
	accum = l1norm_sdata_values_double(sdata);

	if (IS_NVP(accum)) PG_RETURN_NULL();

	PG_RETURN_FLOAT8(accum);
}

PG_FUNCTION_INFO_V1( svec_svec_angle );
/**
 *  svec_svec_angle - Computes the angle between two SVECs.
 */
Datum svec_svec_angle(PG_FUNCTION_ARGS)
{
	SvecType *svec1 = PG_GETARG_SVECTYPE_P(0);
	SvecType *svec2 = PG_GETARG_SVECTYPE_P(1);
	SparseData left  = sdata_from_svec(svec1);
	SparseData right = sdata_from_svec(svec2);
	double dot, m1, m2, result;

	dot = svec_svec_dot_product( svec1, svec2);

	m1 = l2norm_sdata_values_double(left);
	m2 = l2norm_sdata_values_double(right);

	if (IS_NVP(dot) || IS_NVP(m1) || IS_NVP(m2)) PG_RETURN_NULL();

	result = dot/(m1*m2);

	if (result > 1.0) {
		result = 1.0;
	}
	else if (result < -1.0) {
		result = -1.0;
	}

	PG_RETURN_FLOAT8(acos(result));
}

PG_FUNCTION_INFO_V1( svec_svec_tanimoto_distance );
/**
 *  svec_svec_tanimoto_distance - Computes the Tanimoto similarity between two SVECs.
 */
Datum svec_svec_tanimoto_distance(PG_FUNCTION_ARGS)
{
	SvecType *svec1 = PG_GETARG_SVECTYPE_P(0);
	SvecType *svec2 = PG_GETARG_SVECTYPE_P(1);
	SparseData left  = sdata_from_svec(svec1);
	SparseData right = sdata_from_svec(svec2);
	double dot, m1, m2, result;

	dot = svec_svec_dot_product( svec1, svec2);

	m1 = l2norm_sdata_values_double(left);
	m2 = l2norm_sdata_values_double(right);

	if (IS_NVP(dot) || IS_NVP(m1) || IS_NVP(m2)) PG_RETURN_NULL();

	result = dot / (m1*m1 + m2*m2 - dot);

	if (result > 1.0) {
		result = 1.0;
	}
	else if (result < 0.0) {
		result = 0.0;
	}

	PG_RETURN_FLOAT8(1. - result);
}

PG_FUNCTION_INFO_V1( svec_normalize );
/**
 *  svec_normalize - Computes a normalized SVEC.
 */
Datum svec_normalize(PG_FUNCTION_ARGS)
{
	SvecType *svec = PG_GETARG_SVECTYPE_P(0);
	SparseData sdata = sdata_from_svec(svec);
	double norm;

	norm = l2norm_sdata_values_double(sdata);

	op_sdata_by_scalar_inplace( 3, (char *)&norm, sdata, 2);

	PG_RETURN_SVECTYPE_P( svec_from_sparsedata( sdata, true));
}

PG_FUNCTION_INFO_V1( svec_l1norm );
/**
 *  svec_l1norm - computes the l1 norm of an svec
 */
Datum svec_l1norm(PG_FUNCTION_ARGS)
{
	SvecType *svec = PG_GETARG_SVECTYPE_P(0);
	SparseData sdata  = sdata_from_svec(svec);
	double accum;
	accum = l1norm_sdata_values_double(sdata);

	if (IS_NVP(accum)) PG_RETURN_NULL();

	PG_RETURN_FLOAT8(accum);
}

PG_FUNCTION_INFO_V1( svec_summate );
/**
 *  svec_summate - computes the sum of all the elements in an svec
 */
Datum svec_summate(PG_FUNCTION_ARGS)
{
	SvecType *svec = PG_GETARG_SVECTYPE_P(0);
	SparseData sdata  = sdata_from_svec(svec);
	double accum;
	accum = sum_sdata_values_double(sdata);

	if (IS_NVP(accum)) PG_RETURN_NULL();

	PG_RETURN_FLOAT8(accum);
}

PG_FUNCTION_INFO_V1( svec_log );
/**
 *  svec_log - computes the log of each element in an svec
 */
Datum svec_log(PG_FUNCTION_ARGS)
{
	SvecType *svec = PG_GETARG_SVECTYPE_P_COPY(0);
	double *vals = (double *)SVEC_VALS_PTR(svec);
	int unique_value_count=SVEC_UNIQUE_VALCNT(svec);

	for (int i=0;i<unique_value_count;i++) vals[i] = log(vals[i]);

	PG_RETURN_SVECTYPE_P(svec);
}

/**
 *  float8arr_l1norm - computes the l1 norm of a float8 array
 */
Datum float8arr_l1norm(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1( float8arr_l1norm);
Datum float8arr_l1norm(PG_FUNCTION_ARGS) {
	ArrayType *array  = PG_GETARG_ARRAYTYPE_P(0);
	SparseData sdata = sdata_uncompressed_from_float8arr_internal(array);
	double result = l1norm_sdata_values_double(sdata);
	pfree(sdata);

	if (IS_NVP(result)) PG_RETURN_NULL();

	PG_RETURN_FLOAT8(result);
}

/**
 *  float8arr_summate - sums up all the elements of a float8 array
 */
Datum float8arr_summate(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1( float8arr_summate);
Datum float8arr_summate(PG_FUNCTION_ARGS) {
	ArrayType *array  = PG_GETARG_ARRAYTYPE_P(0);
	SparseData sdata = sdata_uncompressed_from_float8arr_internal(array);
	double result = sum_sdata_values_double(sdata);
	pfree(sdata);

	if (IS_NVP(result)) PG_RETURN_NULL();

	PG_RETURN_FLOAT8(result);
}


/**
 *  float8arr_l2norm - computes the l2 norm of a float8 array
 */
Datum float8arr_l2norm(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1( float8arr_l2norm);
Datum float8arr_l2norm(PG_FUNCTION_ARGS) {
	ArrayType *array  = PG_GETARG_ARRAYTYPE_P(0);
	SparseData sdata = sdata_uncompressed_from_float8arr_internal(array);
	double result = l2norm_sdata_values_double(sdata);
	pfree(sdata);

	if (IS_NVP(result)) PG_RETURN_NULL();

	PG_RETURN_FLOAT8(result);
}

/*
 * Hash function for float8[]
 */
static int
float8arr_hash_internal(ArrayType *array)
{
	SparseData sdata = sdata_uncompressed_from_float8arr_internal(array);
	double l1norm = l1norm_sdata_values_double(sdata);
	int arr_hash = DirectFunctionCall1(hashfloat8, Float8GetDatumFast(l1norm));
	pfree(sdata);
	return(arr_hash);
}

Datum float8arr_hash(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1( float8arr_hash);

Datum
float8arr_hash(PG_FUNCTION_ARGS) {
	ArrayType *array  = PG_GETARG_ARRAYTYPE_P(0);

	PG_RETURN_INT32(float8arr_hash_internal(array));
}

PG_FUNCTION_INFO_V1( svec_pivot );
/**
 * Aggregate function svec_pivot takes its float8 argument and appends it
 * to the state variable (an svec) to produce the concatenated return variable.
 * The StringInfo variables within the state variable svec are used in a way
 * that minimizes the number of memory re-allocations.
 *
 * Note that the first time this is called, the state variable should be null.
 */
Datum svec_pivot(PG_FUNCTION_ARGS)
{
	SvecType *svec;
	SparseData sdata;
	float8 value;

	if (PG_ARGISNULL(1)) value = NVP;
	else value = PG_GETARG_FLOAT8(1);

	if (! PG_ARGISNULL(0))
	{
		svec = PG_GETARG_SVECTYPE_P_COPY(0);
	} else {	//first call, construct a new svec
		/*
		 * Allocate space for the unique values and index
		 *
		 * Note that we do this manually because we are going to
		 * manage the memory allocations for the StringInfo structures
		 * manually within this aggregate so that we can preserve
		 * the intermediate state without re-serializing until there is
		 * a need to re-alloc, at which point we will re-serialize to
		 * form the returned state variable.
		 */
		svec = makeEmptySvec(1);
	}
	sdata = sdata_from_svec(svec);

	/*
	 * Add the incoming float8 value to the svec.
	 *
	 * First check to see if there is room in both the data area and index
	 * and if there isn't, re-alloc and recreate the svec
	 */
	if (   ((Size) (sdata->vals->len + sizeof(float8)+1) > (Size) sdata->vals->maxlen)
	    || ((Size) (sdata->index->len + 9 +1)            > (Size) sdata->index->maxlen) )
	{
		svec = reallocSvec(svec);
		sdata = sdata_from_svec(svec);
	}

	/*
	 * Now let's check to see if we're adding a new value or appending to
	 * the last run.  If the incoming value is the same as the last value,
	 * just increment the last run.  Note that we need to use the index
	 * cursor to find where the last index counter is located.
	 */
	{
		char *index_location;
		int old_index_storage_size;
		int64 run_count;
		float8 last_value=-100000;
		bool new_run;

		if (sdata->index->len==0) //New vector
		{
			new_run=true;
			index_location = sdata->index->data;
			sdata->index->cursor = 0;
			run_count = 0;
		} else
		{
			// initialise index cursor if we need to
			if (sdata->index->cursor == 0) {
				char *i_ptr=sdata->index->data;
				int len=0;
				for (int j=0;j<sdata->unique_value_count-1;j++)
				{
					len+=int8compstoragesize(i_ptr);
					i_ptr+=int8compstoragesize(i_ptr);
				}
				sdata->index->cursor = len;
			}

			index_location = sdata->index->data + sdata->index->cursor;
			old_index_storage_size = int8compstoragesize(index_location);
			run_count = compword_to_int8(index_location);
			last_value = *((float8 *)(sdata->vals->data+(sdata->vals->len-sizeof(float8))));

			if (last_value == value ||
			    (IS_NVP(last_value) && IS_NVP(value)))
				new_run = false;
			else new_run = true;
		}
		if (!new_run)
		{
			run_count++;
			int8_to_compword(run_count,index_location);
			sdata->index->len += (int8compstoragesize(index_location)
					- old_index_storage_size);
			sdata->total_value_count++;
		} else {
			add_run_to_sdata((char *)(&value),1,sizeof(float8),sdata);
			char *i_ptr=sdata->index->data;
			int len=0;
			for (int j=0;j<sdata->unique_value_count-1;j++)
			{
				len+=int8compstoragesize(i_ptr);
				i_ptr+=int8compstoragesize(i_ptr);
			}
			sdata->index->cursor = len;
		}
	}

	PG_RETURN_SVECTYPE_P(svec);
}

#define RANDOM_RANGE	drand48()
#define RANDOM_INT(x,y)	((int)(x)+(int)(((y+1)-(x))*RANDOM_RANGE))
#define SWAPVAL(x,y,temp)	{ (temp) = (x); (x) = (y); (y) = (temp); }
#define SWAP(x,y,tmp,size)	{ memcpy((tmp),(x),(size)); memcpy((x),(y),(size)); memcpy((y),(tmp),(size)); }
#define SWAPN(lists,nlists,widths,tmp,I,J) \
{ \
	for (uint64 III=0;III<nlists;III++) /* This should be unrolled as nlists will be small */ \
	{ \
		memcpy((tmp)[III]                  ,(lists)[III]+I*(widths)[III],(widths)[III]); \
		memcpy((lists)[III]+I*(widths)[III],(lists)[III]+J*(widths)[III],(widths)[III]); \
		memcpy((lists)[III]+J*(widths)[III],(tmp)[III]                  ,(widths)[III]); \
	} \
}
/*
 * Implements the partition selection algorithm with randomized selection
 *
 * From: http://en.wikipedia.org/wiki/Selection_algorithm#Linear_general_selection_algorithm_-_.22Median_of_Medians_algorithm.22
 *
 * Arguments:
 * 	char **lists:	A list of lists, the first of which contains the
 *                      values used for pivots, the 2nd and further lists
 *                      will be pivoted alongside the first.
 * 			A common usage would be to have the first list point
 *                      to an array of values, then the second would point to
 *                      another char ** list of strings. The second list would
 *                      have its pointer values moved around as part of the
 *                      pivots, and the index location where the partition
 *                      value (say for the median) occurs would allow a
 *                      reference to the associated strings in the second list.
 *
 * 	size_t nlists	the number of lists
 *
 * 	size_t *widths	An array of widths, one for each list
 *
 * 	int left,right	The left and right boundary of the list to be pivoted
 *
 * 	int pivotIndex	The index around which to pivot the list.  A common
 *                      use-case is to choose pivotIndex = listLength/2, then
 *                      the pivot will provide the median location.
 *
 * 	int (*compar)	A comparison function for the first list, which takes
 *                      two pointers to values in the first list and returns
 *                      0,-1 or 1 when the first value is equal, less than or
 *                      greater than the second.
 *
 * 	char **tmp 	A list of temporary variables, allocated with the size
 *                      of the value in each list.
 *
 * 	void *pvalue	Pointers to temporary variable allocated with the
 *                      width of the values of the first list.
 */
static int
partition_pivot(char **lists, size_t nlists, size_t *widths,
		int left, int right, int pivotIndex,
		int (*compar)(const void *, const void *),
		char **tmp, void *pvalue)
{
	int storeIndex = left;

	memcpy(pvalue,lists[0]+pivotIndex*widths[0],widths[0]);

	SWAPN(lists,nlists,widths,tmp,pivotIndex,right) // Move pivot to end
	for (int i=left;i<right;i++)
	{
		if (compar(lists[0]+i*widths[0],pvalue) <= 0)
		{
			SWAPN(lists,nlists,widths,tmp,i,storeIndex)
			storeIndex++;
		}
	}
	SWAPN(lists,nlists,widths,tmp,storeIndex,right) // Move pivot to its final place
	return(storeIndex);
}

/*
 * The call interface to partition_select has one complicated looking feature:
 * you must pass in a "Real Index Calculation" function that will return an
 * integer corresponding to the actual partition index. This is used to
 * enable the same routine to work with dense and compressed structures.
 * This function can just return the input integer unmodified if using a
 * dense array of values as input.
 * The arguments to realIndexCalc() should be:
 * 	int: 		the pivot index (returned from the pivot function)
 * 	char **:	the list of lists
 * 	size_t:		the number of lists
 * 	size_t *:	the width of each value in the list
 */
static int
partition_select (char **lists, size_t nlists, size_t *widths,
		int left, int right, int k,
		int (*compar)(const void *, const void *),
		int (*realIndexCalc)(const int, const char **, const size_t, const size_t *))
{
	int pivotIndex,pivotNewIndex,realIndex;
	char **tmp,*pvalue;
	int maxlen = right;

	/*
	 * Allocate memory for the temporary variables
	 */
	tmp = (char **)palloc(nlists*sizeof(char *));
	for (uint64 i=0;i<nlists;i++)
	{
		tmp[i] = (void *)palloc(widths[i]);
	}
	pvalue = (char *)palloc(widths[0]);

	while (1)
	{
		pivotIndex = RANDOM_INT(left,right);
		pivotNewIndex = partition_pivot(lists,nlists,widths,
					left,right,pivotIndex,
					compar,
					tmp,pvalue);
		realIndex = realIndexCalc(pivotNewIndex,
				(const char **)lists,nlists,widths);
		int nextRealIndex = realIndexCalc(Min(maxlen,pivotNewIndex+1),
	                                (const char **)lists,nlists,widths);

		if ((realIndex <= k) && (k < nextRealIndex ))
		{
			break;
		} else if (k < realIndex)
		{
			right = pivotNewIndex-1;
		} else
		{
			left = pivotNewIndex+1;
			if (left >= maxlen)
			{
				pivotNewIndex = maxlen;
				break;
			}
		}
	}
	/*
	 * Free temporary variables
	 */
	for (uint64 i=0;i<nlists;i++)
		pfree(tmp[i]);
	pfree(tmp);
	pfree(pvalue);

	return(pivotNewIndex); //This index is in the compressed coordinate system
}

static int
compar_float8(const void *left,const void *right)
{
	if (*(float8 *)left<*(float8 *)right) { return -1; }
	else if(*(float8 *)left==*(float8 *)right) { return 0; }
	else { return 1; }
}

static int
real_index_calc_dense(const int idx,const char **lists,const size_t nlists,const size_t *widths)
{
	(void) lists; /* avoid warning about unused parameter */
	(void) nlists; /* avoid warning about unused parameter */
	(void) widths; /* avoid warning about unused parameter */
	return idx;
}

static int
real_index_calc_sparse_RLE(const int idx,const char **lists,const size_t nlists,const size_t *widths)
{
	(void) nlists; /* avoid warning about unused parameter */
	(void) widths; /* avoid warning about unused parameter */
	int index=0;
	for (int i=0;i<idx;i++)
	{
		index = index + ((int64 *)(lists[1]))[i];
	}
	/*
	 * The index calculation corresponds to the beginning
	 * of the run located at (idx).
	 */
	return (index);
}

static int
float8arr_partition_internal(float8 *array,int len,int k)
{
	size_t width=sizeof(float8);
	char *list = (char *)array;
	int index = partition_select(&list,1,&width,
				0,len-1,
				k,compar_float8,
				real_index_calc_dense);
	return (index);
}

/**
 * Computes the median of an array of float8s
 */
Datum float8arr_median(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1( float8arr_median);

Datum
float8arr_median(PG_FUNCTION_ARGS) {
	ArrayType *array  = PG_GETARG_ARRAYTYPE_P_COPY(0);
	SparseData sdata = sdata_uncompressed_from_float8arr_internal(array);
	int index,median_index = (sdata->total_value_count-1)/2;
	float8 ret;

	double * vals = (double *)(sdata->vals->data);
	for (int i=0; i<sdata->unique_value_count; i++)
		if (IS_NVP(vals[i]))
			PG_RETURN_NULL();

	index = float8arr_partition_internal((double *)(sdata->vals->data),
					     sdata->total_value_count,
					     median_index);

	ret = ((float8 *)(sdata->vals->data))[index];
	if (IS_NVP(ret)) PG_RETURN_NULL();
	PG_RETURN_FLOAT8(ret);
}

/**
 * Computes the median of a sparse vector
 */
Datum svec_median(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1( svec_median);

Datum
svec_median(PG_FUNCTION_ARGS) {
	SvecType *svec  = PG_GETARG_SVECTYPE_P_COPY(0);
	SparseData sdata = sdata_from_svec(svec);
	int index,median_index = (sdata->total_value_count-1)/2;
	char *i_ptr;
	int64 *rle_index;
	float8 ret;

	float8 * vals = (float8 *)sdata->vals->data;
	for (int i=0; i<sdata->unique_value_count; i++)
		if (IS_NVP(vals[i]))
			PG_RETURN_NULL();

	if (sdata->index->data != NULL) //Sparse vector
	{
		/*
		 * We need to create an uncompressed run length index to
		 * feed to the partition select routine
		 */
		rle_index = (int64 *)palloc(sizeof(int64)*(sdata->unique_value_count));
		i_ptr = sdata->index->data;
		for (int i=0;i<sdata->unique_value_count;i++,i_ptr+=int8compstoragesize(i_ptr))
		{
			rle_index[i] = compword_to_int8(i_ptr);
		}
		/*
		 * Allocate the outer "list of lists"
		 */
		char **lists = (char **)palloc(sizeof(char *)*2);
		lists[0] = sdata->vals->data;
		lists[1] = (char *)rle_index;
		size_t *widths = (size_t *)palloc(sizeof(size_t)*2);
		widths[0] = sizeof(float8);
		widths[1] = sizeof(int64);
		index = partition_select(lists,2,widths,
					0,sdata->unique_value_count-1,
					median_index,compar_float8,
					real_index_calc_sparse_RLE);
		/*
		 * Convert the uncompressed index into the compressed index
		 */
		i_ptr = sdata->index->data;
		for (int i=0;i<sdata->unique_value_count;i++,i_ptr+=int8compstoragesize(i_ptr))
		{
			int8_to_compword(rle_index[i],i_ptr);
		}

		pfree(lists);
		pfree(widths);
		pfree(rle_index);
	} else
	{
		index = float8arr_partition_internal((double *)(sdata->vals->data),
				sdata->total_value_count,
				median_index);
	}

	ret = ((float8 *)(sdata->vals->data))[index];

	if (IS_NVP(ret)) PG_RETURN_NULL();
	PG_RETURN_FLOAT8(ret);
}

Datum svec_nonbase_positions(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1( svec_nonbase_positions);

Datum
svec_nonbase_positions(PG_FUNCTION_ARGS) {
	SvecType *svec  = PG_GETARG_SVECTYPE_P_COPY(0);
	float8 base_val = PG_GETARG_FLOAT8(1);
	int64 *result = NULL;
	int result_size = 0;
	int64 size_tracker = 0;
	SparseData sdata = sdata_from_svec(svec);
	char *i_ptr;
	int fill_count = 0;
	int64 *rle_index;
	ArrayType *pgarray;

	float8 * vals = (float8 *)sdata->vals->data;

	if (sdata->index->data != NULL) //Sparse vector
	{
		/*
		 * We need to create an uncompressed run length index to
		 * feed to the partition select routine
		 */
		rle_index = (int64 *)palloc(sizeof(int64)*(sdata->unique_value_count));
		i_ptr = sdata->index->data;
		for (int i=0;i<sdata->unique_value_count;i++,i_ptr+=int8compstoragesize(i_ptr))
		{
			rle_index[i] = compword_to_int8(i_ptr);
			if(memcmp(&base_val,&vals[i],sizeof(float8))){
				result_size+=rle_index[i];
			}
		}
		/*
			now we have array of values and array of runs (vals,rle_index and size sdata->unique_value_count), need to find positions that
			are not base value
		 */
		result = (int64 *)palloc(sizeof(int64)*(result_size));
		for (int i=0;i<sdata->unique_value_count;i++,i_ptr+=int8compstoragesize(i_ptr))
		{
			if(memcmp(&base_val,&vals[i],sizeof(float8))){
				for(int j = 0; j < rle_index[i]; ++j){
					result[fill_count] = size_tracker+1;
					fill_count++;
					size_tracker++;
				}
			}else{
				size_tracker += rle_index[i];
			}
		}

		pfree(rle_index);
	}
	pgarray = construct_array((Datum *)result,
							  result_size, INT8OID,
							  sizeof(int64),true,'d');
    PG_RETURN_ARRAYTYPE_P(pgarray);
}

Datum svec_nonbase_values(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(svec_nonbase_values);

Datum
svec_nonbase_values(PG_FUNCTION_ARGS) {
	SvecType *svec  = PG_GETARG_SVECTYPE_P_COPY(0);
	float8 base_val = PG_GETARG_FLOAT8(1);
	float8 *result = NULL;
	int result_size = 0;
	SparseData sdata = sdata_from_svec(svec);
	char *i_ptr;
	int fill_count = 0;
	int64 *rle_index;
	ArrayType *pgarray;

	float8 * vals = (float8 *)sdata->vals->data;

	if (sdata->index->data != NULL) //Sparse vector
	{
		/*
		 * We need to create an uncompressed run length index to
		 * feed to the partition select routine
		 */
		rle_index = (int64 *)palloc(sizeof(int64)*(sdata->unique_value_count));
		i_ptr = sdata->index->data;
		for (int i=0;i<sdata->unique_value_count;i++,i_ptr+=int8compstoragesize(i_ptr))
		{
			rle_index[i] = compword_to_int8(i_ptr);
			if(memcmp(&base_val,&vals[i],sizeof(float8))){
				result_size+=rle_index[i];
			}
		}
		/*
		 now we have array of values and array of runs (vals,rle_index and size sdata->unique_value_count), need to find positions that
		 are not base value
		 */
		result = (float8 *)palloc(sizeof(int64)*(result_size));
		for (int i=0;i<sdata->unique_value_count;i++,i_ptr+=int8compstoragesize(i_ptr))
		{
			if(memcmp(&base_val,&vals[i],sizeof(float8))){
				for(int j = 0; j < rle_index[i]; ++j){
					result[fill_count] = vals[i];
					fill_count++;
				}
			}
		}

		pfree(rle_index);
	}
	pgarray = construct_array((Datum *)result,
							  result_size, FLOAT8OID,
							  sizeof(float8),true,'d');
    PG_RETURN_ARRAYTYPE_P(pgarray);
}

PG_FUNCTION_INFO_V1(svec_hash);
/**
 *  svec_hash - computes a hash value of svec
 */
Datum svec_hash( PG_FUNCTION_ARGS)
{
	SvecType *svec1 = PG_GETARG_SVECTYPE_P(0);
	SparseData sdata  = sdata_from_svec(svec1);
	char *ix = sdata->index->data;
	double *vals = (double *)sdata->vals->data;

	unsigned long hash = 65599;
    unsigned short c;

    for (int i=0;i<sdata->unique_value_count;i++)
	{
		c = compword_to_int8(ix);
		hash = c + (hash << 7) + (hash << 16) - hash;
		c = vals[i];
		hash = c + (hash << 7) + (hash << 16) - hash;

		ix+=int8compstoragesize(ix);
	}
	PG_RETURN_INT32(hash);
}

/**
 *  svec_mean_transition (float8arr, svec):
 *
 *		Accumulates svec's by adding them elementwise and incrementing
 *      the last element of the state array.
 *
 */
PG_FUNCTION_INFO_V1( svec_mean_transition );
Datum svec_mean_transition(PG_FUNCTION_ARGS)
{
	/* Validate input*/
	if (PG_ARGISNULL(0) && PG_ARGISNULL(1))
		PG_RETURN_NULL();

	if (PG_ARGISNULL(1))
		PG_RETURN_ARRAYTYPE_P(PG_GETARG_ARRAYTYPE_P(0));

	/* Get ARG(1) and convert it into float8 array */
	SvecType *svec = PG_GETARG_SVECTYPE_P(1);
	SparseData sdata = sdata_from_svec( svec);
	int svec_dim = sdata->total_value_count;
	float8 *svec_array = sdata_to_float8arr( sdata);

	if (PG_ARGISNULL(0)) {
		/*
		 * This is the first call, so create new state array
		 */
		float8 *state_array;
		state_array = (float8 *) palloc((svec_dim+1) * sizeof(float8));
		for (int i=0; i<svec_dim; i++) {
			state_array[i] = svec_array[i];
		}
		state_array[svec_dim] = 1;
		ArrayType *out_array = construct_array((Datum *)state_array,
						svec_dim+1, FLOAT8OID,
						sizeof(float8),true,'d');

		PG_RETURN_ARRAYTYPE_P(out_array);
	}

	/* Read the state */
	ArrayType *transarray;
	if (fcinfo->context && IsA(fcinfo->context, AggState))
		transarray = PG_GETARG_ARRAYTYPE_P(0);
	else
		transarray = PG_GETARG_ARRAYTYPE_P_COPY(0);

	/* Extract state array */
	int state_dim = ARR_DIMS(transarray)[0];
	float8 *state_array = (float8 *) ARR_DATA_PTR(transarray);

	/* Check dimensions on both inputs */
	if ( state_dim != (svec_dim+1))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("%s: input dimensions should be dim1=dim2+1, but are: dim1=%d, dim2=%d\n",
						"svec_mean_transition", state_dim, svec_dim)));

	/* Transition */
	for (int i=0; i<svec_dim; i++) {
		state_array[i] = state_array[i] + svec_array[i];
	}
	state_array[svec_dim]++;

	PG_RETURN_ARRAYTYPE_P(transarray);

}

/**
 *  svec_mean_prefunc (float8arr, float8arr):
 *
 *		Preliminary merge function
 *
 */
PG_FUNCTION_INFO_V1( svec_mean_prefunc );
Datum svec_mean_prefunc( PG_FUNCTION_ARGS)
{
	/* Validate input*/
	if (PG_ARGISNULL(0) && PG_ARGISNULL(1))
		PG_RETURN_NULL();

	if (PG_ARGISNULL(0))
		PG_RETURN_ARRAYTYPE_P(PG_GETARG_ARRAYTYPE_P(1));

	if (PG_ARGISNULL(1))
		PG_RETURN_ARRAYTYPE_P(PG_GETARG_ARRAYTYPE_P(0));

	/* Read the arguments */
	ArrayType *transarray1, *transarray2;
	if (fcinfo->context && IsA(fcinfo->context, AggState)) {
		transarray1 = PG_GETARG_ARRAYTYPE_P(0);
		transarray2 = PG_GETARG_ARRAYTYPE_P(1);
	} else {
		transarray1 = PG_GETARG_ARRAYTYPE_P_COPY(0);
		transarray2 = PG_GETARG_ARRAYTYPE_P_COPY(1);
	}

	/* Define the arrays */
	float8 *array1 = (float8 *)ARR_DATA_PTR(transarray1);
	int dim1 = ARR_DIMS(transarray1)[0];
	float8 *array2 = (float8 *)ARR_DATA_PTR(transarray2);
	int dim2 = ARR_DIMS(transarray2)[0];

	/* Check dimensions */
	if ( dim1 != dim2)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("%s: input dimensions should be the same, but are: dim1=%d, dim2=%d\n",
						"svec_mean_prefunc", dim1, dim2)));

	/* Add */
	for (int i=0; i<dim1; i++) {
		array1[i] = array1[i] + array2[i];
	}

	PG_RETURN_ARRAYTYPE_P(transarray1);

}

/**
 *  svec_mean_final (float8arr):
 *
 *		Divides all elements of the array by its last element
 *		and returns n-1 element SVEC
 *
 */
PG_FUNCTION_INFO_V1( svec_mean_final );
Datum svec_mean_final( PG_FUNCTION_ARGS)
{
	/* Validate state input*/
	if PG_ARGISNULL(0) {
		PG_RETURN_NULL();
	}

	SvecType *svec;
	ArrayType *transarray;

	/* Read the argument */
	if (fcinfo->context && IsA(fcinfo->context, AggState))
		transarray = PG_GETARG_ARRAYTYPE_P(0);
	else
		transarray = PG_GETARG_ARRAYTYPE_P_COPY(0);

	/* Define the array */
	float8 *array = (float8 *)ARR_DATA_PTR(transarray);
	int dim = ARR_DIMS(transarray)[0];

	/* Divide */
	for (int i=0; i<dim-1; i++) {
		array[i] = array[i] / array[dim-1];
	}

	/* Create the output SVEC */
	SparseData sdata = float8arr_to_sdata(array,dim-1);
	svec = svec_from_sparsedata(sdata,true);

	PG_RETURN_SVECTYPE_P(svec);

}
