/**
 * @file
 * @brief Support functions for the online SVM routines
 * @author Kee Siong Ng
 */

#include "postgres.h"
#include "funcapi.h"
#include "fmgr.h"
#include "catalog/pg_type.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "executor/executor.h" /* for GetAttributeByName() */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

/*
 * This function computes the inner product of two points.
 * The spvs array is actually an array of data points stored one after another.
 * The first point of the inner product is the idx-th data point in spvs.
 */
static float8 kernel_dot(float8 * spvs, int32 idx, int32 ind_dim, float8 * ind)
{
	float8 ret = 0;
	int i;
	for (i=0; i!=ind_dim; i++)
		ret += spvs[ind_dim * idx + i] * ind[i];
	return ret;
}

/*
 * This function needs to be generalised to use arbitrary given kernel function
 */
static float8 svm_predict_eval(float8 * weights, float8 * support_vectors,
			       float8 * ind, int32 nsvs, int32 ind_dim)
{
	int i; float8 ret = 0;
	for (i=0; i!=nsvs; i++)
		ret += weights[i] * kernel_dot(support_vectors,i,ind_dim,ind);
	return ret;
}

Datum svm_predict_sub(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(svm_predict_sub);

/**
 * This function evaluates a support vector model on an individual data point.
 */
Datum svm_predict_sub(PG_FUNCTION_ARGS)
{
	float8 ret = 0;
	int32 nsvs = PG_GETARG_INT32(0);
	int32 ind_dim = PG_GETARG_INT32(1);
	ArrayType * weights_arr = PG_GETARG_ARRAYTYPE_P(2);
	ArrayType * support_vectors_arr = PG_GETARG_ARRAYTYPE_P(3);
	ArrayType * ind_arr = PG_GETARG_ARRAYTYPE_P(4);

	float8 * weights, * support_vectors, * ind;

	// input error checking 
	if (ARR_NULLBITMAP(weights_arr) || ARR_NDIM(weights_arr) != 1 ||
	    ARR_ELEMTYPE(weights_arr) != FLOAT8OID ||
	    ARR_NULLBITMAP(support_vectors_arr) || 
	    ARR_NDIM(support_vectors_arr) != 1 ||
	    ARR_ELEMTYPE(support_vectors_arr) != FLOAT8OID ||
	    ARR_NULLBITMAP(ind_arr) || ARR_NDIM(ind_arr) != 1 ||
	    ARR_ELEMTYPE(ind_arr) != FLOAT8OID)
		ereport(ERROR,
		       (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			errmsg("function \"%s\" called with invalid parameters",
			       format_procedure(fcinfo->flinfo->fn_oid))));

	weights = (float8 *)ARR_DATA_PTR(weights_arr);
	support_vectors = (float8 *)ARR_DATA_PTR(support_vectors_arr);
	ind = (float8 *)ARR_DATA_PTR(ind_arr);

	ret = svm_predict_eval(weights, support_vectors, ind, nsvs, ind_dim);
	
	PG_RETURN_FLOAT8(ret);
}

/*
static ArrayType * zero_array(int len)
{
	float8 * array = palloc0(len * sizeof(float8));
	ArrayType * ret_arr = construct_array((Datum *)array, len, FLOAT8OID,
					      sizeof(float8), true, 'd');
	return ret_arr;
}
*/

static ArrayType * makeNewWeights(float8 * weights, float8 weight, int nsvs) 
{
	ArrayType * ret_arr;
	float8 * ret = palloc0((nsvs+1) * sizeof(float8));
	int i;
	// for (i=0; i!=nsvs; i++) ret[i] = weights[i];
	memcpy(ret, weights, sizeof(float8) * nsvs);
	ret[nsvs] = weight;
	
	ret_arr = construct_array((Datum *)ret, nsvs+1, FLOAT8OID,
				  sizeof(float8), true, 'd');
	pfree(ret);
	return ret_arr;
}


static ArrayType * makeNewSPVS(float8 * spvs, float8 * ind, int nsvs, int dim) 
{
	ArrayType * ret_arr;
	float8 * ret = palloc0((nsvs+1) * dim * sizeof(float8));
	int i;
	memcpy(ret, spvs, sizeof(float8) * nsvs * dim);
	// for (i=0; i!=nsvs*dim; i++) ret[i] = spvs[i];
	for (i=0; i!=dim; i++) ret[nsvs*dim + i] = ind[i];
	
	ret_arr = construct_array((Datum *)ret, (nsvs+1) * dim, FLOAT8OID,
				  sizeof(float8), true, 'd');
	pfree(ret);

	ret = (float8 *)ARR_DATA_PTR(ret_arr);
	return ret_arr;
}

Datum svm_reg_update_sub(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(svm_reg_update_sub);

Datum svm_reg_update_sub(PG_FUNCTION_ARGS)
{
	float8 eta = 0.05;    // learning rate
	float8 slambda = 0.2; // regularisation parameter
	float8 nu = 0.001;    // compression parameter in [0,1]
	float8 p;             // prediction for data point
	float8 diff;
	float8 error;
	float8 weight;

	int i;
	float8 * temp;

	HeapTupleHeader t = PG_GETARG_HEAPTUPLEHEADER(0);
	ArrayType * ind_arr = PG_GETARG_ARRAYTYPE_P(1);
	float8 label = PG_GETARG_FLOAT8(2);

	float8 * ind = (float8 *)ARR_DATA_PTR(ind_arr);

	TupleDesc tuple;
	HeapTuple ret;

	Datum * values = palloc(9 * sizeof(Datum));
	bool isnull;
	bool * isnulls;

	int32 inds;
	float8 cum_err, epsilon;
	float8 rho, b;
	int32 nsvs, ind_dim;
	ArrayType * weights_arr;
	ArrayType * support_vectors_arr;
	ArrayType * new_weights_arr = NULL;
	ArrayType * new_support_vectors_arr = NULL;
	float8 * weights, * support_vectors;

	int ndims;
	int * dims;

	inds = DatumGetInt32(GetAttributeByName(t, "inds", &isnull));
	if (isnull) elog(ERROR, "inds");
	cum_err = DatumGetFloat8(GetAttributeByName(t, "cum_err", &isnull));
	if (isnull) elog(ERROR, "cum_err");
	epsilon = DatumGetFloat8(GetAttributeByName(t, "epsilon", &isnull));
	if (isnull) elog(ERROR, "epsilon");
	rho = DatumGetFloat8(GetAttributeByName(t, "rho", &isnull));
	if (isnull) elog(ERROR, "rho");
	b = DatumGetFloat8(GetAttributeByName(t, "b", &isnull));
	if (isnull) elog(ERROR, "b");
	nsvs = DatumGetInt32(GetAttributeByName(t, "nsvs", &isnull));
	if (isnull) elog(ERROR, "nsvs");
	ind_dim = DatumGetInt32(GetAttributeByName(t, "ind_dim", &isnull));
	if (isnull) elog(ERROR, "ind_dim");
	weights_arr = DatumGetArrayTypeP(GetAttributeByName(t, "weights", &isnull));
	if (isnull) elog(ERROR, "weights");
	support_vectors_arr = DatumGetArrayTypeP(GetAttributeByName(t, "individuals", &isnull));
	if (isnull) elog(ERROR, "individuals");

	weights = (float8 *)ARR_DATA_PTR(weights_arr);
	support_vectors = (float8 *)ARR_DATA_PTR(support_vectors_arr);

	if (ind_dim == 0) {
		ndims = ARR_NDIM(ind_arr);
		dims = ARR_DIMS(ind_arr);
		ind_dim = ArrayGetNItems(ndims, dims);
	}

	p = svm_predict_eval(weights, support_vectors, ind, nsvs, ind_dim); 
	diff = label - p;
	error = fabs(diff);
			     
	inds++;
	cum_err = cum_err + error;

	if (error > epsilon) {
		for (i=0; i!=nsvs; i++) 
			weights[i] = weights[i] * (1 - eta * slambda);

		weight = diff < 0 ? -eta : eta;
		new_weights_arr = makeNewWeights(weights,weight,nsvs);
	        new_support_vectors_arr = makeNewSPVS(support_vectors,ind,nsvs,ind_dim);
		nsvs++;
		epsilon = epsilon + (1 - nu) * eta;
	} else {
		epsilon = epsilon - eta * nu;
	}

	values[0] = Int32GetDatum(inds);
	values[1] = Float8GetDatum(cum_err);
	values[2] = Float8GetDatum(epsilon);
	values[3] = Float8GetDatum(rho);
	values[4] = Float8GetDatum(b);
	values[5] = Int32GetDatum(nsvs);
	values[6] = Int32GetDatum(ind_dim);
	if (new_weights_arr == NULL) {
		values[7] = PointerGetDatum(weights_arr);
		values[8] = PointerGetDatum(support_vectors_arr);
	} else {
		values[7] = PointerGetDatum(new_weights_arr);
		values[8] = PointerGetDatum(new_support_vectors_arr);
	}

	if (get_call_result_type(fcinfo, NULL, &tuple) != TYPEFUNC_COMPOSITE)
		ereport(ERROR,
			(errcode( ERRCODE_FEATURE_NOT_SUPPORTED ),
			 errmsg( "function returning record called in context "
				 "that cannot accept type record" )));
	tuple = BlessTupleDesc(tuple);

	isnulls = palloc0(9 * sizeof(bool));
	ret = heap_form_tuple(tuple, values, isnulls);

	/*
	for (i=0; i!=9; i++)
		if (isnulls[i])
			ereport(ERROR,
		       (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			errmsg("function \"%s\" called with invalid parameters %d",
			       format_procedure(fcinfo->flinfo->fn_oid),i)));

	*/
	PG_RETURN_DATUM(HeapTupleGetDatum(ret));
}


