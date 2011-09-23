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
#include "parser/parse_func.h"
#include "utils/lsyscache.h"
#include "executor/executor.h" /* for GetAttributeByName() */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>

#ifndef NO_PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

#define FLOAT8ARRAYOID 1022

/*
 * This function constructs an array of zeros.
 */
static ArrayType *construct_zero_array(int inNumElements,
	Oid inElementType, int inElementSize)
{
	int64 size = inElementSize * inNumElements + ARR_OVERHEAD_NONULLS(1);
	ArrayType * array;
	
	array = (ArrayType *) palloc0(size);
	SET_VARSIZE(array, size);
	array->ndim = 1;
	array->dataoffset = 0;
	array->elemtype = inElementType;
	ARR_DIMS(array)[0] = inNumElements;
	ARR_LBOUND(array)[0] = 1;
	return array;
}

/*
 * This function applies the kernel function on its two arguments.
 * This function incurs a large overhead and is up to ten times slower
 * than a kernel function implemented in C and called directly here.
 */
static float8 apply_kernel(Oid foid, ArrayType * x1, ArrayType * x2) 
{
	float8 ret = 
	  DatumGetFloat8(
		OidFunctionCall2(foid,PointerGetDatum(x1),PointerGetDatum(x2)));
	return ret;
}

/*
 * This function computes the inner product of two points.
 */
/*
static float8 apply_kernel(Oid foid, ArrayType * x1, ArrayType * x2) 
{
	float8 * arr1 = (float8 *)ARR_DATA_PTR(x1);
	float8 * arr2 = (float8 *)ARR_DATA_PTR(x2);
	float ret = 0;
	for (int i=0; i!=ARR_DIMS(x1)[0]; i++)
		ret += arr1[i] * arr2[i];
	return ret;
}
*/

Datum svm_dot(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(svm_dot);

/*
 * This function computes the inner product of two points.
 */
Datum svm_dot(PG_FUNCTION_ARGS)
{
	ArrayType * arg1 = PG_GETARG_ARRAYTYPE_P(0);
	ArrayType * arg2 = PG_GETARG_ARRAYTYPE_P(1);

	if (ARR_NDIM(arg1) != 1 || ARR_ELEMTYPE(arg1) != FLOAT8OID ||
	    ARR_NDIM(arg2) != 1 || ARR_ELEMTYPE(arg2) != FLOAT8OID)
		ereport(ERROR,
		       (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			errmsg("function \"%s\" called with invalid parameters",
			       format_procedure(fcinfo->flinfo->fn_oid))));

	int dim1 = ARR_DIMS(arg1)[0];
	int dim2 = ARR_DIMS(arg2)[0];

	if (dim1 != dim2)
		ereport(ERROR,
		       (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			errmsg("function \"%s\" called with invalid parameters",
			       format_procedure(fcinfo->flinfo->fn_oid))));
	
	float8 * x1 = (float8 *)ARR_DATA_PTR(arg1);
	float8 * x2 = (float8 *)ARR_DATA_PTR(arg2);

	float8 ret = 0; 
	for (int i=0; i!=dim1; i++)
		ret += x1[i] * x2[i];
	
	PG_RETURN_FLOAT8(ret);
}

Datum svm_polynomial(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(svm_polynomial);

/*
 * This function is the polynomial kernel
 */
Datum svm_polynomial(PG_FUNCTION_ARGS)
{
	ArrayType * arg1 = PG_GETARG_ARRAYTYPE_P(0);
	ArrayType * arg2 = PG_GETARG_ARRAYTYPE_P(1);
	float8 degree = PG_GETARG_FLOAT8(2);

	if (ARR_NDIM(arg1) != 1 || ARR_ELEMTYPE(arg1) != FLOAT8OID ||
	    ARR_NDIM(arg2) != 1 || ARR_ELEMTYPE(arg2) != FLOAT8OID)
		ereport(ERROR,
		       (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			errmsg("function \"%s\" called with invalid parameters",
			       format_procedure(fcinfo->flinfo->fn_oid))));

	int dim1 = ARR_DIMS(arg1)[0];
	int dim2 = ARR_DIMS(arg2)[0];

	if (dim1 != dim2)
		ereport(ERROR,
		       (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			errmsg("function \"%s\" called with invalid parameters",
			       format_procedure(fcinfo->flinfo->fn_oid))));
	
	float8 * x1 = (float8 *)ARR_DATA_PTR(arg1);
	float8 * x2 = (float8 *)ARR_DATA_PTR(arg2);

	float8 ret = 0; 
	for (int i=0; i!=dim1; i++)
		ret += x1[i] * x2[i];
	
	ret = pow(ret,degree);

	PG_RETURN_FLOAT8(ret);
}

Datum svm_gaussian(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(svm_gaussian);

/*
 * This function is the polynomial kernel
 */
Datum svm_gaussian(PG_FUNCTION_ARGS)
{
	ArrayType * arg1 = PG_GETARG_ARRAYTYPE_P(0);
	ArrayType * arg2 = PG_GETARG_ARRAYTYPE_P(1);
	float8 gamma = PG_GETARG_FLOAT8(2);

	if (ARR_NDIM(arg1) != 1 || ARR_ELEMTYPE(arg1) != FLOAT8OID ||
	    ARR_NDIM(arg2) != 1 || ARR_ELEMTYPE(arg2) != FLOAT8OID)
		ereport(ERROR,
		       (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			errmsg("function \"%s\" called with invalid parameters",
			       format_procedure(fcinfo->flinfo->fn_oid))));

	int dim1 = ARR_DIMS(arg1)[0];
	int dim2 = ARR_DIMS(arg2)[0];

	if (dim1 != dim2)
		ereport(ERROR,
		       (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			errmsg("function \"%s\" called with invalid parameters",
			       format_procedure(fcinfo->flinfo->fn_oid))));
	
	float8 * x1 = (float8 *)ARR_DATA_PTR(arg1);
	float8 * x2 = (float8 *)ARR_DATA_PTR(arg2);

	float8 ret = 0; 
	for (int i=0; i!=dim1; i++)
		ret += (x1[i] - x2[i]) * (x1[i] - x2[i]);
	
	ret = exp(-1 * gamma * ret);

	PG_RETURN_FLOAT8(ret);
}

/*
 * This function evalues a support vector model on a data point.
 */
static float8 
svm_predict_eval(Oid koid, float8 * weights, ArrayType * supp_vectors, 
		 ArrayType * ind, int32 nsvs, int32 ind_dim)
{
	// We are not error-checking the ArrayTypes here because that has
	// been done in the calling function

	int i; float8 ret = 0;
	float8 * spvs = (float8 *)ARR_DATA_PTR(supp_vectors);

	ArrayType * spp_vec = construct_zero_array(ind_dim, FLOAT8OID, 8);
	float8 * spp_vec_data = (float8 *)ARR_DATA_PTR(spp_vec);

	for (i=0; i!=nsvs; i++) {
		// first copy the relevant portion of spvs to spp_vec_data
		memcpy(spp_vec_data, spvs+ind_dim*i, sizeof(float8) * ind_dim);
		ret += weights[i] * apply_kernel(koid, spp_vec, ind);
	}
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
	ArrayType * supp_vecs_arr = PG_GETARG_ARRAYTYPE_P(3);
	ArrayType * ind_arr = PG_GETARG_ARRAYTYPE_P(4);
	text * kernel = PG_GETARG_TEXT_P(5);

	float8 * weights;

	// input error checking 
	if (ARR_NULLBITMAP(weights_arr) || ARR_NDIM(weights_arr) != 1 ||
	    ARR_ELEMTYPE(weights_arr) != FLOAT8OID ||
	    ARR_NULLBITMAP(supp_vecs_arr) || 
	    ARR_NDIM(supp_vecs_arr) != 1 ||
	    ARR_ELEMTYPE(supp_vecs_arr) != FLOAT8OID ||
	    ARR_NULLBITMAP(ind_arr) || ARR_NDIM(ind_arr) != 1 ||
	    ARR_ELEMTYPE(ind_arr) != FLOAT8OID)
		ereport(ERROR,
		       (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			errmsg("function \"%s\" called with invalid parameters",
			       format_procedure(fcinfo->flinfo->fn_oid))));

	weights = (float8 *)ARR_DATA_PTR(weights_arr);

	Oid argtypes[2] = { FLOAT8ARRAYOID, FLOAT8ARRAYOID };
	List * funcname = textToQualifiedNameList(kernel);
	Oid koid = LookupFuncName(funcname, 2, argtypes, false);

	ret = svm_predict_eval(koid,weights,supp_vecs_arr,ind_arr,nsvs,ind_dim);
	
	PG_RETURN_FLOAT8(ret);
}


/*
 * This function extends a weight array with the weight of a new support vector.
 * The extra work of pre-allocating a larger block of memory doesn't appear
 * to make a difference in terms of total computation time.
 */
static int blocksize = 100;
static ArrayType * addNewWeight(ArrayType * weights, float8 weight, int nsvs) 
{
	float8 * weights_data = (float8 *)ARR_DATA_PTR(weights);

	if (nsvs % blocksize == 0) {
		ArrayType * ret_arr = 
			construct_zero_array(nsvs+blocksize, FLOAT8OID, 8);
		float8 * ret = (float8 *)ARR_DATA_PTR(ret_arr); 

		memcpy(ret, weights_data, sizeof(float8) * nsvs);
		ret[nsvs] = weight;

		return ret_arr;
	} else {
		weights_data[nsvs] = weight;
		return weights;
	}
}

/* 
 * This function extends a support vector array with a new support vector.
 * The extra work of pre-allocating a larger block of memory doesn't appear
 * to make a difference in terms of total computation time.
 */
static ArrayType * addNewSV(ArrayType * spvs, float8 * ind, int nsvs, int dim) 
{
	int i;
	float8 * spvs_data = (float8 *)ARR_DATA_PTR(spvs);

	if (nsvs % blocksize == 0) {
		ArrayType * ret_arr = 
			construct_zero_array((nsvs+blocksize)*dim, FLOAT8OID,8);
		float8 * ret = (float8 *)ARR_DATA_PTR(ret_arr); 

		memcpy(ret, spvs_data, sizeof(float8) * nsvs * dim);
		for (i=0; i!=dim; i++) ret[nsvs*dim + i] = ind[i];

		return ret_arr;
	} else {
		for (i=0; i!=dim; i++) spvs_data[nsvs*dim + i] = ind[i];
		return spvs;
	}
}

Datum svm_reg_update(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(svm_reg_update);

/**
 * This is the online support vector regression learning algorithm. 
 * The algorithm is set in the nu-SV regression setting.
 * The function updates the support vector model as it processes each new 
 * training example.
 * This function is wrapped in an aggregate function to process all the 
 * training examples stored in a table.  
 */
Datum svm_reg_update(PG_FUNCTION_ARGS)
{
	float8 p;             // prediction for data point
	float8 diff;          // difference between p and target label
	float8 error;         // absolute value of diff
	float8 weight;        // the weight of new support vector

	int i;

	// Get the input arguments and check for errors
	HeapTupleHeader t = PG_GETARG_HEAPTUPLEHEADER(0);
	ArrayType * ind_arr = PG_GETARG_ARRAYTYPE_P(1);
	float8 label = PG_GETARG_FLOAT8(2);
	text * kernel = PG_GETARG_TEXT_P(3);
	float8 eta = PG_GETARG_FLOAT8(4);
	float8 nu = PG_GETARG_FLOAT8(5);
	float8 slambda = PG_GETARG_FLOAT8(6);

	if (eta <= 0 || eta > 1 || nu <= 0 || nu > 1 || eta * slambda > 1)
		ereport(ERROR,
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 errmsg("function \"%s\" called with invalid parameter",
				format_procedure(fcinfo->flinfo->fn_oid))));

	if (ARR_NULLBITMAP(ind_arr) || ARR_NDIM(ind_arr) != 1 ||
	    ARR_ELEMTYPE(ind_arr) != FLOAT8OID)
		ereport(ERROR,
		       (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			errmsg("function \"%s\" called with invalid parameter",
			       format_procedure(fcinfo->flinfo->fn_oid))));

	float8 * ind = (float8 *)ARR_DATA_PTR(ind_arr);

	// Read the attributes of the input support vector model
	bool nil[10] = { 0,0,0,0,0,0,0,0,0,0 };

	int32 inds = DatumGetInt32(GetAttributeByName(t, "inds", &nil[0]));
	float8 cum_err =DatumGetFloat8(GetAttributeByName(t,"cum_err",&nil[1]));
	float8 epsilon =DatumGetFloat8(GetAttributeByName(t,"epsilon",&nil[2]));
	float8 rho = DatumGetFloat8(GetAttributeByName(t, "rho", &nil[3]));
	float8 b = DatumGetFloat8(GetAttributeByName(t, "b", &nil[4]));
	int32 nsvs = DatumGetInt32(GetAttributeByName(t, "nsvs", &nil[5]));
	int32 ind_dim =DatumGetInt32(GetAttributeByName(t, "ind_dim", &nil[6]));
	ArrayType * weights_arr = 
		DatumGetArrayTypeP(GetAttributeByName(t, "weights", &nil[7]));
	ArrayType * supp_vecs_arr = 
		DatumGetArrayTypeP(GetAttributeByName(t,"individuals",&nil[8]));
	Oid koid = DatumGetUInt32(GetAttributeByName(t, "kernel_oid",&nil[9]));

	for (i=0; i!=10; i++)
		if (nil[i]) elog(ERROR, "error reading support vector model");

	// The first time this function is called, the initial state doesn't
	// tell us the dimension of the data points; this needs to be extracted
	// from the ind argument. 
	if (ind_dim == 0) {
		int ndims = ARR_NDIM(ind_arr);
		int * dims = ARR_DIMS(ind_arr);
		ind_dim = ArrayGetNItems(ndims, dims);
	} 
	// The first time this function is called, we need to retrieve the
	// oid of the kernel function. This is an expensive operation that
	// we only want to do once at the begining of an aggregate.
	if (koid == 0) {
		Oid argtypes[2] = { FLOAT8ARRAYOID, FLOAT8ARRAYOID };
		List * funcname = textToQualifiedNameList(kernel);
		koid = LookupFuncName(funcname, 2, argtypes, false);
	}
	// Also, initially the weights_arr and supp_vecs_arr arrays 
	// are empty, so we can't do a dimension check until there are
	// support vectors.
	if (nsvs > 0) {
		if (ARR_NULLBITMAP(weights_arr) || ARR_NDIM(weights_arr) != 1 ||
		    ARR_ELEMTYPE(weights_arr) != FLOAT8OID || 
		    ARR_NULLBITMAP(supp_vecs_arr) || 
		    ARR_NDIM(supp_vecs_arr) != 1 ||
		    ARR_ELEMTYPE(supp_vecs_arr) != FLOAT8OID)
			ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			errmsg("function \"%s\" called with invalid parameters",
			       format_procedure(fcinfo->flinfo->fn_oid))));
	}

	float8 * weights = (float8 *)ARR_DATA_PTR(weights_arr);

	// This is the main regression update algorithm
	p = svm_predict_eval(koid,weights,supp_vecs_arr,ind_arr,nsvs,ind_dim) + b; 

	diff = label - p;
	error = fabs(diff);
			     
	inds++;
	cum_err = cum_err + error;

	float8 cap = 0.1 + 1 / (1 - eta * slambda);

	if (error > epsilon) {
		// unlike the original algorithm in Kivinen et al, this 
		// rescaling is only done when we make a large enough error
		for (i=0; i!=nsvs; i++) {
			// we need to avoid underflows; cap is designed to 
			// make sure we never go below DBL_MIN
			if (fabs(weights[i]) < (cap + 0.1) * DBL_MIN) { 
				weights[i] = 0;
				continue;
			}
			weights[i] = weights[i] * (1 - eta * slambda);
		}

		weight = diff < 0 ? -eta : eta;
		weights_arr = addNewWeight(weights_arr,weight,nsvs);
	        supp_vecs_arr = addNewSV(supp_vecs_arr,ind,nsvs,ind_dim);
		nsvs++;
		b = b + weight;
		epsilon = epsilon + (1 - nu) * eta;
	} else {
		epsilon = epsilon - eta * nu;
	}

	// Package up the attributes and return the resultant composite object
	Datum values[10];
	values[0] = Int32GetDatum(inds);
	values[1] = Float8GetDatum(cum_err);
	values[2] = Float8GetDatum(epsilon);
	values[3] = Float8GetDatum(rho);
	values[4] = Float8GetDatum(b);
	values[5] = Int32GetDatum(nsvs);
	values[6] = Int32GetDatum(ind_dim);
	values[7] = PointerGetDatum(weights_arr);
	values[8] = PointerGetDatum(supp_vecs_arr);
	values[9] = UInt32GetDatum(koid);

	TupleDesc tuple;
	if (get_call_result_type(fcinfo, NULL, &tuple) != TYPEFUNC_COMPOSITE)
		ereport(ERROR,
			(errcode( ERRCODE_FEATURE_NOT_SUPPORTED ),
			 errmsg( "function returning record called in context "
				 "that cannot accept type record" )));
	tuple = BlessTupleDesc(tuple);

	bool * isnulls = palloc0(10 * sizeof(bool));
	HeapTuple ret = heap_form_tuple(tuple, values, isnulls);

	for (i=0; i!=10; i++)
		if (isnulls[i])
			ereport(ERROR,
		       (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			errmsg("function \"%s\" produced null results",
			       format_procedure(fcinfo->flinfo->fn_oid))));

	PG_RETURN_DATUM(HeapTupleGetDatum(ret));
}

Datum svm_cls_update(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(svm_cls_update);

/**
 * This is the online support vector classification algorithm. 
 * The algorithm is set in the nu-SV classification setting. 
 * The function updates the support vector model as it processes each new 
 * training example.
 * This function is wrapped in an aggregate function to process all the 
 * training examples stored in a table.  
 */
Datum svm_cls_update(PG_FUNCTION_ARGS)
{
	float8 p;             // label * prediction for data point 
	int i;

	// Get the input arguments and check for errors
	HeapTupleHeader t = PG_GETARG_HEAPTUPLEHEADER(0);
	ArrayType * ind_arr = PG_GETARG_ARRAYTYPE_P(1);
	float8 label = PG_GETARG_FLOAT8(2);
	text * kernel = PG_GETARG_TEXT_P(3);
	float8 eta = PG_GETARG_FLOAT8(4);     // learning rate
	float8 nu = PG_GETARG_FLOAT8(5);  /* compression parameter; about nu
					   * fraction of the training data will
					   * become support vectors
					   */

	if (eta <= 0 || eta > 1 || nu <= 0 || nu > 1)
		ereport(ERROR,
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 errmsg("function \"%s\" called with invalid parameter",
				format_procedure(fcinfo->flinfo->fn_oid))));

	if (ARR_NULLBITMAP(ind_arr) || ARR_NDIM(ind_arr) != 1 ||
	    ARR_ELEMTYPE(ind_arr) != FLOAT8OID)
		ereport(ERROR,
		       (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			errmsg("function \"%s\" called with invalid parameter",
			       format_procedure(fcinfo->flinfo->fn_oid))));

	float8 * ind = (float8 *)ARR_DATA_PTR(ind_arr);

	// Read the attributes of the input support vector model
	bool nil[10] = { 0,0,0,0,0,0,0,0,0 };

	int32 inds = DatumGetInt32(GetAttributeByName(t, "inds", &nil[0]));
	float8 cum_err =DatumGetFloat8(GetAttributeByName(t,"cum_err",&nil[1]));
	float8 epsilon =DatumGetFloat8(GetAttributeByName(t,"epsilon",&nil[2]));
	float8 rho = DatumGetFloat8(GetAttributeByName(t, "rho", &nil[3]));
	float8 b = DatumGetFloat8(GetAttributeByName(t, "b", &nil[4]));
	int32 nsvs = DatumGetInt32(GetAttributeByName(t, "nsvs", &nil[5]));
	int32 ind_dim =DatumGetInt32(GetAttributeByName(t, "ind_dim", &nil[6]));
	ArrayType * weights_arr = 
		DatumGetArrayTypeP(GetAttributeByName(t, "weights", &nil[7]));
	ArrayType * supp_vecs_arr = 
		DatumGetArrayTypeP(GetAttributeByName(t,"individuals",&nil[8]));
	Oid koid = DatumGetUInt32(GetAttributeByName(t, "kernel_oid",&nil[9]));

	for (i=0; i!=10; i++)
		if (nil[i]) elog(ERROR, "error reading support vector model");

	// The first time this function is called, the initial state doesn't
	// tell us the dimension of the data points; this needs to be extracted
	// from the ind argument. 
	if (ind_dim == 0) {
		int ndims = ARR_NDIM(ind_arr);
		int * dims = ARR_DIMS(ind_arr);
		ind_dim = ArrayGetNItems(ndims, dims);
	} 
	// The first time this function is called, we need to retrieve the
	// oid of the kernel function. This is an expensive operation that
	// we only want to do once at the begining of an aggregate.
	if (koid == 0) {
		Oid argtypes[2] = { FLOAT8ARRAYOID, FLOAT8ARRAYOID };
		List * funcname = textToQualifiedNameList(kernel);
		koid = LookupFuncName(funcname, 2, argtypes, false);
	}
	// Also, initially the weights_arr and supp_vecs_arr arrays 
	// are empty, so we can't do a dimension check until there are
	// support vectors.
	if (nsvs > 0) {
		if (ARR_NULLBITMAP(weights_arr) || ARR_NDIM(weights_arr) != 1 ||
		    ARR_ELEMTYPE(weights_arr) != FLOAT8OID || 
		    ARR_NULLBITMAP(supp_vecs_arr) || 
		    ARR_NDIM(supp_vecs_arr) != 1 ||
		    ARR_ELEMTYPE(supp_vecs_arr) != FLOAT8OID) 
			ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			errmsg("function \"%s\" called with invalid array parameters",
			       format_procedure(fcinfo->flinfo->fn_oid))));
	}

	float8 * weights = (float8 *)ARR_DATA_PTR(weights_arr);

	// This is the nu-SV classification update algorithm.
	p = svm_predict_eval(koid,weights,supp_vecs_arr,ind_arr,nsvs,ind_dim) + b; 
	// elog(NOTICE, "%f \t %f \t %d", label, p, nsvs);

	p = label * p;

	inds++;
	if (p <= 0) cum_err++;

	if (p <= rho) {
		// unlike the original algorithm in Kivinen et al, this 
		// rescaling is only done when we make a large enough error
		for (i=0; i!=nsvs; i++) {
			// we need to avoid underflows; the weight discounting
			// never multiply a weight by less than 0.9, 
			// and 1.15 * 0.9 > 1
			if (fabs(weights[i]) < 1.15 * DBL_MIN) { 
				weights[i] = 0;
				continue;
			}
			// we set lambda = 0.1 here, the exact value of lambda
			// is mathematically irrelevant
			weights[i] = weights[i] * (1 - 0.1*eta); 
		}

		weights_arr = addNewWeight(weights_arr,label * eta,nsvs);
	        supp_vecs_arr = addNewSV(supp_vecs_arr,ind,nsvs,ind_dim);
		nsvs++;
		b = b + eta * label;
		rho = rho - eta * (1 - nu);
	} else {
		rho = rho + eta * nu; 
	}

	// Package up the attributes and return the resultant composite object
	Datum values[10];
	values[0] = Int32GetDatum(inds);
	values[1] = Float8GetDatum(cum_err);
	values[2] = Float8GetDatum(epsilon);
	values[3] = Float8GetDatum(rho);
	values[4] = Float8GetDatum(b);
	values[5] = Int32GetDatum(nsvs);
	values[6] = Int32GetDatum(ind_dim);
	values[7] = PointerGetDatum(weights_arr);
	values[8] = PointerGetDatum(supp_vecs_arr);
	values[9] = UInt32GetDatum(koid);

	TupleDesc tuple;
	if (get_call_result_type(fcinfo, NULL, &tuple) != TYPEFUNC_COMPOSITE)
		ereport(ERROR,
			(errcode( ERRCODE_FEATURE_NOT_SUPPORTED ),
			 errmsg( "function returning record called in context "
				 "that cannot accept type record" )));
	tuple = BlessTupleDesc(tuple);

	bool * isnulls = palloc0(10 * sizeof(bool));
	HeapTuple ret = heap_form_tuple(tuple, values, isnulls);

	for (i=0; i!=10; i++)
		if (isnulls[i])
			ereport(ERROR,
		       (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			errmsg("function \"%s\" produced null results %d",
			       format_procedure(fcinfo->flinfo->fn_oid),i)));

	PG_RETURN_DATUM(HeapTupleGetDatum(ret));
}

Datum svm_nd_update(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(svm_nd_update);

/**
 * This is the online support vector novelty detection algorithm. 
 * The algorithm is set in the nu-SV setting. 
 * The function updates the support vector model as it processes each new 
 * training example.
 * This function is wrapped in an aggregate function to process all the 
 * training examples stored in a table.  
 */
Datum svm_nd_update(PG_FUNCTION_ARGS)
{
	float8 p;             // prediction for data point 
	int i;

	// Get the input arguments and check for errors
	HeapTupleHeader t = PG_GETARG_HEAPTUPLEHEADER(0);
	ArrayType * ind_arr = PG_GETARG_ARRAYTYPE_P(1);
	text * kernel = PG_GETARG_TEXT_P(2);
	float8 eta = PG_GETARG_FLOAT8(3);     // learning rate
	float8 nu = PG_GETARG_FLOAT8(4);  /* compression parameter; about nu
					   * fraction of the training data will
					   * become support vectors
					   */

	if (eta <= 0 || eta > 1 || nu <= 0 || nu > 1)
		ereport(ERROR,
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 errmsg("function \"%s\" called with invalid parameter",
				format_procedure(fcinfo->flinfo->fn_oid))));

	if (ARR_NULLBITMAP(ind_arr) || ARR_NDIM(ind_arr) != 1 ||
	    ARR_ELEMTYPE(ind_arr) != FLOAT8OID)
		ereport(ERROR,
		       (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			errmsg("function \"%s\" called with invalid parameter",
			       format_procedure(fcinfo->flinfo->fn_oid))));

	float8 * ind = (float8 *)ARR_DATA_PTR(ind_arr);

	// Read the attributes of the input support vector model
	bool nil[10] = { 0,0,0,0,0,0,0,0,0 };

	int32 inds = DatumGetInt32(GetAttributeByName(t, "inds", &nil[0]));
	float8 cum_err =DatumGetFloat8(GetAttributeByName(t,"cum_err",&nil[1]));
	float8 epsilon =DatumGetFloat8(GetAttributeByName(t,"epsilon",&nil[2]));
	float8 rho = DatumGetFloat8(GetAttributeByName(t, "rho", &nil[3]));
	float8 b = DatumGetFloat8(GetAttributeByName(t, "b", &nil[4]));
	int32 nsvs = DatumGetInt32(GetAttributeByName(t, "nsvs", &nil[5]));
	int32 ind_dim =DatumGetInt32(GetAttributeByName(t, "ind_dim", &nil[6]));
	ArrayType * weights_arr = 
		DatumGetArrayTypeP(GetAttributeByName(t, "weights", &nil[7]));
	ArrayType * supp_vecs_arr = 
		DatumGetArrayTypeP(GetAttributeByName(t,"individuals",&nil[8]));
	Oid koid = DatumGetUInt32(GetAttributeByName(t, "kernel_oid",&nil[9]));

	for (i=0; i!=10; i++)
		if (nil[i]) elog(ERROR, "error reading support vector model");

	// The first time this function is called, the initial state doesn't
	// tell us the dimension of the data points; this needs to be extracted
	// from the ind argument. 
	if (ind_dim == 0) {
		int ndims = ARR_NDIM(ind_arr);
		int * dims = ARR_DIMS(ind_arr);
		ind_dim = ArrayGetNItems(ndims, dims);
	} 
	// The first time this function is called, we need to retrieve the
	// oid of the kernel function. This is an expensive operation that
	// we only want to do once at the begining of an aggregate.
	if (koid == 0) {
		Oid argtypes[2] = { FLOAT8ARRAYOID, FLOAT8ARRAYOID };
		List * funcname = textToQualifiedNameList(kernel);
		koid = LookupFuncName(funcname, 2, argtypes, false);
	}
	// Also, initially the weights_arr and supp_vecs_arr arrays 
	// are empty, so we can't do a dimension check until there are
	// support vectors.
	if (nsvs > 0) {
		if (ARR_NULLBITMAP(weights_arr) || ARR_NDIM(weights_arr) != 1 ||
		    ARR_ELEMTYPE(weights_arr) != FLOAT8OID || 
		    ARR_NULLBITMAP(supp_vecs_arr) || 
		    ARR_NDIM(supp_vecs_arr) != 1 ||
		    ARR_ELEMTYPE(supp_vecs_arr) != FLOAT8OID) 
			ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			errmsg("function \"%s\" called with invalid array parameters",
			       format_procedure(fcinfo->flinfo->fn_oid))));
	}

	float8 * weights = (float8 *)ARR_DATA_PTR(weights_arr);

	// This is the nu-SV novelty detection update algorithm.
	p = svm_predict_eval(koid,weights,supp_vecs_arr,ind_arr,nsvs,ind_dim); 
	inds++;

	if (p < rho) {
		// unlike the original algorithm in Kivinen et al, this 
		// rescaling is only done when we make a large enough error
		for (i=0; i!=nsvs; i++) {
			// we need to avoid underflows; the weight discounting
			// never multiply a weight by less than 0.9, 
			// and 1.15 * 0.9 > 1
			if (weights[i] < 1.15 * DBL_MIN) { 
				weights[i] = 0;
				continue;
			}
			// we set lambda = 0.1 here, the exact value of lambda
			// is mathematically irrelevant
			weights[i] = weights[i] * (1 - 0.1*eta); 
		}

		weights_arr = addNewWeight(weights_arr,eta,nsvs);
	        supp_vecs_arr = addNewSV(supp_vecs_arr,ind,nsvs,ind_dim);
		nsvs++;
		rho = rho - eta * (1 - nu);
	} else {
		rho = rho + eta * nu; 
	}

	// Package up the attributes and return the resultant composite object
	Datum values[10];
	values[0] = Int32GetDatum(inds);
	values[1] = Float8GetDatum(cum_err);
	values[2] = Float8GetDatum(epsilon);
	values[3] = Float8GetDatum(rho);
	values[4] = Float8GetDatum(b);
	values[5] = Int32GetDatum(nsvs);
	values[6] = Int32GetDatum(ind_dim);
	values[7] = PointerGetDatum(weights_arr);
	values[8] = PointerGetDatum(supp_vecs_arr);
	values[9] = UInt32GetDatum(koid);

	TupleDesc tuple;
	if (get_call_result_type(fcinfo, NULL, &tuple) != TYPEFUNC_COMPOSITE)
		ereport(ERROR,
			(errcode( ERRCODE_FEATURE_NOT_SUPPORTED ),
			 errmsg( "function returning record called in context "
				 "that cannot accept type record" )));
	tuple = BlessTupleDesc(tuple);

	bool * isnulls = palloc0(10 * sizeof(bool));
	HeapTuple ret = heap_form_tuple(tuple, values, isnulls);

	for (i=0; i!=10; i++)
		if (isnulls[i])
			ereport(ERROR,
		       (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			errmsg("function \"%s\" produced null results %d",
			       format_procedure(fcinfo->flinfo->fn_oid),i)));

	PG_RETURN_DATUM(HeapTupleGetDatum(ret));
}
