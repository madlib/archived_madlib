/* ----------------------------------------------------------------------- *//** 
 *
 * @file regress.c
 *
 * @brief Multi-linear and logistic regression
 * @author Florian Schoppmann
 *
 *//* ----------------------------------------------------------------------- */

#include "postgres.h"

#include "regress.h"
#include "pinv.h"
#include "matrix.h"
#include "student.h"

#include "funcapi.h"
#include "catalog/pg_type.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "executor/executor.h" // Greenplum requires this for GetAttributeByNum()

#include <math.h>
#include <string.h>


/* Indicate "version 1" calling conventions for all exported functions. */

PG_FUNCTION_INFO_V1(float8_mregr_accum);
PG_FUNCTION_INFO_V1(float8_mregr_combine);
PG_FUNCTION_INFO_V1(float8_mregr_coef);
PG_FUNCTION_INFO_V1(float8_mregr_r2);
PG_FUNCTION_INFO_V1(float8_mregr_tstats);
PG_FUNCTION_INFO_V1(float8_mregr_pvalues);


/**
 * @internal
 * @brief Transition state for multi-linear regression functions.
 */
typedef struct {
    ArrayType   *stateAsArray;
    float8      *len;
    float8      *count;
    float8      *sumy;
    float8      *sumy2;
    float8      *Xty;
    float8      *XtX;

    ArrayType   *newX;
    float8      *newXData;
} MRegrAccumState;

/**
 * @internal
 * @brief Final state for multi-linear regression functions.
 */
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

static bool float8_mregr_accum_get_state(PG_FUNCTION_ARGS,
                                         MRegrAccumState *outState);
static bool float8_mregr_get_state(PG_FUNCTION_ARGS,
								   MRegrState *outState);
static void float8_mregr_compute(MRegrState	*inState,
								 ArrayType	**outCoef,
								 float8		*outR2,
								 ArrayType	**outTStats,
								 ArrayType	**outPValues);

#ifndef NO_PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif


static bool
float8_mregr_accum_get_state(PG_FUNCTION_ARGS,
                             MRegrAccumState *outState)
{
    float8      *stateData;
	int         len, statelen;	
	
	/* We should be strict, but it doesn't hurt to be paranoid */
	if (PG_ARGISNULL(0) || PG_ARGISNULL(1) || PG_ARGISNULL(2))
        return false;
	
	outState->stateAsArray = PG_GETARG_ARRAYTYPE_P(0);	
    outState->newX = PG_GETARG_ARRAYTYPE_P(2);
	
	/* Ensure that both arrays are single dimensional float8[] arrays */
	if (ARR_NULLBITMAP(outState->stateAsArray) ||
        ARR_NDIM(outState->stateAsArray) != 1 || 
		ARR_ELEMTYPE(outState->stateAsArray) != FLOAT8OID ||
		ARR_NDIM(outState->newX) != 1 ||
        ARR_ELEMTYPE(outState->newX) != FLOAT8OID)
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
	if (ARR_NULLBITMAP(outState->newX))
		return false;
	
	/*
	 * If length(state) == 1 then it is an unitialized state, extend as
	 * needed, we use this instead of NULL so that we can declare the
	 * function as strict.
	 */
	len = ARR_DIMS(outState->newX)[0];
	statelen = 4 + len + len*len;
	if (ARR_DIMS(outState->stateAsArray)[0] == 1)
	{
		int size = statelen * sizeof(float8) + ARR_OVERHEAD_NONULLS(1);
		outState->stateAsArray = (ArrayType *) palloc(size);
		SET_VARSIZE(outState->stateAsArray, size);
		outState->stateAsArray->ndim = 1;
		outState->stateAsArray->dataoffset = 0;
		outState->stateAsArray->elemtype = FLOAT8OID;
		ARR_DIMS(outState->stateAsArray)[0] = statelen;
		ARR_LBOUND(outState->stateAsArray)[0] = 1;
		stateData = (float8*) ARR_DATA_PTR(outState->stateAsArray);
		memset(stateData, 0, statelen * sizeof(float8));
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
	outState->len = (float8*) ARR_DATA_PTR(outState->stateAsArray);
    outState->count = outState->len + 1;
    outState->sumy = outState->len + 2;
    outState->sumy2 = outState->len + 3;
    outState->Xty = outState->len + 4;
    outState->XtX = outState->len + 4 + len;

	outState->newXData  = (float8*) ARR_DATA_PTR(outState->newX);
	
	/* It is an error if the number of indepent variables is not constant */
	if (*outState->len != len)
	{
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("transition function \"%s\" called with invalid parameters",
					format_procedure(fcinfo->flinfo->fn_oid)),
				 errdetail("The independent-variable array is not of constant width.")));
	}
	
	/* Something is seriously fishy if our state has the wrong length */
	if (ARR_DIMS(outState->stateAsArray)[0] != statelen)
	{
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("transition function \"%s\" called with invalid parameters",
					format_procedure(fcinfo->flinfo->fn_oid))));
	}
    
	/* Okay... All's good now do the work */
    return true;    
}


/**
 * Transition function used by multi-linear regression aggregates.
 */
Datum
float8_mregr_accum(PG_FUNCTION_ARGS)
{
    bool            goodArguments;
    MRegrAccumState state;
    float8          newY;
	int             len, i,j;

	goodArguments = float8_mregr_accum_get_state(fcinfo, &state);
	if (!goodArguments) {
        if (PG_ARGISNULL(0))
			PG_RETURN_NULL();
        else
            PG_RETURN_ARRAYTYPE_P(PG_GETARG_ARRAYTYPE_P(0));
	}
   	newY = PG_GETARG_FLOAT8(1);

    len = (int) *state.len;
	(*state.count)++;
	*state.sumy += newY;
	*state.sumy2 += newY * newY;
	for (i = 0; i < len; i++)
		state.Xty[i] += newY * state.newXData[i];
	
	/* Compute the matrix X[] * X'[] and add it in */
	for (i = 0; i < len; i++)
		for (j = 0; j < len; j++)
			state.XtX[i*len + j] += state.newXData[i] * state.newXData[j];
	
	PG_RETURN_ARRAYTYPE_P(state.stateAsArray);
}


/**
 * Preliminary segment-level calculation function for multi-linear regression
 * aggregates.
 */
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
	len = (int) state1Data[0];
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


/**
 * Check that a valid state is passed to the aggregate's final function.
 */
static bool
float8_mregr_get_state(PG_FUNCTION_ARGS,
					   MRegrState *outState)
{
    /* PG_FUNCTION_ARGS expands to "FunctionCallInfo fcinfo". If we return
     * false, the calling function should return NULL.
     */
    
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

/**
 * Do the computations requested from final functions.
 *
 * Compute regression coefficients, coefficient of determination (R^2),
 * t-statistics, and p-values whenever the respective argument is non-NULL.
 * Since these functions share a lot of computation, they have been distilled
 * into this function.
 *
 * First, we compute the regression coefficients, often called b or beta in
 * the literature.
 * Vector of coefficients c is found via the formula:
   @verbatim
   
     c = (X^T X)+ * X^T * y = X+ * y

   where:
   
     X' = the transpose of X
     X+ = the pseudo-inverse of X
   @endverbatim
 * The identity \f$ X^+ = (X^T X)^+ X^T \f$ holds for all matrices $X$, a proof
 * can be found here:
 * http://en.wikipedia.org/wiki/Proofs_involving_the_Moore%2DPenrose_pseudoinverse
 *
 * Note that when the system \f$ X \boldsymbol c = y \f$ is satisfiable (because
 * \f$ (X|\boldsymbol c) \f$ has rank at most <tt>inState->len</tt>), then
 * setting \f$ \boldsymbol c = X^+ y \f$ means that
 * \f$ |\boldsymbol c|_2 \le |\boldsymbol d|_2 \f$ for all solutions
 * \f$ \boldsymbol d \f$ satisfying \f$ X \boldsymbol c = y \f$.
 * (See http://en.wikipedia.org/wiki/Moore%2DPenrose_pseudoinverse)
 *
 * Explicitly computing $(X^T X)^+$ can become a significant source of numerical
 * rounding erros (see, e.g., 
 * http://en.wikipedia.org/wiki/Moore%2DPenrose_pseudoinverse#Construction
 * or http://www.mathworks.com/moler/leastsquares.pdf p.16).
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
	float8		ssr = 0., tss = 0., ess, r2, variance;
	float8		*coef, *XtXinv, *tstats = NULL, *pvalues = NULL;
	int			i;
	
	temp_datum = DirectFunctionCall2(matrix_multiply, PointerGetDatum(inState->_XtX_inv),
									 PointerGetDatum(inState->Xty));
	coef_array = DatumGetArrayTypeP(temp_datum);

	if (outCoef)
		/* Note that coef_array is still a (1 x inState->len) matrix (= a
		 * two-dimensional array. We want to return a one-dimensional array. */
		*outCoef = construct_array((Datum *) ARR_DATA_PTR(coef_array),
								   inState->len, FLOAT8OID,
								   sizeof(float8), true, 'd');
	
    if (outR2 || outTStats || outPValues)
    {    
        /**
         * \par Computing the total sum of squares (tss) and the explained sum of squares (ssr)
         *
           @verbatim
           
             ssr = y'X * c - sum(y)^2/n
             tss = sum(y^2) - sum(y)^2/n
             R^2 = ssr/tss
           @endverbatim
         */
        temp_datum = DirectFunctionCall2(matrix_multiply, PointerGetDatum(inState->_Xty_t),
                                         PointerGetDatum(coef_array));
        temp_array = DatumGetArrayTypeP(temp_datum);
        ssr = ((float8 *)ARR_DATA_PTR(temp_array))[0] - inState->sumy*inState->sumy/inState->count;
        tss = inState->sumy2 - inState->sumy * inState->sumy / inState->count;
    }
	
	if (outR2)
	{
		r2  = ssr/tss;
		*outR2 = r2;
	}
	
	if (outTStats || outPValues)
	{
		/**
         * \par Computing t-statistics and p-values
         *
		 * Total sum of squares (tss) = Residual Sum of sqaures (ess) +
		 * Explained Sum of Squares (ssr) for linear regression.
		 * Proof: http://en.wikipedia.org/wiki/Sum_of_squares
		 */
		ess = tss - ssr;
		
		/* Variance is also called the Mean Square Error */
		variance = ess / (inState->count - inState->len);
		
		coef = (float8 *) ARR_DATA_PTR(coef_array);
		XtXinv = (float8 *) ARR_DATA_PTR(inState->_XtX_inv);
		
		/**
         * The t-statistic for each $c_i$ is $c_i / se(c_i)$
		 * where $se(c_i)$ is the standard error of $c_i$, i.e.,
		 * the square root of the i'th diagonoal element of
         * \f$ \mathit{variance} * (X^T X)^{-1} \f$
         */
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
			pvalues[i] = 2. * (1. - studentT_cdf(
                    (int) (inState->count - inState->len), fabs( tstats[i] )
                ));
		
		*outPValues = construct_array((Datum *) pvalues, inState->len, FLOAT8OID,
									  sizeof(float8), true, 'd');
		
		/* we clean up anything we allocated ourselves */
		pfree(pvalues);
	}
	
	/* we clean up anything we allocated ourselves */
	if (outTStats || outPValues)
		pfree(tstats);
}


/**
 * PostgreSQL final function for computing regression coefficients.
 *
 * This function is essentially a wrapper for float8_mregr_compute().
 */
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

/**
 * PostgreSQL final function for computing the coefficient of determination, $R^2$.
 *
 * This function is essentially a wrapper for float8_mregr_compute().
 */
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

/**
 * PostgreSQL final function for computing the vector of t-statistics, for every coefficient.
 *
 * This function is essentially a wrapper for float8_mregr_compute().
 */
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

/**
 * PostgreSQL final function for computing the vector of p-values, for every coefficient.
 *
 * This function is essentially a wrapper for float8_mregr_compute().
 */
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
Datum float8_cg_update_combine(PG_FUNCTION_ARGS);
Datum float8_irls_update_accum(PG_FUNCTION_ARGS);
Datum float8_cg_update_final(PG_FUNCTION_ARGS);
Datum float8_irls_update_final(PG_FUNCTION_ARGS);
Datum logregr_should_terminate(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(float8_cg_update_accum);
PG_FUNCTION_INFO_V1(float8_cg_update_combine);
PG_FUNCTION_INFO_V1(float8_irls_update_accum);
PG_FUNCTION_INFO_V1(float8_cg_update_final);
PG_FUNCTION_INFO_V1(float8_irls_update_final);
PG_FUNCTION_INFO_V1(logregr_should_terminate);

/**
 * \internal 
 * Note the order of computation:
 * 
 * FIXME: Several assumptions about this struct are hard coded (e.g., number of
 * elements)
 * \endinternal 
 */
typedef struct {
	int32		iteration;	/* current iteration */
	int32		len;		/* number of coefficients */
	ArrayType	*coef;		/* vector of coefficients c */
	ArrayType	*dir;		/* direction */
	ArrayType	*grad;		/* gradient */
	float8		beta;		/* scale factor */

	int64		count;		/* number of rows */
	ArrayType	*gradNew;	/* intermediate value for gradient */
	float8		dTHd;		/* intermediate value for d^T * H * d */
    float8      logLikelihood; /* ln(l(c))  */
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


#define HeapTupleHeaderHasNulls(tuple) \
	((tuple->t_infomask & HEAP_HASNULL) != 0)
#define StartCopyFromTuple(tuple, nullArr) { \
	HeapTupleHeader __tuple = tuple; int __tupleItem = 0; bool *__nullArr = nullArr
#define CopyFromTuple(dest, DatumGet) \
	do { \
		Datum tempDatum = \
			GetAttributeByNum(__tuple, __tupleItem + 1, &__nullArr[__tupleItem]); \
		if (__nullArr[__tupleItem++] == false) \
			dest = DatumGet(tempDatum); \
		else \
			dest = 0; \
	} while (0)
#define CopyFloatArrayFromTuple(dest) \
	do { \
		Datum tempDatum = \
			GetAttributeByNum(__tuple, __tupleItem + 1, &__nullArr[__tupleItem]); \
		if (__nullArr[__tupleItem++] == false) { \
			ArrayType *array = DatumGetArrayTypeP(tempDatum); \
			dest = construct_array((Datum *) ARR_DATA_PTR(array), \
				outState->len, FLOAT8OID, sizeof(float8), true, 'd'); \
		} else \
			dest = 0; \
	} while(0)
#define EndCopyFromTuple }
static void copy_tuple_to_logregr_state(HeapTupleHeader inTuple,
                                       LogRegrState *outState,
                                       bool *outIsNull,
                                       bool copyIntraIterationState)
{
	StartCopyFromTuple(inTuple, outIsNull);
    /* copy inter-iteration state information*/
	CopyFromTuple(outState->iteration, DatumGetInt32);
	CopyFromTuple(outState->len, DatumGetInt32);
	CopyFloatArrayFromTuple(outState->coef);
	CopyFloatArrayFromTuple(outState->dir);
	CopyFloatArrayFromTuple(outState->grad);
	CopyFromTuple(outState->beta, DatumGetFloat8);
    if (copyIntraIterationState) {
        /* copy also intra-iteration (single aggregate) state information */
		CopyFromTuple(outState->count, DatumGetInt64);
		CopyFromTuple(outState->gradNew, DatumGetArrayTypeP);
		CopyFromTuple(outState->dTHd, DatumGetFloat8);
        CopyFromTuple(outState->logLikelihood, DatumGetFloat8);
    }
	EndCopyFromTuple;    
}
#undef StartCopyFromTuple
#undef CopyFromTuple

#define StartCopyToHeapTuple(datumArr, nullArr) { \
	Datum *__datumArr = datumArr; int __tupleItem = 0; bool *__nullArr = nullArr
#define CopyToHeapTuple(src, GetDatum) \
    do { \
        if (__nullArr[__tupleItem] == false) \
            __datumArr[__tupleItem++] = GetDatum(src); \
        else \
            __datumArr[__tupleItem++] = PointerGetDatum(NULL); \
    } while (0)
#define EndCopyToDatum }
static HeapTuple logregr_state_to_heap_tuple(LogRegrState *inState,
                                            bool *inIsNull,
                                            PG_FUNCTION_ARGS)
{
	TypeFuncClass	funcClass;
	Oid				resultType;
	TupleDesc		resultDesc;
	Datum			resultDatum[10];

	funcClass = get_call_result_type(fcinfo, &resultType, &resultDesc);
	BlessTupleDesc(resultDesc);

    StartCopyToHeapTuple(resultDatum, inIsNull);
    CopyToHeapTuple(inState->iteration, Int32GetDatum);
    CopyToHeapTuple(inState->len, Int32GetDatum);
    CopyToHeapTuple(inState->coef, PointerGetDatum);
    CopyToHeapTuple(inState->dir, PointerGetDatum);
    CopyToHeapTuple(inState->grad, PointerGetDatum);
    CopyToHeapTuple(inState->beta, Float8GetDatum);
    
    CopyToHeapTuple(inState->count, Int64GetDatum);
    CopyToHeapTuple(inState->gradNew, PointerGetDatum);
    CopyToHeapTuple(inState->dTHd, Float8GetDatum);
    CopyToHeapTuple(inState->logLikelihood, Float8GetDatum);
    EndCopyToDatum;

	return heap_form_tuple(resultDesc, resultDatum, inIsNull);
}
#undef StartCopyToHeapTuple
#undef EndCopyToDatum

/**
 * @internal There are aggregation states and iteration states: Aggregation
 * states contain the previous iteration state.
 * In the first iteration, we need to compute (only) the gradient.
 */

static bool float8_cg_update_get_state(PG_FUNCTION_ARGS,
									   LogRegrState *outState)
{
	ArrayType		*newX;
	HeapTupleHeader	iterationStateTuple = PG_NARGS() < 3 || PG_ARGISNULL(3) ?
						NULL : PG_GETARG_HEAPTUPLEHEADER(3),
					aggregateStateTuple = PG_ARGISNULL(0) ?
						NULL : PG_GETARG_HEAPTUPLEHEADER(0);
	bool			isNull[10];

	memset(isNull, 0, sizeof(isNull));
	if (aggregateStateTuple == NULL) {
		/* This means: State transition function was called for first row */
	
		if (iterationStateTuple != NULL)
            copy_tuple_to_logregr_state(iterationStateTuple, outState,
                isNull, /* copyIntraIterationState */ false);
		
		if (iterationStateTuple == NULL || outState->iteration == 0) {
			/* Note: In PL/pgSQL assinging a tuple variable NULL sets all
			 * components to NULL. However, the tuple itself is not NULL and
			 * iterationStateTuple would *not* be NULL. We therefore also
			 * have the test outState->iteration == 0. */
			 
			/* This means: We are in the first iteration. We need to initialize the state. */

			/* The length will only be set once: Here. */
			newX = PG_GETARG_ARRAYTYPE_P(2);
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
        outState->logLikelihood = 0.;
	} else {
        copy_tuple_to_logregr_state(aggregateStateTuple, outState, isNull,
            /* copyIntraIterationState */ true);
	}
			
	for (int i = 0; i < 9; i++)
		if (isNull[i]) return false;

	return true;
}

Datum float8_cg_update_accum(PG_FUNCTION_ARGS)
{
	bool			goodArguments;
	bool			resultNull[10];
	HeapTuple		result;
	
	LogRegrState	state;
	ArrayType		*newX;
	int32			newY;
	float8			*newXData;
	float8			cTx, dTx;
	
	/* Input should be 4 parameters */
	if (PG_NARGS() != 4)
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("transition function \"%s\" called with invalid parameters",
					format_procedure(fcinfo->flinfo->fn_oid))));

    /* If dependent or inependent variables are null, ignore this row */
	for (int i = 1; i <= 2; i++) {
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
	
	newY  = PG_GETARG_BOOL(1) ? 1 : -1;
    newX  = PG_GETARG_ARRAYTYPE_P(2);
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

	cTx = 0.;
	dTx = 0.;
	
	if (state.iteration > 0) {
		/* if state.iteration = 0 then cTx and dTx will remain 0 anyway. */
	
		for (int i = 0; i < state.len; i++) {
			cTx += newXData[i] * ((float8 *) ARR_DATA_PTR(state.coef))[i];
			dTx += newXData[i] * ((float8 *) ARR_DATA_PTR(state.dir))[i];
		}
	}
	
    // FIXME: y has different signs than in Minka (2003). Where is the bug?
	if (state.iteration % 2 == 0) {		
		for (int i = 0; i < state.len; i++) {
			((float8*) ARR_DATA_PTR(state.gradNew))[i] -=
				sigma(newY * cTx) * newY * newXData[i];
		}
	} else {
		state.dTHd += sigma(cTx) * (1 - sigma(cTx)) * dTx * dTx;
	}

    //          n
    //         --
    // l(c) = -\  ln(1 + exp(-y_i * c^T x_i))
    //         /_
    //         i=1
    state.logLikelihood -= log( 1. + exp(-newY * cTx) );
	
	/* Construct the return tuple */
	memset(resultNull, 0, sizeof(resultNull));
	resultNull[3] = state.dir == NULL;
	resultNull[4] = state.grad == NULL;
    result = logregr_state_to_heap_tuple(&state, resultNull, fcinfo);
	PG_RETURN_DATUM(HeapTupleGetDatum(result));
}

Datum float8_cg_update_combine(PG_FUNCTION_ARGS)
{
	HeapTupleHeader	tuple1 = PG_ARGISNULL(0) ?
						NULL : PG_GETARG_HEAPTUPLEHEADER(0),
                    tuple2 = PG_ARGISNULL(1) ?
						NULL : PG_GETARG_HEAPTUPLEHEADER(1);
    LogRegrState    state1, state2, returnState;
    int             numNulls1 = 0, numNulls2 = 0;
	bool			isNull[10];
    
    copy_tuple_to_logregr_state(tuple1, &state1, isNull,
        /* copyIntraIterationState */ true);
    for (unsigned int i = 0; i < sizeof(isNull); i++)
        numNulls1 += (int) isNull[i];

    copy_tuple_to_logregr_state(tuple2, &state2, isNull,
        /* copyIntraIterationState */ true);
    for (unsigned int i = 0; i < sizeof(isNull); i++)
        numNulls2 += (int) isNull[i];
    
    // FIXME: This only partially checks the input for correctness
    // (Of course, unless of bugs in the code, these conditions should never be
    // true.)
   	if (numNulls1 + numNulls2 > 0 ||
        state1.iteration != state2.iteration ||
        state1.len != state2.len ||
        state1.beta != state2.beta)
		ereport(ERROR, 
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
             errmsg("preliminary segment-level calculation function \"%s\" called with invalid parameters",
                format_procedure(fcinfo->flinfo->fn_oid))));
                
    memset(&returnState, 0, sizeof(returnState));
    copy_tuple_to_logregr_state(tuple1, &returnState, isNull, false);
    
    returnState.count = state1.count + state2.count;
    returnState.dTHd = state1.dTHd + state2.dTHd;
    for (int i = 0; i < state1.len; i++) {
        ((float8*) ARR_DATA_PTR(returnState.gradNew))[i] =
            ((float8*) ARR_DATA_PTR(state1.gradNew))[i] +
            ((float8*) ARR_DATA_PTR(state2.gradNew))[i];
    }
    returnState.logLikelihood = state1.logLikelihood + state2.logLikelihood;
    
	/* Construct the return tuple */
	memset(isNull, 0, sizeof(isNull));
    PG_RETURN_DATUM(HeapTupleGetDatum(
        logregr_state_to_heap_tuple(&returnState, isNull, fcinfo)
    ));
}

Datum
float8_irls_update_accum(PG_FUNCTION_ARGS)
{
    bool            goodArguments;
    MRegrAccumState state;
	HeapTupleHeader	iterationStateTuple = PG_NARGS() < 3 || PG_ARGISNULL(3) ?
						NULL : PG_GETARG_HEAPTUPLEHEADER(3);
    int32           newY;
	int             len, i, j;
    float8          cTx, a, z;
    ArrayType       *coef = NULL;
    float8          *coefData;

	goodArguments = float8_mregr_accum_get_state(fcinfo, &state);
	if (!goodArguments) {
        if (PG_ARGISNULL(0))
			PG_RETURN_NULL();
        else
            PG_RETURN_ARRAYTYPE_P(PG_GETARG_ARRAYTYPE_P(0));
	}
	newY  = PG_GETARG_BOOL(1) ? 1 : -1;

    len = (int) *state.len;

    if (iterationStateTuple != NULL) {
        bool  coefIsNull;
        Datum coefDatum = GetAttributeByNum(iterationStateTuple, 1, &coefIsNull);
    
		coef = coefIsNull ? NULL : DatumGetArrayTypeP(coefDatum);
    }

    // If the array with coefficients is NULL or contains NULLs, assume
    // that we are in the initial iteration. In that case initialize the vector
    // of coefficients: c_0 = 0
    if (coef != NULL) {
        if (ARR_NDIM(coef) != 1 || ARR_DIMS(coef)[0] != len ||
            ARR_ELEMTYPE(coef) != FLOAT8OID)
      		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("transition function \"%s\" called with invalid parameters",
					format_procedure(fcinfo->flinfo->fn_oid))));
        
        if (ARR_NULLBITMAP(coef))
            coef = NULL;
    }
    
    if (coef == NULL) {
        coefData = (float8 *) palloc(sizeof(float8) * len);
        memset(coefData, 0, sizeof(float8) * len);
    } else {
        coefData = (float8 *) ARR_DATA_PTR(coef);
    }

    // cTx = c_i^T x_i
	cTx = 0.;
	for (int i = 0; i < len; i++)
		cTx += state.newXData[i] * coefData[i];
    
    // a_i = sigma(c^T x_i) sigma(-c^T x_i)
    a = sigma(cTx) * sigma(-cTx);
    
    // FIXME: y has different signs than in Minka (2003). Where is the bug?
    // Note: sigma(y_i c^T x_i) = 1 - sigma(-y_i c^T x_i).
    //
    //               sigma(y_i c^T x_i) y_i
    // z_i = c^T x + ----------------------
    //                           a_i
    z = cTx + sigma(newY * cTx) * newY / a;

	(*state.count)++;
    // Currently, we are only computing coefficients. Therefore, the following
    // two lines are not needed.
	// *state.sumy += z * sqrt(a);
	// *state.sumy2 += z * z * a;
	for (i = 0; i < len; i++)
		state.Xty[i] += z * state.newXData[i] * a;
	
	/* Compute the matrix X[] * X'[] and add it in */
	for (i = 0; i < len; i++)
		for (j = 0; j < len; j++)
			state.XtX[i*len + j] += state.newXData[i] * state.newXData[j] * a;
    
    if (coef == NULL)
        pfree(coefData);
    
    // We use state.sumy to store the log likelihood.
    //          n
    //         --
    // l(c) = -\  ln(1 + exp(-y_i * c^T x_i))
    //         /_
    //         i=1
    *state.sumy -= log( 1. + exp(-newY * cTx) );

	PG_RETURN_ARRAYTYPE_P(state.stateAsArray);
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

/**
 * Use conjugate-gradient approach to compute logistic regression coefficients.
 *   
 * The method we are using is known as Fletcher–Reeves method in the
 * literature, where we use the Hestenes-Stiefel rule for calculating the step
 * size.
 *
 * The gradient of \f$l(\boldsymbol c)\f$ is
   @verbatim
                 n
                --  
     ∇_c l(c) = \  (1 - σ(z_i c^T x_i)) z_i x_i
                /_ 
                i=1
   @endverbatim
 *
 * We compute
 *
   @verbatim
   For k = 0, 1, 2, ...:

                       n
                      --  
     g_0 = ∇_c l(0) = \  (1 - σ(z_i c^T x_i)) z_i x_i
                      /_ 
                      i=1
     
     d_0 = g_0
     
            g_0^T d_0
     c_0 = ----------- d_0
           d_0^T H d_0
       
   For k = 1, 2, ...:

     g_k = ∇_c l(c_{k-1})
     
            g_k^T (g_k - g_{k-1})
     β_k = -----------------------
           d_{k-1} (g_k - g_{k-1})

     d_k = g_k - β_k d_{k-1}
     
                      g_k^T d_k
     c_k = c_{k-1} + ----------- d_k
                     d_k^T H d_k

   where:
                      n
                     --
     d_k^T H d_k = - \  σ(c^T x_i) (1 - σ(c^T x_i)) (d^T x_i)^2 
                     /_
                     i=1

   and

   H = the Hessian of the objective
   @endverbatim
 */

Datum float8_cg_update_final(PG_FUNCTION_ARGS)
{
	bool			goodArguments;
	bool			resultNull[10];
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
			DirectFunctionCall2(matrix_add,
                PointerGetDatum(state.coef),
			
                // alpha_k * d_k
                DirectFunctionCall2(float8_matrix_smultiply,
                    PointerGetDatum(state.dir), Float8GetDatum(-alpha))
            )
        );
	}

	/* Construct the return tuple */
    state.iteration++;
	memset(resultNull, 0, sizeof(resultNull));
	resultNull[6] = resultNull[7] = resultNull[8] = true;
    result = logregr_state_to_heap_tuple(&state, resultNull, fcinfo);
	PG_RETURN_DATUM(HeapTupleGetDatum(result));	
}

Datum float8_irls_update_final(PG_FUNCTION_ARGS)
{
	bool            goodArguments;
	MRegrState      state;
	ArrayType       *coef;

	TypeFuncClass	funcClass;
	Oid				resultType;
	TupleDesc		resultDesc;
	Datum			resultDatum[2];
	bool			resultNull[2];
	HeapTuple		result;
	
	goodArguments = float8_mregr_get_state(fcinfo, &state);
	if (!goodArguments)
		PG_RETURN_NULL();
	
	float8_mregr_compute(&state, &coef /* coefficients */, NULL /* R2 */,
						 NULL /* t-statistics */, NULL /* p-values */);

    /* Construct the return tuple */
	funcClass = get_call_result_type(fcinfo, &resultType, &resultDesc);
	BlessTupleDesc(resultDesc);
	
	resultDatum[0] = PointerGetDatum(coef);
    /* We used sumy in MRegrState to store the log-likelihood */
	resultDatum[1] = Float8GetDatum(state.sumy);
	memset(resultNull, 0, sizeof(resultNull));
	
	result = heap_form_tuple(resultDesc, resultDatum, resultNull);
	PG_RETURN_DATUM(HeapTupleGetDatum(result));
}


Datum logregr_should_terminate(PG_FUNCTION_ARGS)
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
