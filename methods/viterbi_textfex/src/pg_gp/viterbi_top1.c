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

/**
 * calculate the top1 labeling of each sentence and the probablity of that top1 labeling using viterbi algorithm
 * @param segtbl the segments in a sentence/document
 * @param mArray  m factors
 * @param rArray  r factors
 * @param nlabel total number of labels in the label space
 * @return the top1 labeling(best labeling) of the sentence and the probability of the top1 labeling
 **/

PG_FUNCTION_INFO_V1(__vcrf_top1_label);

Datum
__vcrf_top1_label(PG_FUNCTION_ARGS)
{
	ArrayType *mArray,*rArray, *result, *path;
        ArrayType *prev_top1_array, *curr_top1_array, *prev_norm_array, *curr_norm_array;
        int ndims, ndatabytes, nbytes, norm_factor, i;
        int start_pos, label, currlabel, prevlabel, doclen, nlabel;
        Oid     element_type;
        if(PG_ARGISNULL(0)||PG_ARGISNULL(1)||PG_ARGISNULL(2))
        	PG_RETURN_NULL();	     
        mArray = PG_GETARG_ARRAYTYPE_P(0);
        element_type = ARR_ELEMTYPE(mArray);        
        rArray = PG_GETARG_ARRAYTYPE_P(1);
        nlabel = PG_GETARG_INT32(2);
        doclen = ARR_DIMS(rArray)[0]/nlabel;
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
        result = (ArrayType *) palloc(sizeof(int)*(doclen+2) + ARR_OVERHEAD_NONULLS(1));
        SET_VARSIZE(result, sizeof(int)*(doclen+2) + ARR_OVERHEAD_NONULLS(1));
        result->ndim = 1;
        result->dataoffset = 0;
        result->elemtype = element_type;
        ARR_DIMS(result)[0] = doclen+2; 
        ARR_LBOUND(result)[0] = 1;
        for(start_pos=0;start_pos<doclen;start_pos++){
            for(label=0; label<nlabel; label++){
	       ((int*)ARR_DATA_PTR(curr_top1_array))[label] = 0;
	       ((int*)ARR_DATA_PTR(curr_norm_array))[label] = 0;
            }
            if (start_pos == 0){
               for(int label=0; label<nlabel; label++){
                  ((int*)ARR_DATA_PTR(curr_norm_array))[label] = ((int*)ARR_DATA_PTR(rArray))[start_pos*nlabel+label] + 
                                                                 ((int*)ARR_DATA_PTR(mArray))[label]; 
                  ((int*)ARR_DATA_PTR(curr_top1_array))[label] = ((int*)ARR_DATA_PTR(rArray))[start_pos*nlabel+label] + 
                                                                 ((int*)ARR_DATA_PTR(mArray))[label]; 
               }

            } else if(start_pos == doclen-1){
                   for(currlabel=0; currlabel<nlabel; currlabel++){
                       for(prevlabel=0; prevlabel<nlabel; prevlabel++){
                          // calculate the best label sequence
                          int top1_new_score = ((int*)ARR_DATA_PTR(prev_top1_array))[prevlabel] + 
                                               ((int*)ARR_DATA_PTR(rArray))[start_pos*nlabel+currlabel] +
                                               ((int*)ARR_DATA_PTR(mArray))[(prevlabel+1)*nlabel+currlabel] +
                                               ((int*)ARR_DATA_PTR(mArray))[(nlabel+1)*nlabel+currlabel];
                          if(top1_new_score > ((int*)ARR_DATA_PTR(curr_top1_array))[currlabel]){
                             ((int*)ARR_DATA_PTR(curr_top1_array))[currlabel] = top1_new_score;
                             ((int*)ARR_DATA_PTR(path))[start_pos*nlabel+currlabel] = prevlabel;
                          }
                          // calculate the probability of the best label sequence
                          int norm_new_score = ((int*)ARR_DATA_PTR(prev_norm_array))[prevlabel] +
                                               ((int*)ARR_DATA_PTR(rArray))[start_pos*nlabel+currlabel] +
                                               ((int*)ARR_DATA_PTR(mArray))[(prevlabel+1)*nlabel+currlabel] +
                                               ((int*)ARR_DATA_PTR(mArray))[(nlabel+1)*nlabel+currlabel];
		          // 0.5 is for rounding
	                  ((int*)ARR_DATA_PTR(curr_norm_array))[currlabel] = 
                          (int)(log(exp(((int*)ARR_DATA_PTR(curr_norm_array))[currlabel]/1000.0) + 
                          exp(norm_new_score/1000.0))*1000.0 + 0.5);
                       } 
                   }
            } else {
                   for(currlabel=0; currlabel<nlabel; currlabel++){
                       for(prevlabel=0; prevlabel<nlabel; prevlabel++){
                          // calculate the best label sequence
                          int top1_new_score = ((int*)ARR_DATA_PTR(prev_top1_array))[prevlabel] +
                                               ((int*)ARR_DATA_PTR(rArray))[start_pos*nlabel+currlabel] +
                                               ((int*)ARR_DATA_PTR(mArray))[(prevlabel+1)*nlabel+currlabel];
                          if(top1_new_score > ((int*)ARR_DATA_PTR(curr_top1_array))[currlabel]){
                             ((int*)ARR_DATA_PTR(curr_top1_array))[currlabel] = top1_new_score;
                             ((int*)ARR_DATA_PTR(path))[start_pos*nlabel+currlabel] = prevlabel;
                          }
                          // calculate the probability of the best label sequence
                          int norm_new_score = ((int*)ARR_DATA_PTR(prev_norm_array))[prevlabel] +
                                               ((int*)ARR_DATA_PTR(rArray))[start_pos*nlabel+currlabel] +
                                               ((int*)ARR_DATA_PTR(mArray))[(prevlabel+1)*nlabel+currlabel];
                          // 0.5 is for rounding
                          ((int*)ARR_DATA_PTR(curr_norm_array))[currlabel] =
                          (int)(log(exp(((int*)ARR_DATA_PTR(curr_norm_array))[currlabel]/1000.0) +
                          exp(norm_new_score/1000.0))*1000.0 + 0.5);
                       } 
                   }
            }
            for(label=0; label<nlabel; label++){
                       ((int*)ARR_DATA_PTR(prev_top1_array))[label] = ((int*)ARR_DATA_PTR(curr_top1_array))[label];
                       ((int*)ARR_DATA_PTR(prev_norm_array))[label] = ((int*)ARR_DATA_PTR(curr_norm_array))[label];
            }
        }
        // find the label of the last token in a sentence
        int top1label = 0;
        int maxscore =0;
        int pos =0;
        for(label=0; label<nlabel; label++){
               if(((int*)ARR_DATA_PTR(curr_top1_array))[label]>maxscore){
	          maxscore = ((int*)ARR_DATA_PTR(curr_top1_array))[label];
                  top1label = label;
               }
        }
        // trace back to get the labels for the rest tokens in a sentence
        ((int*)ARR_DATA_PTR(result))[doclen-1] = top1label;
        for (pos=doclen-1; pos>=1; pos--){
            top1label = ((int*)ARR_DATA_PTR(path))[pos*nlabel+top1label];  
            ((int*)ARR_DATA_PTR(result))[pos-1] = top1label;           
        }
        // compute the sum_i of log(v1[i]/1000), return (e^sum)*1000
        // used in the UDFs which needs marginalization e.g. normalization 
        norm_factor = 0;
        for(i = 0; i < nlabel; i++) {
                norm_factor = (int)(log(exp(norm_factor/1000.0) + exp(((int*)ARR_DATA_PTR(curr_norm_array))[i]/1000.0))*1000.0 + 0.5);
        }
        ((int*)ARR_DATA_PTR(result))[doclen] = maxscore;           
        ((int*)ARR_DATA_PTR(result))[doclen+1] = norm_factor;           
        PG_RETURN_ARRAYTYPE_P(result);
}
