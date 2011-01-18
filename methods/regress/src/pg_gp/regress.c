/* -----------------------------------------------------------------------------
 *
 * regress.c
 *
 * Multi-Linear Regression.
 *
 * Copyright (c) 2010, EMC
 *
 * -----------------------------------------------------------------------------
 */

#include "postgres.h"

#include "regress.h"
#include "pinv.h"
#include "matrix.h"
#include "student.h"

#include "funcapi.h"
#include "catalog/pg_type.h"
#include "utils/array.h"
#include "utils/builtins.h"

#include <math.h>
#include <string.h>


/* Indicate "version 1" calling conventions for all exported functions. */

PG_FUNCTION_INFO_V1(float8_mregr_accum);
PG_FUNCTION_INFO_V1(float8_mregr_combine);
PG_FUNCTION_INFO_V1(float8_mregr_coef);
PG_FUNCTION_INFO_V1(float8_mregr_r2);
PG_FUNCTION_INFO_V1(float8_mregr_tstats);
PG_FUNCTION_INFO_V1(float8_mregr_pvalues);



typedef struct {
	int			len;		/* scalar:               len(X[]) */
	float8		count;		/* scalar:               count(*) */
	float8		sumy;		/* scalar:               sum(y)   */
	float8		sumy2;		/* scalar:               sum(y*y) */
	ArrayType	*Xty;		/* vector[count]:        sum(X'[] * y) */
	ArrayType	*_Xty_t;	/* redundant: Xty transposed */
	ArrayType	*XtX;		/* matrix[count][count]: sum(X'[] * X[]) */
	ArrayType	*_XtX_inv;	/* redundant: pseudo-inverse of XtX */
} MRegrState;


/* Prototypes for static functions */

static bool float8_mregr_get_state(PG_FUNCTION_ARGS,
								   MRegrState *outState);
static void float8_mregr_compute(MRegrState	*inState,
								 ArrayType	**outCoef,
								 float8		*outR2,
								 ArrayType	**outTStats,
								 ArrayType	**outPValues);

PG_MODULE_MAGIC;


Datum
float8_mregr_accum(PG_FUNCTION_ARGS)
{
	ArrayType  *state;
	float8     *stateData, *newData, *matrix;
	float8      newY;
	ArrayType  *newX;
	int         len, statelen;	
	int         i,j;
	
	/* We should be strict, but it doesn't hurt to be paranoid */
	if (PG_ARGISNULL(0) || PG_ARGISNULL(1) || PG_ARGISNULL(2))
	{
		if (PG_ARGISNULL(0))
			PG_RETURN_NULL();
		PG_RETURN_ARRAYTYPE_P(PG_GETARG_ARRAYTYPE_P(0));
	}
	
	state = PG_GETARG_ARRAYTYPE_P(0);	
	newY  = PG_GETARG_FLOAT8(1);
    newX  = PG_GETARG_ARRAYTYPE_P(2);
	
	/* Ensure that both arrays are single dimensional float8[] arrays */
	if (ARR_NULLBITMAP(state) || ARR_NDIM(state) != 1 || 
		ARR_ELEMTYPE(state) != FLOAT8OID ||
		ARR_NDIM(newX) != 1 || ARR_ELEMTYPE(newX) != FLOAT8OID)
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("transition function \"%s\" called with invalid parameters",
					format_procedure(fcinfo->flinfo->fn_oid))));
	
	/* Only callable as a transition function */
	if (!(fcinfo->context && IsA(fcinfo->context, AggState)))
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("transition function \"%s\" not called from aggregate",
					format_procedure(fcinfo->flinfo->fn_oid))));
	
	/* newX with nulls will be ignored */
	if (ARR_NULLBITMAP(newX))
		PG_RETURN_ARRAYTYPE_P(state);
	
	/*
	 * If length(state) == 1 then it is an unitialized state, extend as
	 * needed, we use this instead of NULL so that we can declare the
	 * function as strict.
	 */
	len = ARR_DIMS(newX)[0];
	statelen = 4 + len + len*len;
	if (ARR_DIMS(state)[0] == 1)
	{
		int size = statelen * sizeof(int64) + ARR_OVERHEAD_NONULLS(1);
		state = (ArrayType *) palloc(size);
		SET_VARSIZE(state, size);
		state->ndim = 1;
		state->dataoffset = 0;
		state->elemtype = FLOAT8OID;
		ARR_DIMS(state)[0] = statelen;
		ARR_LBOUND(state)[0] = 1;
		stateData = (float8*) ARR_DATA_PTR(state);
		memset(stateData, 0, statelen * sizeof(int64));
		stateData[0] = len;
	}
	
	/* 
	 * Contents of 'state' are as follows:
	 *   [0]     = len(X[])
	 *   [1]     = count
	 *   [2]     = sum(y)
	 *   [3]     = sum(y*y)
	 *   [4:N]   = sum(X'[] * y) 
	 *   [N+1:M] = sum(X[] * X'[])
	 *   N       = 3 + len(X)
	 *   M       = N + len(X)*len(X)
	 */
	newData   = (float8*) ARR_DATA_PTR(newX);
	stateData = (float8*) ARR_DATA_PTR(state);
	
	/* It is an error if the number of indepent variables is not constant */
	if (stateData[0] != len)
	{
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("transition function \"%s\" called with invalid parameters",
					format_procedure(fcinfo->flinfo->fn_oid)),
				 errdetail("The independent-variable array is not of constant width.")));
	}
	
	/* Something is seriously fishy if our state has the wrong length */
	if (ARR_DIMS(state)[0] != statelen)
	{
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("transition function \"%s\" called with invalid parameters",
					format_procedure(fcinfo->flinfo->fn_oid))));
	}
	
	/* Okay... All's good now do the work */
	stateData[1]++;
	stateData[2] += newY;
	stateData[3] += newY * newY;
	for (i = 0; i < len; i++)
		stateData[4+i] += newY * newData[i];
	
	/* Compute the matrix X[] * X'[] and add it in */
	matrix = stateData + 4 + len;
	for (i = 0; i < len; i++)
		for (j = 0; j < len; j++)
			matrix[i*len + j] += newData[i] * newData[j];
	
	PG_RETURN_ARRAYTYPE_P(state);
}


Datum
float8_mregr_combine(PG_FUNCTION_ARGS)
{
	ArrayType  *state1, *state2, *result;
	float8     *state1Data, *state2Data, *resultData;
	int         i, size;
	int         len, statelen;
	
	/* We should be strict, but it doesn't hurt to be paranoid */
	if (PG_ARGISNULL(0))
	{
		if (PG_ARGISNULL(1))
			PG_RETURN_NULL();
		PG_RETURN_ARRAYTYPE_P(PG_GETARG_ARRAYTYPE_P(1));
	}
	if (PG_ARGISNULL(1))
		PG_RETURN_ARRAYTYPE_P(PG_GETARG_ARRAYTYPE_P(0));
	
	state1 = PG_GETARG_ARRAYTYPE_P(0);	
	state2 = PG_GETARG_ARRAYTYPE_P(1);
	
	/* Ensure that both arrays are single dimensional float8[] arrays */
	if (ARR_NULLBITMAP(state1) || ARR_NULLBITMAP(state2) || 
		ARR_NDIM(state1) != 1 || ARR_NDIM(state2) != 1 || 
		ARR_ELEMTYPE(state1) != FLOAT8OID || ARR_ELEMTYPE(state2) != FLOAT8OID)
	{
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("preliminary segment-level calculation function \"%s\" called with invalid parameters",
					format_procedure(fcinfo->flinfo->fn_oid))));
	}
	
	/* 
	 * Remember that we initialized to {0}, so if either array is still at
	 * the initial value then just return the other one 
	 */
	if (ARR_DIMS(state1)[0] == 1)
		PG_RETURN_ARRAYTYPE_P(state2);
	if (ARR_DIMS(state2)[0] == 1)
		PG_RETURN_ARRAYTYPE_P(state1);
	
	state1Data = (float8*) ARR_DATA_PTR(state1);
	state2Data = (float8*) ARR_DATA_PTR(state2);
	
	if (ARR_DIMS(state1)[0] != ARR_DIMS(state2)[0] || 
		state1Data[0] != state2Data[0])
	{
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("preliminary segment-level calculation function \"%s\" called with invalid parameters",
					format_procedure(fcinfo->flinfo->fn_oid)),
				 errdetail("The independent-variable array is not of constant width.")));
	}
	len = state1Data[0];
	statelen = 4 + len + len*len;
	if (ARR_DIMS(state1)[0] != statelen)
	{
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("preliminary segment-level calculation function \"%s\" called with invalid parameters",
					format_procedure(fcinfo->flinfo->fn_oid))));
	}
	
	/* Validations pass, allocate memory for result and do work */
	size = statelen * sizeof(int64) + ARR_OVERHEAD_NONULLS(1);
	result = (ArrayType *) palloc(size);
	SET_VARSIZE(result, size);
	result->ndim = 1;
	result->dataoffset = 0;
	result->elemtype = FLOAT8OID;
	ARR_DIMS(result)[0] = statelen;
	ARR_LBOUND(result)[0] = 1;
	resultData = (float8*) ARR_DATA_PTR(result);
	memset(resultData, 0, statelen * sizeof(int64));
	
	/* 
	 * Contents of 'state' are as follows:
	 *   [0]     = len(X[])
	 *   [1]     = count
	 *   [2]     = sum(y)
	 *   [3]     = sum(y*y)
	 *   [4:N]   = sum(X'[] * y) 
	 *   [N+1:M] = sum(X[] * X'[])
	 *   N       = 3 + len(X)
	 *   M       = N + len(X)*len(X)
	 */
	resultData[0] = len;
	for (i = 1; i < statelen; i++)
		resultData[i] = state1Data[i] + state2Data[i];	
	PG_RETURN_ARRAYTYPE_P(result);
}


/*
 * float8_mregr_get_state()
 *
 * Check that a valid state is passed to the aggregate's final function.
 * PG_FUNCTION_ARGS expands to "FunctionCallInfo fcinfo". If we return
 * false, the calling function should return NULL.
 */

static bool
float8_mregr_get_state(PG_FUNCTION_ARGS,
					   MRegrState *outState)
{
	Datum		tempDatum;
	ArrayType	*in;
	float8		*data;
	int			dims[2], lbs[2]; /* dimensions, lower bounds for arrays */
	
	/* Input should be a single parameter, the aggregate state */
	if (PG_NARGS() != 1)
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("final calculation function \"%s\" called with invalid parameters",
					format_procedure(fcinfo->flinfo->fn_oid))));
	
	if (PG_ARGISNULL(0))
		return false;
	
	/* Validate array type */
	in = PG_GETARG_ARRAYTYPE_P(0);
	if (ARR_ELEMTYPE(in) != FLOAT8OID || ARR_NDIM(in) != 1 || ARR_NULLBITMAP(in))
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("final calculation function \"%s\" called with invalid parameters",
					format_procedure(fcinfo->flinfo->fn_oid))));
	
	/* Validate the correct size input */
	if (ARR_DIMS(in)[0] < 2)
		return false;  /* no input */
	
	data = (float8*) ARR_DATA_PTR(in);
	outState->len    = (int) data[0];   /* scalar:           len(X[]) */
	if (ARR_DIMS(in)[0] != 4 + outState->len + outState->len * outState->len) 
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("final calculation function \"%s\" called with invalid parameters",
					format_procedure(fcinfo->flinfo->fn_oid))));
	
	outState->count  = data[1];         /* scalar:           count(*) */
	outState->sumy   = data[2];         /* scalar:           sum(y)   */
	outState->sumy2  = data[3];         /* scalar:           sum(y*y) */

	/* 
	 * The various matrix routines all take ArrayTypes as input, so we need to add the
	 * various array headers to our vector and matrix 
	 */
	lbs[0] = 1;
	lbs[1] = 1;
	
	dims[0] = outState->len;
	dims[1] = 1;
	outState->Xty = construct_md_array((Datum *) &data[4], NULL, 2, dims, lbs,
									   FLOAT8OID, sizeof(float8), true, 'd');
										/* vector[len]:      sum(X'[] * y) */
	
	dims[0] = 1;
	dims[1] = outState->len;
	outState->_Xty_t = construct_md_array((Datum *) &data[4], NULL, 2, dims, lbs,
										  FLOAT8OID, sizeof(float8), true, 'd');
										/* needed in calling functions */
	
	dims[0] = outState->len;
	dims[1] = outState->len;
	outState->XtX = construct_md_array((Datum *) &data[4 + outState->len], NULL, 2, dims, lbs,
									  FLOAT8OID, sizeof(float8), true, 'd');
										/* matrix[len][len]: sum(X[] * X'[]) */
	
	tempDatum = DirectFunctionCall1(pseudoinverse, PointerGetDatum(outState->XtX));
	outState->_XtX_inv = DatumGetArrayTypeP(tempDatum);
										/* needed in calling functions */
	
	return true;
}

/*
 * float8_mregr_compute()
 *
 * Compute regression coefficients, coefficient of determination (R^2),
 * t-statistics, and p-values whenever the respective argument is non-NULL.
 */

static void
float8_mregr_compute(MRegrState	*inState,
					 ArrayType	**outCoef,
					 float8		*outR2,
					 ArrayType	**outTStats,
					 ArrayType	**outPValues)
{
	Datum		temp_datum;
	ArrayType	*coef_array, *temp_array;
	float8		ssr, tss, ess, r2, variance;
	float8		*coef, *XtXinv, *tstats, *pvalues;
	int			i;
	
	/* 
	 * First, we compute the regression coefficients, often called b or beta in
	 * the literature.
	 * Vector of coefficients b is found via the formula:
	 *      b = (X'X)+ * X' * y = X+ * y
	 *
	 * The identity X+ = (X'X)+ * X' holds for all matrices X, a proof can be
	 * found here:
	 * http://en.wikipedia.org/wiki/Proofs_involving_the_Moore–Penrose_pseudoinverse
	 *
	 * Note that when the system X b = y is satisfiable (because X has rank at
	 * most inState->len), then setting b = X+ * y means that |b|_2 <=
	 * |c|_2 for all solutions c satisfying X c = y.
	 * (See http://en.wikipedia.org/wiki/Moore–Penrose_pseudoinverse)
	 *
	 * Explicitly computing (X'X)+ can become a significant source of numerical
	 * rounding erros (see, e.g., 
	 * http://en.wikipedia.org/wiki/Moore–Penrose_pseudoinverse#Construction
	 * or http://www.mathworks.com/moler/leastsquares.pdf p.16).
	 */
	
	temp_datum = DirectFunctionCall2(matrix_multiply, PointerGetDatum(inState->_XtX_inv),
									 PointerGetDatum(inState->Xty));
	coef_array = DatumGetArrayTypeP(temp_datum);

	if (outCoef)
		/* Note that coef_array is still a (1 x inState->len) matrix (= a
		 * two-dimensional array. We want to return a one-dimensional array. */
		*outCoef = construct_array((Datum *) ARR_DATA_PTR(coef_array),
								   inState->len, FLOAT8OID,
								   sizeof(float8), true, 'd');
	
	/* 
	 * Next, we compute the total sum of squares (tss) and the explained sumed
	 * of squares (ssr).
	 *     ssr = y'X * b - sum(y)^2/c
	 *     tss = sum(y^2) - sum(y)^2/c
     *     R^2 = ssr/tss
	 */
	temp_datum = DirectFunctionCall2(matrix_multiply, PointerGetDatum(inState->_Xty_t),
									 PointerGetDatum(coef_array));
	temp_array = DatumGetArrayTypeP(temp_datum);
	ssr = ((float8 *)ARR_DATA_PTR(temp_array))[0] - inState->sumy*inState->sumy/inState->count;
	tss = inState->sumy2 - inState->sumy * inState->sumy / inState->count;
	
	if (outR2)
	{
		r2  = ssr/tss;
		*outR2 = r2;
	}
	
	if (outTStats || outPValues)
	{
		/*
		 * Total sum of squares (tss) = Residual Sum of sqaures (ess) +
		 * Explained Sum of Squares (ssr) for linear regression.
		 * Proof: http://en.wikipedia.org/wiki/Sum_of_squares
		 */
		ess = tss - ssr;
		
		/* Variance is also called the Mean Square Error */
		variance = ess / (inState->count - inState->len);
		
		coef = (float8 *) ARR_DATA_PTR(coef_array);
		XtXinv = (float8 *) ARR_DATA_PTR(inState->_XtX_inv);
		
		/* The test statistics for each coef[i] is coef[i] / se(coef[i])
		 * where se(coef[i]) is the standard error of coef[i], i.e.,
		 * the square root of the i'th diagonoal element of variance * (X'X)^{-1} */
		tstats = (float8*) palloc(inState->len * sizeof(float8));
		for (i = 0; i < inState->len; i++)
			tstats[i] = coef[i] / sqrt( variance * XtXinv[i * (inState->len + 1)] );
	}
	
	if (outTStats)
		*outTStats = construct_array((Datum *) tstats, inState->len, FLOAT8OID,
									 sizeof(float8), true, 'd');
	
	if (outPValues)
	{
		pvalues = (float8*) palloc(inState->len * sizeof(float8));
		for (i = 0; i < inState->len; i++)
			pvalues[i] = 2. * (1. - studentT_cdf( inState->count - inState->len, fabs( tstats[i] ) ));
		
		*outPValues = construct_array((Datum *) pvalues, inState->len, FLOAT8OID,
									  sizeof(float8), true, 'd');
		
		/* we clean up anything we allocated ourselves */
		pfree(pvalues);
	}
	
	/* we clean up anything we allocated ourselves */
	if (outTStats || outPValues)
		pfree(tstats);
}


Datum float8_mregr_coef(PG_FUNCTION_ARGS)
{
	bool goodArguments;
	MRegrState state;
	ArrayType *coef;
	
	goodArguments = float8_mregr_get_state(fcinfo, &state);
	if (!goodArguments)
		PG_RETURN_NULL();
	
	float8_mregr_compute(&state, &coef /* coefficients */, NULL /* R2 */,
						 NULL /* t-statistics */, NULL /* p-values */);
	
	PG_RETURN_ARRAYTYPE_P(coef);
}


Datum float8_mregr_r2(PG_FUNCTION_ARGS)
{
	bool goodArguments;
	MRegrState state;
	float8 r2 = 0.0;
	
	goodArguments = float8_mregr_get_state(fcinfo, &state);
	if (!goodArguments)
		PG_RETURN_NULL();
	
	float8_mregr_compute(&state, NULL /* coefficients */, &r2,
						 NULL /* t-statistics */, NULL /* p-values */);
	
	PG_RETURN_FLOAT8(r2);
}


Datum float8_mregr_tstats(PG_FUNCTION_ARGS)
{
	bool goodArguments;
	MRegrState state;
	ArrayType *tstats;
	
	goodArguments = float8_mregr_get_state(fcinfo, &state);
	if (!goodArguments)
		PG_RETURN_NULL();
	
	float8_mregr_compute(&state, NULL /* coefficients */, NULL /* R2 */,
						 &tstats, NULL /* p-values */);
	
	PG_RETURN_ARRAYTYPE_P(tstats);
}


Datum float8_mregr_pvalues(PG_FUNCTION_ARGS)
{
	bool goodArguments;
	MRegrState state;
	ArrayType *pvalues;
	
	goodArguments = float8_mregr_get_state(fcinfo, &state);
	if (!goodArguments)
		PG_RETURN_NULL();
	
	float8_mregr_compute(&state, NULL /* coefficients */, NULL /* R2 */,
						 NULL /* t-statistics */, &pvalues);
	
	PG_RETURN_ARRAYTYPE_P(pvalues);
}


Datum float8_cg_update_accum(PG_FUNCTION_ARGS);
Datum float8_cg_update_final(PG_FUNCTION_ARGS);
Datum logreg_should_terminate(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(float8_cg_update_accum);
PG_FUNCTION_INFO_V1(float8_cg_update_final);
PG_FUNCTION_INFO_V1(logreg_should_terminate);

typedef struct {
	int32		iteration;	/* current iteration */
	int32		len;		/* number of coefficients */
	ArrayType	*coef;		/* coefficients */
	ArrayType	*dir;		/* direction */
	ArrayType	*grad;		/* gradient */
	float8		beta;		/* scale factor */

	int64		count;		/* number of rows */
	ArrayType	*gradNew;	/* intermediate value for gradient */
	float8		dTHd;		/* intermediate value for d^T * H * d */	
} LogRegrState;


static inline float8 sigma(float8 x)
{
	return 1 / (1 + exp(x));
}

static ArrayType *construct_uninitialized_array(int inNumElements,
	Oid inElementType, int inElementSize)
{
	int64		size = inElementSize * inNumElements + ARR_OVERHEAD_NONULLS(1);
	ArrayType	*array;
	void		*arrayData;
	
	array = (ArrayType *) palloc(size);
	SET_VARSIZE(array, size);
	array->ndim = 1;
	array->dataoffset = 0;
	array->elemtype = FLOAT8OID;
	ARR_DIMS(array)[0] = inNumElements;
	ARR_LBOUND(array)[0] = 1;
	arrayData = (float8 *) ARR_DATA_PTR(array);
	memset(arrayData, 0, inNumElements * inElementSize);
	return array;
}


/**
 * @internal There are aggregation states and iteration states: Aggregation
 * states contain the previous iteration state.
 * In the first iteration, we need to compute (only) the gradient.
 */

#define HeapTupleHeaderHasNulls(tuple) \
	((tuple->t_infomask & HEAP_HASNULL) != 0)
#define StartCopyFromTuple(tuple, nullArr) { \
	HeapTupleHeader __tuple = tuple; int __tupleItem = 0; bool *__nullArr = nullArr
#define CopyFromTuple(dest, DatumGet) \
	{ \
		Datum tempDatum = \
			GetAttributeByNum(__tuple, __tupleItem + 1, &__nullArr[__tupleItem]); \
		if (__nullArr[__tupleItem++] == false) \
			dest = DatumGet(tempDatum); \
		else \
			dest = 0; \
	}
#define CopyFloatArrayFromTuple(dest) \
	{ \
		Datum tempDatum = \
			GetAttributeByNum(__tuple, __tupleItem + 1, &__nullArr[__tupleItem]); \
		if (__nullArr[__tupleItem++] == false) { \
			ArrayType *array = DatumGetArrayTypeP(tempDatum); \
			dest = construct_array((Datum *) ARR_DATA_PTR(array), \
				outState->len, FLOAT8OID, sizeof(float8), true, 'd'); \
		} else \
			dest = 0; \
	}
#define EndCopyFromTuple }
static bool float8_cg_update_get_state(PG_FUNCTION_ARGS,
									   LogRegrState *outState)
{
	ArrayType		*newX;
	HeapTupleHeader	iterationStateTuple = PG_NARGS() == 1 || PG_ARGISNULL(1) ?
						NULL : PG_GETARG_HEAPTUPLEHEADER(1),
					aggregateStateTuple = PG_ARGISNULL(0) ?
						NULL : PG_GETARG_HEAPTUPLEHEADER(0);
	bool			isNull[9];

	memset(isNull, 0, sizeof(isNull));
	if (aggregateStateTuple == NULL) {
		/* This means: State transition function was called for first row */
	
		if (iterationStateTuple != NULL) {
			StartCopyFromTuple(iterationStateTuple, isNull);
			CopyFromTuple(outState->iteration, DatumGetInt32);
			CopyFromTuple(outState->len, DatumGetInt32);
			CopyFloatArrayFromTuple(outState->coef);
			CopyFloatArrayFromTuple(outState->dir);
			CopyFloatArrayFromTuple(outState->grad);
			CopyFromTuple(outState->beta, DatumGetFloat8);
			EndCopyFromTuple;
		}
		
		if (iterationStateTuple == NULL || outState->iteration == 0) {
			/* Note: In PL/pgSQL assinging a tuple variable NULL sets all
			 * components to NULL. However, the tuple itself is not NULL and
			 * iterationStateTuple would *not* be NULL. We therefore also
			 * have the test outState->iteration == 0. */
			 
			/* This means: We are in the first iteration. We need to initialize the state. */

			/* The length will only be set once: Here. */
			newX = PG_GETARG_ARRAYTYPE_P(3);
			outState->iteration = 0;
			outState->len = ARR_DIMS(newX)[0];		
			outState->coef = construct_uninitialized_array(outState->len, FLOAT8OID, 8);
			outState->dir = construct_uninitialized_array(outState->len, FLOAT8OID, 8);
			outState->grad = construct_uninitialized_array(outState->len, FLOAT8OID, 8);
			outState->beta = 0;
			
			isNull[0] = isNull[1] = isNull[2] = isNull[3] = isNull[4] = isNull[5] = false;
		}

		outState->count = 0;
		outState->gradNew = construct_uninitialized_array(outState->len, FLOAT8OID, 8);
		outState->dTHd = 0.;
	} else {
		StartCopyFromTuple(aggregateStateTuple, isNull);
		CopyFromTuple(outState->iteration, DatumGetInt32);
		CopyFromTuple(outState->len, DatumGetInt32);
		CopyFromTuple(outState->coef, DatumGetArrayTypeP);
		CopyFromTuple(outState->dir, DatumGetArrayTypeP);
		CopyFromTuple(outState->grad, DatumGetArrayTypeP);
		CopyFromTuple(outState->beta, DatumGetFloat8);
		
		CopyFromTuple(outState->count, DatumGetInt64);
		CopyFromTuple(outState->gradNew, DatumGetArrayTypeP);
		CopyFromTuple(outState->dTHd, DatumGetFloat8);
		EndCopyFromTuple;
		
//		if (outState->iteration == 0) {
//			isNull[3] = isNull[4] = false;
//		}
	}
			
	for (int i = 0; i < 9; i++)
		if (isNull[i]) return false;

	return true;
}
#undef StartCopyFromTuple
#undef CopyFromTuple

Datum float8_cg_update_accum(PG_FUNCTION_ARGS)
{
	bool			goodArguments;
	TypeFuncClass	funcClass;
	Oid				resultType;
	TupleDesc		resultDesc;
	Datum			resultDatum[9];
	bool			resultNull[9];
	HeapTuple		result;
	
	LogRegrState	state;
	ArrayType		*newX;
	int32			newY;
	float8			*newXData;
	float8			wTx, dTx;
	
	/* Input should be 4 parameters */
	if (PG_NARGS() != 4)
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("transition function \"%s\" called with invalid parameters",
					format_procedure(fcinfo->flinfo->fn_oid))));

	for (int i = 2; i < 4; i++) {
		if (PG_ARGISNULL(i)) {
			PG_RETURN_DATUM(PG_GETARG_DATUM(0));
		}
	}

	/* Only callable as a transition function */
	if (!(fcinfo->context && IsA(fcinfo->context, AggState)))
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("transition function \"%s\" not called from aggregate",
					format_procedure(fcinfo->flinfo->fn_oid))));
	
	newY  = PG_GETARG_BOOL(2) ? 1 : -1;
    newX  = PG_GETARG_ARRAYTYPE_P(3);
	newXData = (float8 *) ARR_DATA_PTR(newX);
		
	/* Ensure that all arrays are single dimensional float8[] arrays without NULLs */
	if (ARR_NULLBITMAP(newX) || ARR_NDIM(newX) != 1 || ARR_ELEMTYPE(newX) != FLOAT8OID)
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("transition function \"%s\" called with invalid parameters",
					format_procedure(fcinfo->flinfo->fn_oid))));

	goodArguments = float8_cg_update_get_state(fcinfo, &state);
	if (!goodArguments)
		PG_RETURN_NULL();

	/* Something is seriously fishy if our state has the wrong form */
	if (state.len != ARR_DIMS(newX)[0] ||
		ARR_DIMS(state.coef)[0] != state.len ||
		(state.dir && ARR_DIMS(state.dir)[0] != state.len) ||
		(state.grad && ARR_DIMS(state.grad)[0] != state.len) ||
		ARR_DIMS(state.gradNew)[0] != state.len)
	{
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("transition function \"%s\" called with invalid parameters",
					format_procedure(fcinfo->flinfo->fn_oid))));
	}
	
	/* Okay... All's good now do the work */
	state.count++;

	wTx = 0.;
	dTx = 0.;
	
	if (state.iteration > 0) {
		/* if state.iteration = 0 then wTx and dTx will remain 0 anyway. */
	
		for (int i = 0; i < state.len; i++) {
			wTx += newXData[i] * ((float8 *) ARR_DATA_PTR(state.coef))[i];
			dTx += newXData[i] * ((float8 *) ARR_DATA_PTR(state.dir))[i];
		}
	}
	
	if (state.iteration % 2 == 0) {		
		for (int i = 0; i < state.len; i++) {
			((float8*) ARR_DATA_PTR(state.gradNew))[i] +=
				(1 - sigma(newY * wTx)) * newY * newXData[i];
		}
	} else {
		state.dTHd += sigma(wTx) * (1 - sigma(wTx)) * dTx * dTx;
	}
	
	/* Construct the return tuple */
	funcClass = get_call_result_type(fcinfo, &resultType, &resultDesc);
	BlessTupleDesc(resultDesc);
	
	resultDatum[0] = Int32GetDatum(state.iteration);
	resultDatum[1] = Int32GetDatum(state.len);
	resultDatum[2] = PointerGetDatum(state.coef);
	resultDatum[3] = PointerGetDatum(state.dir);
	resultDatum[4] = PointerGetDatum(state.grad);
	resultDatum[5] = Float8GetDatum(state.beta);
	
	resultDatum[6] = Int64GetDatum(state.count);
	resultDatum[7] = PointerGetDatum(state.gradNew);
	resultDatum[8] = Float8GetDatum(state.dTHd);
	memset(resultNull, 0, sizeof(resultNull));
	resultNull[3] = state.dir == NULL;
	resultNull[4] = state.grad == NULL;
	
	result = heap_form_tuple(resultDesc, resultDatum, resultNull);
	PG_RETURN_DATUM(HeapTupleGetDatum(result));
}

static inline float8 float8_dotProduct(ArrayType *inVec1, ArrayType *inVec2)
{
	float8	returnValue = 0.;
	float8	*arr1, *arr2;
	int		size;
	
	if (ARR_ELEMTYPE(inVec1) != FLOAT8OID || ARR_ELEMTYPE(inVec2) != FLOAT8OID ||
		ARR_NDIM(inVec1) != 1 || ARR_NDIM(inVec2) != 1 ||
		ARR_DIMS(inVec1)[0] != ARR_DIMS(inVec2)[0])
	{
		ereport(ERROR, 
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 errmsg("internal function float8_dotProduct called with invalid parameters")));
	}
	
	size = ARR_DIMS(inVec1)[0];
	arr1 = (float8 *) ARR_DATA_PTR(inVec1);
	arr2 = (float8 *) ARR_DATA_PTR(inVec2);
	for (int i = 0; i < ARR_DIMS(inVec1)[0]; i++)
		returnValue += arr1[i] * arr2[i];
	
	return returnValue;
}

static inline ArrayType *float8_vectorMinus(ArrayType *inVec1, ArrayType *inVec2)
{
	float8		*arr1, *arr2, *returnData;
	ArrayType	*returnVec;
	int			size;
	
	if (ARR_ELEMTYPE(inVec1) != FLOAT8OID || ARR_ELEMTYPE(inVec2) != FLOAT8OID ||
		ARR_NDIM(inVec1) != 1 || ARR_NDIM(inVec2) != 1 ||
		ARR_DIMS(inVec1)[0] != ARR_DIMS(inVec2)[0])
	{
		ereport(ERROR, 
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 errmsg("internal function float8_vectorMinus called with invalid parameters")));
	}
	
	size = ARR_DIMS(inVec1)[0];
	arr1 = (float8 *) ARR_DATA_PTR(inVec1);
	arr2 = (float8 *) ARR_DATA_PTR(inVec2);
	returnVec = construct_uninitialized_array(size, FLOAT8OID, 8);
	returnData = (float8 *) ARR_DATA_PTR(returnVec);
	
	for (int i = 0; i < size; i++)
		returnData[i] = arr1[i] - arr2[i];
	
	return returnVec;
}

Datum float8_cg_update_final(PG_FUNCTION_ARGS)
{
	bool			goodArguments;
	TypeFuncClass	funcClass;
	Oid				resultType;
	TupleDesc		resultDesc;
	Datum			resultDatum[9];
	bool			resultNull[9];
	HeapTuple		result;

	LogRegrState	state;
	ArrayType		*gradMinusGradOld;
	float8			alpha;
	ArrayType		*betaDir;

	if (PG_NARGS() != 1)
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("final calculation function \"%s\" called with invalid parameters",
					format_procedure(fcinfo->flinfo->fn_oid))));

	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();

	goodArguments = float8_cg_update_get_state(fcinfo, &state);
	if (!goodArguments)
		PG_RETURN_NULL();
	
	
	// k = iteration / 2
	if (state.iteration == 0) {
		// Iteration computes the gradient
	
		state.grad = state.gradNew;
		state.dir = state.gradNew;
	} else if (state.iteration % 2 == 0) {
		// Even iterations compute the gradient (during the accumulation phase)
		// and the new direction (during the final phase).  Note that
		// state.gradNew != state.grad starting from iteration 2
		
		//            g_k^T (g_k - g_{k-1})
		// beta_k = -------------------------
		//          d_{k-1}^T (g_k - g_{k-1})
		gradMinusGradOld = float8_vectorMinus(state.gradNew, state.grad);
		state.beta = float8_dotProduct(state.gradNew, gradMinusGradOld) /
			float8_dotProduct(state.dir, gradMinusGradOld);
		pfree(gradMinusGradOld);

		// d_k = g_k - beta_k * d_{k-1}
		betaDir = DatumGetArrayTypeP(
			DirectFunctionCall2(float8_matrix_smultiply,
				PointerGetDatum(state.dir), Float8GetDatum(state.beta)));
		state.dir = float8_vectorMinus(state.gradNew, betaDir);
		
		state.grad = state.gradNew;
	} else {
		// Odd iteration compute -d^T H d (during the accumulation phase) and
		// and the new coefficients (during the final phase).

		//              g_k^T d_k
		// alpha_k = - -----------
		//             d_k^T H d_k
		alpha = float8_dotProduct(state.grad, state.dir) /
			state.dTHd;
		
		// c_k = c_{k-1} - alpha_k * d_k
		state.coef = DatumGetArrayTypeP(
			DirectFunctionCall2(matrix_add, PointerGetDatum(state.coef),
			
			// alpha_k * d_k
			DirectFunctionCall2(float8_matrix_smultiply,
				PointerGetDatum(state.dir), Float8GetDatum(-alpha))));
	}

	/* Construct the return tuple */
	funcClass = get_call_result_type(fcinfo, &resultType, &resultDesc);
	BlessTupleDesc(resultDesc);
	
	resultDatum[0] = Int32GetDatum(state.iteration + 1);
	resultDatum[1] = Int32GetDatum(state.len);
	resultDatum[2] = PointerGetDatum(state.coef);
	resultDatum[3] = PointerGetDatum(state.dir);
	resultDatum[4] = PointerGetDatum(state.grad);
	resultDatum[5] = Float8GetDatum(state.beta);
	
	resultDatum[6] = Int64GetDatum(state.count);
	resultDatum[7] = PointerGetDatum(state.gradNew);
	resultDatum[8] = Float8GetDatum(state.dTHd);
	
	resultNull[0] = resultNull[1] = resultNull[2] = resultNull[3] = resultNull[4] = resultNull[5] = false;
	resultNull[6] = resultNull[7] = resultNull[8] = true;
	
	result = heap_form_tuple(resultDesc, resultDatum, resultNull);
	PG_RETURN_DATUM(HeapTupleGetDatum(result));	
}


Datum logreg_should_terminate(PG_FUNCTION_ARGS)
{
	ArrayType	*oldCoef;
	ArrayType	*newCoef;	
	float8		precision;
	ArrayType	*lastChange;
	float8		l2LastChange;
	
	if (PG_ARGISNULL(0) || PG_ARGISNULL(1) || PG_ARGISNULL(3))
		ereport(ERROR, 
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 errmsg("termination check for logistic regression \"%s\" called with invalid parameters",
				format_procedure(fcinfo->flinfo->fn_oid))));
	
	oldCoef = PG_GETARG_ARRAYTYPE_P(0);
	newCoef = PG_GETARG_ARRAYTYPE_P(1);
	precision = PG_GETARG_FLOAT8(3);
	
	lastChange = float8_vectorMinus(oldCoef, newCoef);
	l2LastChange = float8_dotProduct(lastChange, lastChange);
	
	if (l2LastChange <= precision * precision)
		PG_RETURN_DATUM(BoolGetDatum(true));
	
	PG_RETURN_DATUM(BoolGetDatum(false));
}
