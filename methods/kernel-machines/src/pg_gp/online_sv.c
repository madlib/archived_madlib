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
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

static float8 kernel_dot(float8 * spvs, int32 idx, int32 ind_dim, float8 * ind)
{
	float8 ret = 0;
	int i;
	for (i=0; i!=ind_dim; i++)
		ret += spvs[ind_dim * idx + i] * ind[i];
	return ret;
}

Datum svm_predict_sub(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(svm_predict_sub);

Datum svm_predict_sub(PG_FUNCTION_ARGS)
{
	int32 nsvs = PG_GETARG_INT32(0);
	int32 ind_dim = PG_GETARG_INT32(1);
	ArrayType * weights_arr = PG_GETARG_ARRAYTYPE_P(2);
	ArrayType * support_vectors_arr = PG_GETARG_ARRAYTYPE_P(3);
	ArrayType * ind_arr = PG_GETARG_ARRAYTYPE_P(4);

	float8 * weights = (float8 *)ARR_DATA_PTR(weights_arr);
	float8 * support_vectors = (float8 *)ARR_DATA_PTR(support_vectors_arr);
	float8 * ind = (float8 *)ARR_DATA_PTR(ind_arr);

	int32 i;
	float8 ret = 0;

	for (i=0; i!=nsvs; i++)
		ret += weights[i] * kernel_dot(support_vectors,i,ind_dim,ind);
	
	PG_RETURN_FLOAT8(ret);
}
