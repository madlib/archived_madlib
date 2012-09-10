#include "postgres.h"
#include <string.h>
#include "fmgr.h"
#include "utils/array.h"
#include <math.h>
#include "catalog/pg_type.h"

#ifndef NO_PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

#ifndef min
#define min(a,b)((a) < (b) ? (a):(b))
#endif

Datum vcrf_top1_label(PG_FUNCTION_ARGS);

/**
 * @file viterbi_top1.c
 * @brief find the most probable sequence assignment and the conditional probability using Viterbi algorithm [1]
 * @date February 2012
 * @param marray  Encode the edge feature, start feature and end feature
 * @param rarray  Encode the single state feature, e.g, word feature, regex feature.
 * @param nlabel Total number of labels in the label space
 * @literature
 *  [1] http://en.wikipedia.org/wiki/Viterbi_algorithm
 * @return the most probable sequence assignment and two integer numbers used to calculate the conditional probability.
 *         the fist number is the numerator, the second number is the normalization factor.
 **/

PG_FUNCTION_INFO_V1(vcrf_top1_label);

Datum
vcrf_top1_label(PG_FUNCTION_ARGS)
{
	ArrayType *result;
        int *prev_top1_array, *curr_top1_array, *prev_norm_array, *curr_norm_array, *path, *mArray, *rArray;
        int ndatabytes, nbytes, norm_factor, i;
        int start_pos, label, currlabel, prevlabel, doclen, nlabel;
        Oid     element_type;
        mArray = (int*)ARR_DATA_PTR(PG_GETARG_ARRAYTYPE_P(0));
        element_type = ARR_ELEMTYPE(PG_GETARG_ARRAYTYPE_P(0));        
        rArray = (int*)ARR_DATA_PTR(PG_GETARG_ARRAYTYPE_P(1));
        nlabel = PG_GETARG_INT32(2);
        doclen = ARR_DIMS(PG_GETARG_ARRAYTYPE_P(1))[0]/nlabel;
        // initialize the four temporary arrays
        ndatabytes = sizeof(int) * nlabel;
        prev_top1_array = (int*) palloc0(ndatabytes);
        curr_top1_array = (int*) palloc0(ndatabytes);
        prev_norm_array = (int*) palloc0(ndatabytes);
        curr_norm_array = (int*) palloc0(ndatabytes);
        
        // define the path
        nbytes = doclen*nlabel*sizeof(int); 
        path = (int*)palloc0(nbytes);
        
        // define the output, the first doclen elements are to store the best label sequence
        // the last two elements are used to calculate the probability.
        result = (ArrayType *) palloc(sizeof(int)*(doclen+1) + ARR_OVERHEAD_NONULLS(1));
        SET_VARSIZE(result, sizeof(int)*(doclen+1) + ARR_OVERHEAD_NONULLS(1));
        result->ndim = 1;
        result->dataoffset = 0;
        result->elemtype = element_type;
        ARR_DIMS(result)[0] = doclen+1; 
        ARR_LBOUND(result)[0] = 1;
        for(start_pos=0;start_pos<doclen;start_pos++){
            memset(curr_top1_array, 0, nlabel*sizeof(int));
            memset(curr_norm_array, 0, nlabel*sizeof(int));
            if (start_pos == 0){// the first token in a sentence, the start feature to be fired.
               for(int label=0; label<nlabel; label++){
                  curr_norm_array[label] = rArray[start_pos*nlabel+label] + mArray[label]; 
                  curr_top1_array[label] = rArray[start_pos*nlabel+label] + mArray[label]; 
               }
            } else {
                    for(currlabel=0; currlabel<nlabel; currlabel++){
                       for(prevlabel=0; prevlabel<nlabel; prevlabel++){
                          // calculate the best label sequence
                          int top1_new_score = prev_top1_array[prevlabel] + rArray[start_pos*nlabel+currlabel] +
                                               mArray[(prevlabel+1)*nlabel+currlabel];
                          // the last token in a sentence, the end feature should be fired 
                          if (start_pos == doclen-1){
                             top1_new_score += mArray[(nlabel+1)*nlabel+currlabel];
                          }
                          if(top1_new_score > curr_top1_array[currlabel]){
                             curr_top1_array[currlabel] = top1_new_score;
                             path[start_pos*nlabel+currlabel] = prevlabel;
                          }
                          // calculate the probability of the best label sequence
                          int norm_new_score = prev_norm_array[prevlabel] + rArray[start_pos*nlabel+currlabel] +
                                               mArray[(prevlabel+1)*nlabel+currlabel];
                          // the last token in a sentence, the end feature should be fired 
                          if(start_pos == doclen-1){
                              norm_new_score += mArray[(nlabel+1)*nlabel+currlabel];
                          }  
                          // the following wants to do z=log(exp(x)+exp(y)), the faster implementation is 
                          // z=min(x,y) + log(exp(abs(x-y))+1)
                          // 0.5 is for rounding
                          if (curr_norm_array[currlabel] == 0){
                              curr_norm_array[currlabel] = norm_new_score;
                          } else {
                              curr_norm_array[currlabel] = min(curr_norm_array[currlabel], norm_new_score) + 
                              (int)(log(exp(abs(norm_new_score - curr_norm_array[currlabel])/1000.0) + 1)*1000.0 + 0.5);
                          }
                       } 
                   }
            }
            for(label=0; label<nlabel; label++){
                       prev_top1_array[label] = curr_top1_array[label];
                       prev_norm_array[label] = curr_norm_array[label];
            }
        }
        // find the label of the last token in a sentence
        int top1label = 0;
        int maxscore =0;
        int pos =0;
        for(label=0; label<nlabel; label++){
               if(curr_top1_array[label]>maxscore){
	          maxscore = curr_top1_array[label];
                  top1label = label;
               }
        }
        // trace back to get the labels for the rest tokens in a sentence
        ((int*)ARR_DATA_PTR(result))[doclen-1] = top1label;
        for (pos=doclen-1; pos>=1; pos--){
            top1label = path[pos*nlabel+top1label];  
            ((int*)ARR_DATA_PTR(result))[pos-1] = top1label;           
        }
        // compute the sum_i of log(v1[i]/1000), return (e^sum)*1000
        // used in the UDFs which needs marginalization e.g., normalization
        // the following wants to do z=log(exp(x)+exp(y)), the faster implementation is 
        // z=min(x,y) + log(exp(abs(x-y))+1)
        norm_factor = 0;
        for(i = 0; i < nlabel; i++) {
            if(i == 0) {
               norm_factor = curr_norm_array[0];
            } else {
               norm_factor = min(curr_norm_array[i], norm_factor) +
                             (int)(log(exp(abs(norm_factor - curr_norm_array[i])/1000.0) + 1)*1000.0 + 0.5);
            } 
        }
        // calculate the conditional probability.
        // to convert the probability into integer, firstly,let it multiply 1000000, then later make the product divided by 1000000
        // to get the real conditional probability
        ((int*)ARR_DATA_PTR(result))[doclen] = (int)(exp((maxscore - norm_factor)/1000.0)*1000000);

        PG_RETURN_ARRAYTYPE_P(result);
}
