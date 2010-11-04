#include "regress.h"
#include "pinv.h"
#include "matrix.h"
#include "cephes/cephes.h"

#include "funcapi.h"
#include "catalog/pg_type.h"
#include "utils/array.h"

#include <math.h>
#include <string.h>


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


PG_FUNCTION_INFO_V1(float8_mregr_accum);

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
		ARR_ELEMTYPE(state) != FLOAT8OID)
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("invalid transition state")));
	if (ARR_NDIM(newX) != 1 || ARR_ELEMTYPE(newX) != FLOAT8OID)
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("data array must be a single dimensional float8[]")));
	
	/* Only callable as a transition function */
	if (!(fcinfo->context && IsA(fcinfo->context, AggState)))
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("float8_mregr_accum called from invalid context")));
	
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
				 errmsg("independent variable array not constant width")));
	}
	
	/* Something is seriously fishy if our state has the wrong length */
	if (ARR_DIMS(state)[0] != statelen)
	{
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("float8_mregr_accum malformed state value")));
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


PG_FUNCTION_INFO_V1(float8_mregr_combine);

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
				 errmsg("invalid transition state")));
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
				 errmsg("independent variable array not constant width")));
	}
	len = state1Data[0];
	statelen = 4 + len + len*len;
	if (ARR_DIMS(state1)[0] != statelen)
	{
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("mregr_combine: invalid state")));
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

PG_FUNCTION_INFO_V1(float8_mregr_get_state);

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
				 errmsg("float8_mregr_* called with %d arguments", 
						PG_NARGS())));
	
	if (PG_ARGISNULL(0))
		return false;
	
	/* Validate array type */
	in = PG_GETARG_ARRAYTYPE_P(0);
	if (ARR_ELEMTYPE(in) != FLOAT8OID || ARR_NDIM(in) != 1 || ARR_NULLBITMAP(in))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("float8_mregr_* called on invalid input array")));
	
	/* Validate the correct size input */
	if (ARR_DIMS(in)[0] < 2)
		return false;  /* no input */
	
	data = (float8*) ARR_DATA_PTR(in);
	outState->len    = (int) data[0];   /* scalar:           len(X[]) */
	if (ARR_DIMS(in)[0] != 4 + outState->len + outState->len * outState->len) 
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("float8_mregr_* called on invalid input array")));
	
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
	 *      b = (X'X)^-1 * X' * y
	 */
	
	temp_datum = DirectFunctionCall2(matrix_multiply, PointerGetDatum(inState->_XtX_inv),
									 PointerGetDatum(inState->Xty));
	coef_array = DatumGetArrayTypeP(temp_datum);

	if (outCoef)
		*outCoef = coef_array;
	
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
			pvalues[i] = 2. * (1. - stdtr( inState->count - inState->len, fabs( tstats[i] ) ));
		
		*outPValues = construct_array((Datum *) pvalues, inState->len, FLOAT8OID,
									  sizeof(float8), true, 'd');
		
		/* we clean up anything we allocated ourselves */
		pfree(pvalues);
	}
	
	/* we clean up anything we allocated ourselves */
	if (outTStats || outPValues)
		pfree(tstats);
}


PG_FUNCTION_INFO_V1(float8_mregr_coef);

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


PG_FUNCTION_INFO_V1(float8_mregr_r2);

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


PG_FUNCTION_INFO_V1(float8_mregr_tstats);

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


PG_FUNCTION_INFO_V1(float8_mregr_pvalues);

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

