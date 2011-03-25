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

/**
 * This is the main online support vector regression learning algorithm. 
 * The function updates the support vector model as it processes each new 
 * training example.
 * This function is wrapped in an aggregate function to process all the 
 * training examples stored in a table.  
 * The learning parameters (eta, slambda, and nu) are hardcoded at the moment. 
 * We may want to make them input parameters at some stage, although the naive 
 * user would probably be daunted with the prospect of having to specify them.
 */
Datum svm_reg_update(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(svm_reg_update);

Datum svm_reg_update(PG_FUNCTION_ARGS)
{
	float8 eta = 0.05;    // learning rate
	float8 slambda = 0.2; // regularisation parameter
	float8 nu = 0.001;    // compression parameter in [0,1]
	float8 p;             // prediction for data point
	float8 diff;          // difference between p and target label
	float8 error;         // absolute value of diff
	float8 weight;        // the weight of new support vector

	int i;

	// Get the input arguments and check for errors
	HeapTupleHeader t = PG_GETARG_HEAPTUPLEHEADER(0);
	ArrayType * ind_arr = PG_GETARG_ARRAYTYPE_P(1);
	float8 label = PG_GETARG_FLOAT8(2);

	if (ARR_NULLBITMAP(ind_arr) || ARR_NDIM(ind_arr) != 1 ||
	    ARR_ELEMTYPE(ind_arr) != FLOAT8OID)
		ereport(ERROR,
		       (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			errmsg("function \"%s\" called with invalid ind parameter",
			       format_procedure(fcinfo->flinfo->fn_oid))));

	float8 * ind = (float8 *)ARR_DATA_PTR(ind_arr);

	// Read the attributes of the input support vector model
	bool nil[9] = { 0,0,0,0,0,0,0,0,0 };

	int32 inds = DatumGetInt32(GetAttributeByName(t, "inds", &nil[0]));
	float8 cum_err =DatumGetFloat8(GetAttributeByName(t,"cum_err",&nil[1]));
	float8 epsilon =DatumGetFloat8(GetAttributeByName(t,"epsilon",&nil[2]));
	float8 rho = DatumGetFloat8(GetAttributeByName(t, "rho", &nil[3]));
	float8 b = DatumGetFloat8(GetAttributeByName(t, "b", &nil[4]));
	int32 nsvs = DatumGetInt32(GetAttributeByName(t, "nsvs", &nil[5]));
	int32 ind_dim =DatumGetInt32(GetAttributeByName(t, "ind_dim", &nil[6]));
	ArrayType * weights_arr = 
		DatumGetArrayTypeP(GetAttributeByName(t, "weights", &nil[7]));
	ArrayType * support_vectors_arr = 
		DatumGetArrayTypeP(GetAttributeByName(t,"individuals",&nil[8]));

	for (i=0; i!=9; i++)
		if (nil[i]) elog(ERROR, "error reading support vector model");

	// The first time this function is called, the initial state doesn't
	// tell us the dimension of the data points; this needs to be extracted
	// from the ind argument. 
	if (ind_dim == 0) {
		int ndims = ARR_NDIM(ind_arr);
		int * dims = ARR_DIMS(ind_arr);
		ind_dim = ArrayGetNItems(ndims, dims);
	} 
	// Also, initially the weights_arr and support_vectors_arr arrays 
	// are empty, so we can't do a dimension check until there are
	// support vectors.
	if (nsvs > 0) {
		if (ARR_NULLBITMAP(weights_arr) || ARR_NDIM(weights_arr) != 1 ||
		    ARR_ELEMTYPE(weights_arr) != FLOAT8OID || 
		    ARR_NULLBITMAP(support_vectors_arr) || 
		    ARR_NDIM(support_vectors_arr) != 1 ||
		    ARR_ELEMTYPE(support_vectors_arr) != FLOAT8OID)
			ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			errmsg("function \"%s\" called with invalid parameters",
			       format_procedure(fcinfo->flinfo->fn_oid))));
	}

	float8 * weights = (float8 *)ARR_DATA_PTR(weights_arr);
	float8 * support_vectors = (float8 *)ARR_DATA_PTR(support_vectors_arr);

	// This is the main regression update algorithm
	p = svm_predict_eval(weights, support_vectors, ind, nsvs, ind_dim); 
	diff = label - p;
	error = fabs(diff);
			     
	inds++;
	cum_err = cum_err + error;

	if (error > epsilon) {
		// unlike the original algorithm in Kivinen et at, this 
		// rescaling is only done when we make a large enough error
		for (i=0; i!=nsvs; i++) 
			weights[i] = weights[i] * (1 - eta * slambda);

		weight = diff < 0 ? -eta : eta;
		weights_arr = addNewWeight(weights_arr,weight,nsvs);
	        support_vectors_arr = addNewSV(support_vectors_arr,ind,nsvs,ind_dim);
		nsvs++;
		epsilon = epsilon + (1 - nu) * eta;
	} else {
		epsilon = epsilon - eta * nu;
	}

	// Package up the attributes and return the resultant composite object
	Datum values[9];
	values[0] = Int32GetDatum(inds);
	values[1] = Float8GetDatum(cum_err);
	values[2] = Float8GetDatum(epsilon);
	values[3] = Float8GetDatum(rho);
	values[4] = Float8GetDatum(b);
	values[5] = Int32GetDatum(nsvs);
	values[6] = Int32GetDatum(ind_dim);
	values[7] = PointerGetDatum(weights_arr);
	values[8] = PointerGetDatum(support_vectors_arr);

	TupleDesc tuple;
	if (get_call_result_type(fcinfo, NULL, &tuple) != TYPEFUNC_COMPOSITE)
		ereport(ERROR,
			(errcode( ERRCODE_FEATURE_NOT_SUPPORTED ),
			 errmsg( "function returning record called in context "
				 "that cannot accept type record" )));
	tuple = BlessTupleDesc(tuple);

	bool * isnulls = palloc0(9 * sizeof(bool));
	HeapTuple ret = heap_form_tuple(tuple, values, isnulls);

	for (i=0; i!=9; i++)
		if (isnulls[i])
			ereport(ERROR,
		       (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			errmsg("function \"%s\" produced null results",
			       format_procedure(fcinfo->flinfo->fn_oid),i)));

	PG_RETURN_DATUM(HeapTupleGetDatum(ret));
}


/**
 * This is the main online support vector classification algorithm. 
 * The function updates the support vector model as it processes each new 
 * training example.
 * This function is wrapped in an aggregate function to process all the 
 * training examples stored in a table.  
 * The learning parameters (eta and nu) are hardcoded at the moment. 
 * We may want to make them input parameters at some stage, although the 
 * naive user would probably be daunted with the prospect of having to 
 * specify them. 
 */
Datum svm_cls_update(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(svm_cls_update);

Datum svm_cls_update(PG_FUNCTION_ARGS)
{
	float8 p;             // label * prediction for data point 
	int i;

	// Get the input arguments and check for errors
	HeapTupleHeader t = PG_GETARG_HEAPTUPLEHEADER(0);
	ArrayType * ind_arr = PG_GETARG_ARRAYTYPE_P(1);
	float8 label = PG_GETARG_FLOAT8(2);
	float8 eta = PG_GETARG_FLOAT8(3);     // learning rate
	float8 slambda = PG_GETARG_FLOAT8(4); // regularisation parameter
	float8 rho = PG_GETARG_FLOAT8(5);     // margin

	if (ARR_NULLBITMAP(ind_arr) || ARR_NDIM(ind_arr) != 1 ||
	    ARR_ELEMTYPE(ind_arr) != FLOAT8OID)
		ereport(ERROR,
		       (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			errmsg("function \"%s\" called with invalid ind parameter",
			       format_procedure(fcinfo->flinfo->fn_oid))));

	float8 * ind = (float8 *)ARR_DATA_PTR(ind_arr);

	// Read the attributes of the input support vector model
	bool nil[9] = { 0,0,0,0,0,0,0,0,0 };

	int32 inds = DatumGetInt32(GetAttributeByName(t, "inds", &nil[0]));
	float8 cum_err =DatumGetFloat8(GetAttributeByName(t,"cum_err",&nil[1]));
	float8 epsilon =DatumGetFloat8(GetAttributeByName(t,"epsilon",&nil[2]));
	float8 rho2 = DatumGetFloat8(GetAttributeByName(t, "rho", &nil[3]));
	float8 b = DatumGetFloat8(GetAttributeByName(t, "b", &nil[4]));
	int32 nsvs = DatumGetInt32(GetAttributeByName(t, "nsvs", &nil[5]));
	int32 ind_dim =DatumGetInt32(GetAttributeByName(t, "ind_dim", &nil[6]));
	ArrayType * weights_arr = 
		DatumGetArrayTypeP(GetAttributeByName(t, "weights", &nil[7]));
	ArrayType * support_vectors_arr = 
		DatumGetArrayTypeP(GetAttributeByName(t,"individuals",&nil[8]));

	for (i=0; i!=9; i++)
		if (nil[i]) elog(ERROR, "error reading support vector model");

	// The first time this function is called, the initial state doesn't
	// tell us the dimension of the data points; this needs to be extracted
	// from the ind argument. 
	if (ind_dim == 0) {
		int ndims = ARR_NDIM(ind_arr);
		int * dims = ARR_DIMS(ind_arr);
		ind_dim = ArrayGetNItems(ndims, dims);
	} 
	// Also, initially the weights_arr and support_vectors_arr arrays 
	// are empty, so we can't do a dimension check until there are
	// support vectors.
	if (nsvs > 0) {
		if (ARR_NULLBITMAP(weights_arr) || ARR_NDIM(weights_arr) != 1 ||
		    ARR_ELEMTYPE(weights_arr) != FLOAT8OID || 
		    ARR_NULLBITMAP(support_vectors_arr) || 
		    ARR_NDIM(support_vectors_arr) != 1 ||
		    ARR_ELEMTYPE(support_vectors_arr) != FLOAT8OID) 
			ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			errmsg("function \"%s\" called with invalid array parameters",
			       format_procedure(fcinfo->flinfo->fn_oid))));
	}

	float8 * weights = (float8 *)ARR_DATA_PTR(weights_arr);
	float8 * support_vectors = (float8 *)ARR_DATA_PTR(support_vectors_arr);

	// This is the main classification update algorithm.
	// When rho = 0 and slambda = 0, this is equiv. to kernel perceptron.
	// When rho = 0 and slambda > 0, this is equiv. to kernel perceptron 
	// with regularisation
	// The standard SVM case is when rho > 0 and slambda > 0.
	p = svm_predict_eval(weights, support_vectors, ind, nsvs, ind_dim) + b; 
	p = label * p;

	inds++;
	if (p < 0) cum_err++;

	if (p <= rho) {
		// unlike the original algorithm in Kivinen et at, this 
		// rescaling is only done when we make a large enough error
		for (i=0; i!=nsvs; i++) 
			weights[i] = weights[i] * (1 - eta * slambda);

		weights_arr = addNewWeight(weights_arr,label * eta,nsvs);
	        support_vectors_arr = addNewSV(support_vectors_arr,ind,nsvs,ind_dim);
		nsvs++;
		b = b + eta * label;
	}

	// Package up the attributes and return the resultant composite object
	Datum values[9];
	values[0] = Int32GetDatum(inds);
	values[1] = Float8GetDatum(cum_err);
	values[2] = Float8GetDatum(epsilon);
	values[3] = Float8GetDatum(rho);
	values[4] = Float8GetDatum(b);
	values[5] = Int32GetDatum(nsvs);
	values[6] = Int32GetDatum(ind_dim);
	values[7] = PointerGetDatum(weights_arr);
	values[8] = PointerGetDatum(support_vectors_arr);

	TupleDesc tuple;
	if (get_call_result_type(fcinfo, NULL, &tuple) != TYPEFUNC_COMPOSITE)
		ereport(ERROR,
			(errcode( ERRCODE_FEATURE_NOT_SUPPORTED ),
			 errmsg( "function returning record called in context "
				 "that cannot accept type record" )));
	tuple = BlessTupleDesc(tuple);

	bool * isnulls = palloc0(9 * sizeof(bool));
	HeapTuple ret = heap_form_tuple(tuple, values, isnulls);

	for (i=0; i!=9; i++)
		if (isnulls[i])
			ereport(ERROR,
		       (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			errmsg("function \"%s\" produced null results %d",
			       format_procedure(fcinfo->flinfo->fn_oid),i)));

	PG_RETURN_DATUM(HeapTupleGetDatum(ret));
}

