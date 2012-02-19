#include "postgres.h"
#include <string.h>
#include "fmgr.h"
#include "utils/array.h"
#include <math.h>
#include "catalog/pg_type.h"

#ifndef NO_PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

Datum __vcrf_top1_label(PG_FUNCTION_ARGS);
Datum __vcrf_sum_array(PG_FUNCTION_ARGS);
Datum __vcrf_sum(PG_FUNCTION_ARGS);

/*@param segments for a document
 *@param m table
 *@param r table
 *@param nlable 
 *calculate the top1 labeling of each sentence using viterbi algorithm
 *create function __vcrf_top1_label(anyarray, anyarray, anyarray, anyelement) returns anyarray
 **/

PG_FUNCTION_INFO_V1(__vcrf_top1_label);

Datum
__vcrf_top1_label(PG_FUNCTION_ARGS)
{
	ArrayType *segtbl, *mArray,*rArray, *result, *path;
        ArrayType *prev_top1_array, *curr_top1_array, *prev_norm_array, *curr_norm_array;
        int ndims, ndatabytes, nbytes, norm_factor;
        int start_pos, label, currlabel, prevlabel, doclen, nlabel;
        Oid     element_type;
        if(PG_ARGISNULL(0)||PG_ARGISNULL(1)||PG_ARGISNULL(2)||PG_ARGISNULL(3))
        	PG_RETURN_NULL();	     
        segtbl = PG_GETARG_ARRAYTYPE_P(0);
        mArray = PG_GETARG_ARRAYTYPE_P(1);
        element_type = ARR_ELEMTYPE(mArray);        
        rArray = PG_GETARG_ARRAYTYPE_P(2);
        nlabel = PG_GETARG_INT32(3);
        doclen = ARR_DIMS(segtbl)[0];
        // initialize the four arrays
        ndatabytes = sizeof(int) * nlabel;
        ndims =1;
        nbytes = ndatabytes +  ARR_OVERHEAD_NONULLS(ndims);
        prev_top1_array = (ArrayType *) palloc0(nbytes);
        curr_top1_array = (ArrayType *) palloc0(nbytes);
        prev_norm_array = (ArrayType *) palloc0(nbytes);
        curr_norm_array = (ArrayType *) palloc0(nbytes);
        SET_VARSIZE(prev_top1_array, nbytes);
        SET_VARSIZE(curr_top1_array, nbytes);
        SET_VARSIZE(prev_norm_array, nbytes);
        SET_VARSIZE(curr_norm_array, nbytes);
        prev_top1_array->ndim = ndims;
        curr_top1_array->ndim = ndims;
        prev_norm_array->ndim = ndims;
        curr_norm_array->ndim = ndims;
        prev_top1_array->dataoffset = 0;
        curr_top1_array->dataoffset = 0;
        prev_norm_array->dataoffset = 0;
        curr_norm_array->dataoffset = 0;
        prev_top1_array->elemtype = element_type;
        curr_top1_array->elemtype = element_type;
        prev_norm_array->elemtype = element_type;
        curr_norm_array->elemtype = element_type;
        ARR_DIMS(prev_top1_array)[0]=nlabel; 
        ARR_DIMS(curr_top1_array)[0]=nlabel; 
        ARR_DIMS(prev_norm_array)[0]=nlabel; 
        ARR_DIMS(curr_norm_array)[0]=nlabel; 
        
        // define the path
        nbytes = doclen*nlabel*sizeof(int) + ARR_OVERHEAD_NONULLS(2); 
        path = (ArrayType*)palloc0(nbytes);
        SET_VARSIZE(path,nbytes);
        path->ndim = 2;
        path->dataoffset = 0;
        path->elemtype = element_type;
        ARR_DIMS(path)[0] = nlabel;
        ARR_DIMS(path)[1] = doclen;
        
        // define the output
        result = (ArrayType *) palloc(sizeof(int) * doclen + ARR_OVERHEAD_NONULLS(1));
        SET_VARSIZE(result, sizeof(int) * doclen + ARR_OVERHEAD_NONULLS(1));
        result->ndim = 1;
        result->dataoffset = 0;
        result->elemtype = element_type;
        ARR_DIMS(result)[0] = doclen; 
        ARR_LBOUND(result)[0] = 1;
        for(start_pos=0;start_pos<doclen;start_pos++){
            for(label=0; label<nlabel; label++){
	       ((int*)ARR_DATA_PTR(curr_top1_array))[label] = 0;
            }
            int segid = ((int*)ARR_DATA_PTR(segtbl))[start_pos]; 
            if (start_pos == 0){
               for(int label=0; label<nlabel; label++){
                  ((int*)ARR_DATA_PTR(prev_norm_array))[label] = ((int*)ARR_DATA_PTR(rArray))[(segid-1)*nlabel+label] + 
                                                                 ((int*)ARR_DATA_PTR(mArray))[label]; 
                  ((int*)ARR_DATA_PTR(prev_top1_array))[label] = ((int*)ARR_DATA_PTR(rArray))[(segid-1)*nlabel+label] + 
                                                                 ((int*)ARR_DATA_PTR(mArray))[label]; 
               }

            } else{
              // calculate the best label sequence
              for(currlabel=0; currlabel<nlabel; currlabel++){
                    for(prevlabel=0; prevlabel<nlabel; prevlabel++){
                        int top1_new_score = ((int*)ARR_DATA_PTR(prev_top1_array))[prevlabel] + 
                                         ((int*)ARR_DATA_PTR(rArray))[(segid-1)*nlabel+currlabel] +
                                         ((int*)ARR_DATA_PTR(mArray))[(prevlabel+1)*nlabel+currlabel];
                        if(top1_new_score > ((int*)ARR_DATA_PTR(curr_top1_array))[currlabel]){
                          ((int*)ARR_DATA_PTR(curr_top1_array))[currlabel] = top1_new_score;
                          ((int*)ARR_DATA_PTR(path))[start_pos*nlabel+currlabel] = prevlabel;
                        }
                    } 
                 }
              }
              // calculate the probability of the best label sequence
              curr_norm_array=__vcrf_sum_array(prev_norm_array, mArray, rArray, segid, nlabel);             
               //curr_norm_array = __vcrf_sum_array(prev_norm_array);             
              for(label=0; label<nlabel; label++){
                       ((int*)ARR_DATA_PTR(prev_top1_array))[label] = ((int*)ARR_DATA_PTR(curr_top1_array))[label];
                       ((int*)ARR_DATA_PTR(prev_norm_array))[label] = ((int*)ARR_DATA_PTR(curr_norm_array))[label];
              }
        }
        // find the maximum top1 label
        int top1label = 0;
        int maxscore =0;
        int pos =0;
        for(label=0; label<nlabel; label++){
               if(((int*)ARR_DATA_PTR(curr_top1_array))[label]>maxscore){
	          maxscore = ((int*)ARR_DATA_PTR(curr_top1_array))[label];
                  top1label = label;
               }
        }
        ((int*)ARR_DATA_PTR(result))[doclen-1] = top1label;
        for (pos=doclen-1; pos>=1; pos--){
            top1label = ((int*)ARR_DATA_PTR(path))[pos*nlabel+top1label];  
            ((int*)ARR_DATA_PTR(result))[pos-1] = top1label;           
        }
        //compute the sum_i of log(v1[i]/1000), return (e^sum)*1000
	//norm_factor = __vcrf_sum(curr_norm_array);
        // define the output
        PG_RETURN_ARRAYTYPE_P(result);
        //PG_RETURN_NULL();
}

/*
 *@param prev array
 *@param m table
 *@param r table
 *@param segid
 *@param nlabel
 *create function __vcrf_sum_array(anyarray, anyarray, anyarray, anyelement, anyelement) returns anyarray as '__vcrf_sum_array' language c strict;
 */
PG_FUNCTION_INFO_V1(__vcrf_sum_array);

Datum
__vcrf_sum_array(PG_FUNCTION_ARGS)

{
        // v1 is prev array, v2 is m table, v3 is r table, v4 is segid, v5 is the 
	ArrayType  *v1, *mArray, *rArray;
	ArrayType  *result;
	int	ndims, ndatabytes, nbytes;
   	int     i, nitems;
        Oid     element_type;
	int32	dataoffset;
	if (PG_ARGISNULL(0) || PG_ARGISNULL(1))
                PG_RETURN_NULL();

	// pointers to the postgres arrays
	v1 = PG_GETARG_ARRAYTYPE_P(0);
	rArray = PG_GETARG_ARRAYTYPE_P(1);
	mArray = PG_GETARG_ARRAYTYPE_P(2);
        int32 segid =  PG_GETARG_INT32(3);
        nitems = PG_GETARG_INT32(4);
	// postgres type of elements in an array
	element_type = ARR_ELEMTYPE(v1);
        /* new array is the same as v1 for top 1 only !! */
	ndims = ARR_NDIM(v1);
	ndatabytes = ARR_SIZE(v1) - ARR_DATA_OFFSET(v1);
	dataoffset = 0;			/* marker for no null bitmap */
	nbytes = ndatabytes + ARR_OVERHEAD_NONULLS(ndims);
	result = (ArrayType *) palloc0(nbytes);
	SET_VARSIZE(result, nbytes);
	result->ndim = ndims;
	result->dataoffset = dataoffset;
	result->elemtype = element_type;
        for(i=0;i<ndims;i++) {
          ARR_DIMS(result)[i]=ARR_DIMS(v1)[i];
          ARR_LBOUND(result)[i]=ARR_LBOUND(v1)[i];
        }
        int n2times = nitems * nitems;
        // do sum over all possible value of y' calculation here
        for(i = 0; i < n2times; i++) {
                  int k = i / nitems;
                  int k_rem = i % nitems;
                  int new_score = ((int*)ARR_DATA_PTR(v1))[k] +
                                         ((int*)ARR_DATA_PTR(rArray))[segid*nitems+k_rem] +
                                         ((int*)ARR_DATA_PTR(mArray))[(k+1)*nitems+k_rem];
		  // 0.5 is for rounding
	          ((int*)ARR_DATA_PTR(result))[k_rem] = 
                  (int)(log(exp(((int*)ARR_DATA_PTR(result))[k_rem]/1000.0) + exp(new_score/1000.0))*1000.0 + 0.5);
        }

	PG_RETURN_ARRAYTYPE_P(result);

}

/*
compute the sum_i of log(v1[i]/1000), return (e^sum)*1000
used in the UDFs which needs marginalization
e.g. normalization 
create function __vcrf_sum(anyarray) returns integer as language c strict;
*/

PG_FUNCTION_INFO_V1(__vcrf_sum);

Datum
__vcrf_sum(PG_FUNCTION_ARGS)
{
        ArrayType  *v1;
        int     i;
        int nitems1;
        int32 score = 0;

        if (PG_ARGISNULL(0))
                PG_RETURN_NULL();

        // pointers to the postgres arrays
        v1 = PG_GETARG_ARRAYTYPE_P(0);

        // number of items in the arrays
        nitems1 = ARR_DIMS(v1)[0];

        for(i = 0; i < nitems1; i++) {
                score = (int)(log(exp(score/1000.0) + exp(((int*)ARR_DATA_PTR(v1))[i]/1000.0))*1000.0 + 0.5);
        }

        PG_RETURN_INT32(score);
}
