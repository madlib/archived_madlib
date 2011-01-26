#include "postgres.h"
#include <string.h>
#include "fmgr.h"
#include "utils/array.h"
#include <math.h>
#include "catalog/pg_type.h"

Datum vcrf_max_top1_array(PG_FUNCTION_ARGS);
Datum vcrf_topk_array(PG_FUNCTION_ARGS);
Datum vcrf_sum_array(PG_FUNCTION_ARGS);
Datum vcrf_sum(PG_FUNCTION_ARGS);

PG_MODULE_MAGIC;

/*
 *
 * create function vcrf_max_top1_array(anyarray) returns anyarray as '/home/daisyw/p84/mingw-postgresql-8.4dev/src/bayesstore/vcrf_topk_array', 'vcrf_max_top1_array' language c strict;
 *
 *  */

PG_FUNCTION_INFO_V1(vcrf_max_top1_array);

Datum
vcrf_max_top1_array(PG_FUNCTION_ARGS)
{
        ArrayType  *v1 ;
        // ,
        //                    *v2;
        ArrayType  *result;
        // int                *dims,
                           // *lbs,
        int                        ndims,
                                // nitems,
                                ndatabytes,
                                nbytes;
        int         nitems1;
        // int                *dims1,
        //                    *lbs1,
        //                         ndims1,
        //                         nitems1,
        //                         ndatabytes1;
        // int                *dims2,
                           // *lbs2,
                                // ndims2,
                                // nitems2,
                                // ndatabytes2;
        int                     i;
        // char       *dat1,
        //                    *dat2;
        // bits8      *bitmap1,
        //                    *bitmap2;
        Oid                     element_type;
    // Oid                     element_type1;
    //         Oid                     element_type2;
        int32           dataoffset;

        if (PG_ARGISNULL(0))
                abort();

        v1 = PG_GETARG_ARRAYTYPE_P(0);

        element_type = ARR_ELEMTYPE(v1);

        nitems1 = ARR_DIMS(v1)[1];

        if (nitems1 == 0)
                abort();

        /* new array is the same as v1 for top 1 only !! */
        ndims = 2;
        ndatabytes = 3*sizeof(int);
        dataoffset = 0;                 /* marker for no null bitmap */
        nbytes = ndatabytes + ARR_OVERHEAD_NONULLS(ndims);

        result = (ArrayType *) palloc(nbytes);
        SET_VARSIZE(result, nbytes);
        result->ndim = ndims;
        result->dataoffset = dataoffset;
        result->elemtype = element_type;
        ARR_DIMS(result)[0]=3; /* num elements in first dim */
        ARR_DIMS(result)[1]=1; /* num elements in second dim */
	 /* postgres arrays can be index from any start position eg:
 *            int[-5:14]. this is unlike c, where index always starts at 0 */
        ARR_LBOUND(result)[0]=1; /* index of the first dim starts at .. */
        ARR_LBOUND(result)[1]=1; /* index of second dim starts at ... */

	// do top1 calculation here
	for(i = 0; i < nitems1; i++) {
                  if (i == 0 || ((int*)ARR_DATA_PTR(v1))[0*nitems1+i] >
                                ((int*)ARR_DATA_PTR(v1))[0*nitems1+((int*)ARR_DATA_PTR(result))[0*1+0]]) {
                        ((int*)ARR_DATA_PTR(result))[0*1+0] = i;
                        ((int*)ARR_DATA_PTR(result))[1*1+0] = ((int*)ARR_DATA_PTR(v1))[1*nitems1+i];
                        ((int*)ARR_DATA_PTR(result))[2*1+0] = ((int*)ARR_DATA_PTR(v1))[0*nitems1+i];
                  }
        }
        
        PG_RETURN_ARRAYTYPE_P(result);
}

/*

create function vcrf_topk_array(anyarray, anyarray) returns anyarray as '/home/daisyw/p84/mingw-postgresql-8.4dev/src/bayesstore/vcrf_topk_array', 'vcrf_topk_array' language c strict;
 
 */

PG_FUNCTION_INFO_V1(vcrf_topk_array);

Datum
vcrf_topk_array(PG_FUNCTION_ARGS)
{
	ArrayType  *v1,
			   *v2;
	ArrayType  *result;
    // int         *dims,
    //         *lbs,
    int         ndims,
                // nitems,
				ndatabytes,
				nbytes;
    // int         *dims1,
    //         *lbs1,
    //          ndims1,
    //          nitems1,
    //          ndatabytes1;
    // int         *dims2,
    //         *lbs2,
    //          ndims2,
    //          nitems2,
    //          ndatabytes2;
    int         nitems1, nitems2;
	int			i;
    // char    *dat1,
    //         *dat2;
    // bits8       *bitmap1,
    //         *bitmap2;
	Oid			element_type;
    // Oid          element_type1;
    // Oid          element_type2;
	int32		dataoffset;
    int         spos;

	if (PG_ARGISNULL(0) || PG_ARGISNULL(1))
		abort();

	// pointers to the postgres arrays
	v1 = PG_GETARG_ARRAYTYPE_P(0);
	v2 = PG_GETARG_ARRAYTYPE_P(1);

	// postgres type of elements in an array
	element_type = ARR_ELEMTYPE(v1);

	// number of items in the arrays
	nitems1 = ARR_DIMS(v1)[1];
	nitems2 = ARR_DIMS(v2)[0];

//	if (nitems1 == 0 || nitems2 == 0 || nitems2 % nitems1 != 0)
//		abort();

        /* new array is the same as v1 for top 1 only !! */
	ndims = 2;
	ndatabytes = ARR_SIZE(v1) - ARR_DATA_OFFSET(v1);
	dataoffset = 0;			/* marker for no null bitmap */
	nbytes = ndatabytes + ARR_OVERHEAD_NONULLS(ndims);

	result = (ArrayType *) palloc(nbytes);
	SET_VARSIZE(result, nbytes);
	result->ndim = ndims;
	result->dataoffset = dataoffset;
	result->elemtype = element_type;
        for(i=0;i<ndims;i++) {
          ARR_DIMS(result)[i]=ARR_DIMS(v1)[i];
          ARR_LBOUND(result)[i]=ARR_LBOUND(v1)[i];
        }

	// array v2 can either have nitems1 or nitems1*nitems1 or (nitems1+1)*nitems1 items, because of the -1 arrays from MR 
	// for the first and third case we want to skip the first nitems1 items 
	//int spos=0;
	spos = nitems2 % (nitems1*nitems1);

        // do top 1 calculation here
        //for(kk = 0; kk < topk; kk++) { // TODO: handle k > 1 dimenstions
        for(i = spos; i < nitems2; i++) {
        //for(i = 0; i < nitems2; i++) {
                  int k = (i-spos) / nitems1;
                  int k_rem = i % nitems1;

	     	  int new_score = ((int*)ARR_DATA_PTR(v1))[0*nitems1+k] + ((int*)ARR_DATA_PTR(v2))[i];

                  if (k == 0 || new_score > ((int*)ARR_DATA_PTR(result))[0*nitems1+k_rem]) {
		    ((int*)ARR_DATA_PTR(result))[0*nitems1+k_rem] = new_score;
		    ((int*)ARR_DATA_PTR(result))[1*nitems1+k_rem] = k;
                  }
        }

	PG_RETURN_ARRAYTYPE_P(result);
}

/*
 * 

create function vcrf_sum_array(anyarray, anyarray) returns anyarray as '/home/daisyw/p84/mingw-postgresql-8.4dev/src/bayesstore/vcrf_topk_array', 'vcrf_sum_array' language c strict;
 
 */

PG_FUNCTION_INFO_V1(vcrf_sum_array);

Datum
vcrf_sum_array(PG_FUNCTION_ARGS)
{
	ArrayType  *v1,
			   *v2;
	ArrayType  *result;
    // int         *dims,
    //         *lbs,
	int			ndims,
                // nitems,
				ndatabytes,
				nbytes;
    int         nitems1, nitems2;
    // int         *dims1,
    //         *lbs1,
    //          ndims1,
    //          nitems1,
    //          ndatabytes1;
    // int         *dims2,
    //         *lbs2,
    //          ndims2,
    //          nitems2,
    //          ndatabytes2;
    int          i;
    // char    *dat1,
    //         *dat2;
    // bits8       *bitmap1,
    //         *bitmap2;
    Oid          element_type;
    // Oid          element_type1;
    // Oid          element_type2;
	int32		dataoffset;
    int         spos;

	if (PG_ARGISNULL(0) || PG_ARGISNULL(1))
		abort();

	// pointers to the postgres arrays
	v1 = PG_GETARG_ARRAYTYPE_P(0);
	v2 = PG_GETARG_ARRAYTYPE_P(1);

	// postgres type of elements in an array
	element_type = ARR_ELEMTYPE(v1);

	// number of items in the arrays
	nitems1 = ARR_DIMS(v1)[0];
	nitems2 = ARR_DIMS(v2)[0];

//	if (nitems1 == 0 || nitems2 == 0 || nitems2 % nitems1 != 0)
//		abort();

        /* new array is the same as v1 for top 1 only !! */
	ndims = ARR_NDIM(v1);
	ndatabytes = ARR_SIZE(v1) - ARR_DATA_OFFSET(v1);
	dataoffset = 0;			/* marker for no null bitmap */
	nbytes = ndatabytes + ARR_OVERHEAD_NONULLS(ndims);

	result = (ArrayType *) palloc(nbytes);
	SET_VARSIZE(result, nbytes);
	result->ndim = ndims;
	result->dataoffset = dataoffset;
	result->elemtype = element_type;
        for(i=0;i<ndims;i++) {
          ARR_DIMS(result)[i]=ARR_DIMS(v1)[i];
          ARR_LBOUND(result)[i]=ARR_LBOUND(v1)[i];
        }

	// array v2 can either have nitems1 or nitems1*nitems1 or (nitems1+1)*nitems1 items, because of the -1 arrays from MR 
	// for the first and third case we want to skip the first nitems1 items 
	//int spos=0;
	spos = nitems2 % (nitems1*nitems1);
	for(i=0; i<nitems1; i++)
	  ((int*)ARR_DATA_PTR(result))[i] = 0; 

        // do sum over all possible value of y' calculation here
        for(i = spos; i < nitems2; i++) {
                  int k = (i-spos) / nitems1;
                  int k_rem = i % nitems1;

	     	  int new_score = ((int*)ARR_DATA_PTR(v1))[0*nitems1+k] + ((int*)ARR_DATA_PTR(v2))[i];
		// 0.5 is for rounding
	         ((int*)ARR_DATA_PTR(result))[0*nitems1+k_rem] = (int)(log(exp(((int*)ARR_DATA_PTR(result))[0*nitems1+k_rem]/1000.0) + exp(new_score/1000.0))*1000.0 + 0.5);
        }

	PG_RETURN_ARRAYTYPE_P(result);
}

/*

compute the sum_i of log(v1[i]/1000), return (e^sum)*1000
used in the UDFs which needs marginalization
e.g. normalization 

create function vcrf_sum(anyarray) returns integer as '/home/daisyw/p84/mingw-postgresql-8.4dev/src/bayesstore/vcrf_topk_array', 'sum' language c strict;
 
 */

PG_FUNCTION_INFO_V1(vcrf_sum);

Datum
vcrf_sum(PG_FUNCTION_ARGS)
{
	ArrayType  *v1;
	int	i;
	int nitems1;
    int32 score;

	if (PG_ARGISNULL(0))
		abort();

	// pointers to the postgres arrays
	v1 = PG_GETARG_ARRAYTYPE_P(0);

	// number of items in the arrays
	nitems1 = ARR_DIMS(v1)[0];

	score = 0;
        for(i = 0; i < nitems1; i++) {
		score = (int)(log(exp(score/1000.0) + exp(((int*)ARR_DATA_PTR(v1))[i]/1000.0))*1000.0 + 0.5);
        }

	PG_RETURN_INT32(score);
}


