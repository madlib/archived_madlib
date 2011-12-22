/*
 *
 * @file decision_tree.c
 *
 * @brief decision tree related aggregate and utility functions written in C
 *
 * @date Dec. 22 2011
 */

#include <float.h>
#include <math.h>

#include "postgres.h"
#include "fmgr.h"
#include "utils/array.h"
#include "catalog/pg_type.h"

#ifndef NO_PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

#ifdef __DT_SHOW_DEBUG_INFO__
#define dtelog(...) elog(__VA_ARGS__)
#else
#define dtelog(...)
#endif

/*
 * This function is used to test if a float value is 0.
 * Due to the precision of floating numbers, we can not compare them directly with 0.
 */
#define is_float_zero(value)  ((value) < DBL_EPSILON && (value) > -DBL_EPSILON)

/*
 *  For Error Based Pruning (EBP), we need to compute the additional errors
 *  if the error rate increases to the upper limit of the confidence level.
 *  The coefficient is the square of the number of standard deviations
 *  corresponding to the selected confidence level.
 *  (Taken from Documenta Geigy Scientific Tables (Sixth Edition),
 *  p185 (with modifications).)
 */
static float CONFIDENCE_LEVEL[] = {0, 0.001, 0.005, 0.01, 0.05, 0.10, 0.20, 0.40, 1.00};
static float CONFIDENCE_DEV[]   = {4.0, 3.09, 2.58, 2.33, 1.65, 1.28, 0.84, 0.25, 0.00};

#define MIN_CONFIDENCE_LEVEL 0.00001
#define MAX_CONFIDENCE_LEVEL 1.0


float ebp_calc_coeff_internal
(
	float total_cases,
	float num_errors,
	float conf_level,
	float coeff
);

/*
 * Calculates the total errors used by Error Based Pruning (EBP).
 * This will be wrapped as a plc function.
 *
 * Parameters:
 *      total: 			the number of total cases represented by the node being processed.
 *      probability:   	the probability to mis-classify cases represented by the child nodes
 *      			    if they are pruned with EBP.
 *      conf_level:  	A certainty factor to calculate the confidence limits
 *      				for the probability of error using the binomial theorem.
 * Return:
 *      The computed total error
 */
Datum ebp_calc_coeff(PG_FUNCTION_ARGS)
{
    float8 total 		= PG_GETARG_FLOAT8(0);
    float8 probability 	= PG_GETARG_FLOAT8(1);
    float8 conf_level 	= PG_GETARG_FLOAT8(2);
    float8 result 		= 1;
    float8 coeff 		= 0;

    if (!is_float_zero(1 - conf_level))
    {
    	float8 num_errors = total * (1 - probability);
		result = ebp_calc_coeff_internal(total, num_errors, conf_level, coeff) + num_errors;
    }

	PG_RETURN_FLOAT8((float8)result);
}
PG_FUNCTION_INFO_V1(ebp_calc_coeff);

/*
 * This function calculates the additional errors for EBP.
 * Detailed description of that pruning strategy can be found in the paper
 * of 'Error-Based Pruning of Decision Trees Grown on Very Large Data Sets Can Work!'.
 *
 * Parameters:
 *      total_cases:  the number of total cases represented by the node being processed.
 *      num_errors:   the number of mis-classified cases represented by the child nodes
 *      			  if they are pruned with EBP.
 *      conf_level:   A certainty factor to calculate the confidence limits
 *      			  for the probability of error using the binomial theorem.
 * Return:
 *      The additional errors if we prune the node being processed.
 *
 */
float ebp_calc_coeff_internal
    (
	float total_cases,
	float num_errors,
	float conf_level,
	float coeff
    )
{
	Assert(total_cases > 0);
	Assert(num_errors >= 0);
	Assert(conf_level >= MIN_CONFIDENCE_LEVEL && conf_level <= MAX_CONFIDENCE_LEVEL);
	Assert(coeff >= 0);

    if (is_float_zero(coeff))
    {
        int i = 0;
        while (conf_level > CONFIDENCE_LEVEL[i]) i++;

        coeff = CONFIDENCE_DEV[i-1] +
                (CONFIDENCE_DEV[i] - CONFIDENCE_DEV[i-1]) *
                (conf_level - CONFIDENCE_LEVEL[i-1]) /
                (CONFIDENCE_LEVEL[i] - CONFIDENCE_LEVEL[i-1]);

        coeff = coeff * coeff;
    }

    if (num_errors < 1E-6)
    {
        return total_cases * (1 - exp(log(conf_level) / total_cases));
    }
    else
    if (num_errors < 0.9999)
    {
        float tmp = total_cases * (1 - exp(log(conf_level) / total_cases));
        return tmp + num_errors * 
            (ebp_calc_coeff_internal(total_cases, 1.0, conf_level, coeff) - tmp);
    }
    else
    if (num_errors + 0.5 >= total_cases)
    {
        return 0.67 * (total_cases - num_errors);
    }
    else
    {
        float tmp =
			(
			num_errors + 0.5 + coeff/2 +
			sqrt(coeff * ((num_errors + 0.5) * (1 - (num_errors + 0.5)/total_cases) + coeff/4))
			)
			/ (total_cases + coeff);

        return (total_cases * tmp - num_errors);
    }
}

/*
 * This function allocate int64 array.
 * Parameters:
 *       size:    the size of array.
 *       value:   the initial value. All the
 *                elements of that array are initialized
 *                with that value.
 * Return:
 *      The int64 array allocated on current memory context.
 */

int64* alloc_int64_array(int size, int value)
{
    Assert(size > 0);

    int64 *result   = (int64*)palloc(sizeof(int64) * size);

    if (!result)
        elog(ERROR, "Memory allocation failure");

    memset(result, value, sizeof(int64) * size);
    return result;
}

/*
 * The step function for aggregating the class counts while doing Reduce Error Pruning (REP).
 *
 * Parameters:
 *      class_count_array   The array used to store the accumulated information.
 *                          [0]: the total number of mis-classified cases
 *                          [i]: the number of cases belonging to the ith class
 *      classified_class    The predicted class based on our trained DT model.
 *      original_class      The real class value provided in the validation set.
 *      max_num_of_classes  The total number of distinct class values.
 * Return:
 *      An updated state array.
 */
Datum rep_aggr_class_count_sfunc(PG_FUNCTION_ARGS)
{
    ArrayType *class_count_array    = NULL;
    int array_dim                   = 0;
    int *p_array_dim                = NULL;
    int array_length                = 0;
    int64 *class_count_data         = NULL;
    int classified_class    		= PG_GETARG_INT32(1);
    int original_class      		= PG_GETARG_INT32(2);
    int max_num_of_classes  		= PG_GETARG_INT32(3);

    Assert(original_class > 0 && original_class <= max_num_of_classes);
    Assert(classified_class > 0 && classified_class <= max_num_of_classes);

    /* test if the first argument (class count array) is null */
    if (PG_ARGISNULL(0))
    {
    	/*
    	 * We assume the maximum number of classes is limited (up to millions),
    	 * so that the allocated array won't break our memory limitation.
    	 */
        class_count_data	= alloc_int64_array(max_num_of_classes + 1, 0);
        array_length 		= max_num_of_classes + 1;

        /* construct a new array to keep the aggr states. */
        class_count_array =
        	construct_array(
        		(Datum *)class_count_data,
                array_length,
                FLOAT8OID,
                sizeof(int64),
                true,
                'd'
                );

        if(!class_count_array)
            elog(ERROR, "Array construction failure.");
    }
    else
    {
        class_count_array = PG_GETARG_ARRAYTYPE_P(0);
        if (!class_count_array)
        	elog(ERROR, "Invalid class count array.");

        array_dim           = ARR_NDIM(class_count_array);
        p_array_dim         = ARR_DIMS(class_count_array);
        array_length        = ArrayGetNItems(array_dim,p_array_dim);
        class_count_data    = (int64 *)ARR_DATA_PTR(class_count_array);

        if (array_length != max_num_of_classes + 1)
        	elog(ERROR, "Bad class count data.");
    }

    /*
     * If the condition is met, then the current record has been mis-classified.
     * Therefore, we will need to increase the first element.
     */
    if(original_class != classified_class)
        ++class_count_data[0];

    /* In any case, we will update the original class count */
    ++class_count_data[original_class];

    PG_RETURN_ARRAYTYPE_P(class_count_array);
}
PG_FUNCTION_INFO_V1(rep_aggr_class_count_sfunc);

/*
 * The pre-function for REP. It takes two class count arrays
 * produced by the sfunc and combine them together.
 *
 * Parameters:
 *      1 arg:         The array returned by sfun1
 *      2 arg:         The array returned by sfun2
 * Return:
 *      The array with the combined information
 */
Datum rep_aggr_class_count_prefunc(PG_FUNCTION_ARGS)
{
    ArrayType *class_count_array    = NULL;
    int array_dim                   = 0;
    int *p_array_dim                = NULL;
    int array_length                = 0;
    int64 *class_count_data         = NULL;

    ArrayType *class_count_array2   = NULL;
    int array_dim2                  = 0;
    int *p_array_dim2               = NULL;
    int array_length2               = 0;
    int64 *class_count_data2        = NULL;

    if (PG_ARGISNULL(0) && PG_ARGISNULL(1))
        PG_RETURN_NULL();
    else
    if (PG_ARGISNULL(1) || PG_ARGISNULL(0))
    {
        /* If one of the two array is null, just return the non-null array directly */
    	PG_RETURN_ARRAYTYPE_P(PG_ARGISNULL(1) ? PG_GETARG_ARRAYTYPE_P(0) : PG_GETARG_ARRAYTYPE_P(1));
    }
    else
    {
        /*
         *  If both arrays are not null, we will merge them together.
         */
        class_count_array       = PG_GETARG_ARRAYTYPE_P(0);
        array_dim               = ARR_NDIM(class_count_array);
        p_array_dim             = ARR_DIMS(class_count_array);
        array_length            = ArrayGetNItems(array_dim,p_array_dim);
        class_count_data        = (int64 *)ARR_DATA_PTR(class_count_array);

        class_count_array2      = PG_GETARG_ARRAYTYPE_P(1);
        array_dim2              = ARR_NDIM(class_count_array2);
        p_array_dim2            = ARR_DIMS(class_count_array2);
        array_length2           = ArrayGetNItems(array_dim2,p_array_dim2);
        class_count_data2       = (int64 *)ARR_DATA_PTR(class_count_array2);

        if (array_length != array_length2)
            elog(ERROR, "The size of the two arrays must be the same.");

        for (int index =0; index < array_length; index++)
            class_count_data[index] += class_count_data2[index];

        PG_RETURN_ARRAYTYPE_P(class_count_array);
    }

}
PG_FUNCTION_INFO_V1(rep_aggr_class_count_prefunc);

/*
 * The final function for aggregating the class counts for REP. It takes the class
 * count array produced by the sfunc and produces a two-element array. The first
 * element is the ID of the class that has the maximum number of cases represented by
 * the root node of the subtree being processed. The second element is the number of
 * reduced misclassified cases if the leave nodes of the subtree are pruned.
 *
 * Parameters:
 *      class_count_data:   The array containing all the information for the 
 *                          calculation of Reduced-Error pruning. 
 * Return:
 *      A two element array.
 */
Datum rep_aggr_class_count_ffunc(PG_FUNCTION_ARGS)
{
    ArrayType *class_count_array    = PG_GETARG_ARRAYTYPE_P(0);
    int array_dim                   = ARR_NDIM(class_count_array);
    int *p_array_dim                = ARR_DIMS(class_count_array);
    int array_length                = ArrayGetNItems(array_dim,p_array_dim);
    int64 *class_count_data         = (int64 *)ARR_DATA_PTR(class_count_array);
    int64 *result                   = palloc(sizeof(int64)*2);

    if (!result)
        elog(ERROR, "Memory allocation failure");

    int64 max = class_count_data[1];
    int64 sum = max;
    int maxid = 1;

    for(int i = 2; i < array_length; ++i)
    {
        if(max < class_count_data[i])
        {
            max = class_count_data[i];
            maxid = i;
        }

        sum += class_count_data[i];
    }

    /* maxid is the id of the class, which has the most cases */
    result[0] = maxid;

    /*
     * (sum - max) is the number of mis-classified cases represented by
     * the root node of the subtree being processed
     * class_count_data[0] the total number of mis-classified cases
     */
    result[1] = class_count_data[0] - (sum - max);

    ArrayType* result_array =
    	construct_array(
         (Datum *)result,
         2,
         FLOAT8OID,
         sizeof(int64),
         true,
         'd'
        );

    if(!result_array)
        elog(ERROR, "Array construction failure.");

    PG_RETURN_ARRAYTYPE_P(result_array);
}
PG_FUNCTION_INFO_V1(rep_aggr_class_count_ffunc);

/*
 * We use a 15-element array to keep the state of the
 * aggregate for calculating splitting criteria values (SCVs).
 * The enum types defines which element of that array is used
 * for which purpose.
 */
enum SCV_STATE_ARRAY_INDEX
{
    /* the following two are used for discrete features */
    
    /* The value of one distinct feature we are processing */
    SCV_STATE_CURR_FEATURE_VALUE  =  0,
    /* 
     * Total number of elements equal to the value of
     * SCV_STATE_CURR_FEATURE_VALUE
     */ 
    SCV_STATE_CURR_FEATURE_ELEM_COUNT,

    /* the following two are used for continuous features */

    /* How many elements are less than or equal to the split value*/ 
    SCV_STATE_LESS_ELEM_COUNT,
    /* How many elements are greater than the split value*/ 
    SCV_STATE_GREAT_ELEM_COUNT,
    /* the following are used for both continuous and discrete features */

    /* Total count of records whose value is not null */
    SCV_STATE_TOTAL_ELEM_COUNT,

    /* It is used to store the accumulated entropy value */
    SCV_STATE_ENTROPY_DATA,
    /* It is used to store the accumulated split info value */
    SCV_STATE_SPLIT_INFO_DATA,
    /* It is used to store the accumulated gini value */
    SCV_STATE_GINI_DATA,
    /* 1 infogain, 2 gainratio, 3 gini */
    SCV_STATE_SPLIT_CRIT,
    /* whether the selected feature is continuous or discrete*/
    SCV_STATE_IS_CONT,
    /* init value of entropy/gini before split */
    SCV_STATE_INIT_IMPURITY_VAL,
    /* 
     * It specifies the total number of records in training set.
     * 
     * If there is missing values, the total count accumulated and 
     * stored in SCV_STATE_TOTAL_ELEM_COUNT are not equal to 
     * this element. In that case, our calculated gain should be 
     * discounted by the ratio of 
     * SCV_STATE_TOTAL_ELEM_COUNT/SCV_STATE_TRUE_TOTAL_COUNT.
     */
    SCV_STATE_TRUE_TOTAL_COUNT,
    /*
     * 1 We are computing initial entropy before split.
     * 0 We are computing the gain for certain split.
     */ 
    SCV_STATE_IS_CALC_PRE_SPLIT,
    /*
     * The ID of the class with the most elements.
     */ 
    SCV_STATE_MAX_CLASS_ID,
    /* The total number of elements belonging to MAX_CLASS */
    SCV_STATE_MAX_CLASS_ELEM_COUNT
};

/*
 * We use a 12-element array to keep the final result of the
 * aggregate for calculating splitting criteria values (SCVs).
 * The enum types defines which element of that array is used
 * for which purpose.
 */
enum SCV_FINAL_ARRAY_INDEX
{
    /* calculated entropy value */
    SCV_FINAL_ENTROPY = 0,
    /* calculated split info value */
    SCV_FINAL_SPLIT_INFO,
    /* calculated gini value */
    SCV_FINAL_GINI,
    /* 1 infogain, 2 gainratio, 3 gini */
    SCV_FINAL_SPLIT_CRITERION,
    /* entropy_before_split-SCV_FINAL_ENTROPY */
    SCV_FINAL_INFO_GAIN,
    /* SCV_FINAL_ENTROPY/SCV_FINAL_SPLIT_INFO */
    SCV_FINAL_GAIN_RATIO,
    /* gini_before_split- SCV_FINAL_GINI */
    SCV_FINAL_GINI_GAIN,
    /* whether the selected feature is continuous or discrete*/
    SCV_FINAL_IS_CONT_FEATURE,
    /*
     * 1 We are computing initial entropy before split.
     * 0 We are computing the gain for certain split.
     */     
    SCV_FINAL_CALC_PRE_SPLIT,
    /*
     * The ID of the class with the most elements.
     */     
    SCV_FINAL_CLASS_ID,
    /* The total number of elements belonging to MAX_CLASS */
    SCV_FINAL_CLASS_COUNT,
    /* Total count of records whose value is not null */
    SCV_FINAL_TOTAL_COUNT
};

/* The code for different split criteria. */
#define SC_INFOGAIN  1
#define SC_GAINRATIO 2
#define SC_GINI      3

/*
 * 	The step function for aggregating splitting criteria values based on our touched-up
 * 	(attribute, class) distribution information:
 *
 *	  fid | fval | class | iscont | splitvalue | less | great | selection
 *	-----+------+-------+--------+------------+------+-------+-----------
 *		 |      |       |        |            |   14 |       |         1
 *		 |      |     2 |        |            |    9 |       |         1
 *		 |      |     1 |        |            |    5 |       |         1
 *	   4 |    2 |       | f      |            |    6 |       |         1
 *	   4 |    2 |     2 | f      |            |    3 |       |         1
 *	   4 |    2 |     1 | f      |            |    3 |       |         1
 *	   4 |    1 |       | f      |            |    8 |       |         1
 *	   4 |    1 |     2 | f      |            |    6 |       |         1
 *	   4 |    1 |     1 | f      |            |    2 |       |         1
 *	   3 |    3 |       | f      |            |    5 |       |         1
 *	   3 |    3 |     2 | f      |            |    2 |       |         1
 *	   3 |    3 |     1 | f      |            |    3 |       |         1
 *	   3 |    2 |       | f      |            |    5 |       |         1
 *	   3 |    2 |     2 | f      |            |    3 |       |         1
 *	   3 |    2 |     1 | f      |            |    2 |       |         1
 *	   3 |    1 |       | f      |            |    4 |       |         1
 *	   3 |    1 |     2 | f      |            |    4 |       |         1
 *	   3 |    1 |     1 | f      |            |      |       |         1
 *	   2 |   96 |       | t      |         96 |   14 |       |         1
 *	   2 |   96 |     2 | t      |         96 |    9 |       |         1
 *	   2 |   96 |     1 | t      |         96 |    5 |       |         1
 *	   2 |   95 |       | t      |         95 |   13 |     1 |         1
 *	   2 |   95 |     2 | t      |         95 |    8 |     1 |         1
 *	   2 |   95 |     1 | t      |         95 |    5 |       |         1
 *
 * The rows are grouped by (selection, fid, splitvalue). For each group, we will
 * calculate a SCV based on the specified splitting criteria. For discrete attributes,
 * only less is used. Both less and great will be used for continuous values.
 *
 * Given the format of the input data stream, SCV calculation is different from using
 * the SCV formulas directly.
 *
 * For infogain and gainratio, there are (number_of_classes + 1) rows for
 * each distinct value of an attribute. The first row is the sub-total count (STC)
 * of cases for a (fval, class) pair. We will simply record it in the agg state.
 * For each of the successive rows in that group, we calculate the partial SVC with
 * this formula:
 *     (count) * log (STC/count)
 *
 * If TC is the total count of records.
 * Entropy is defined as (STC1/TC) * ( (count1/STC1)*log(STC1/count1) + (count2/STC1)*log(STC1/count2) )+
 * (STC2/TC) * ( (count3/STC2)*log(STC2/count3) + (count4/STC2)*log(STC2/count4) )= 
 * (count1*log(STC1/count1)+count2*log(STC1/count2)+count3*log(STC2/count3+count4*log(STC2/count4)/TC.
 *
 * As a result, in final function, we can calculate (accumulated value)/TC to get entropy. 
 *
 * For gini, it is very similar to infogain. The only difference is that we calculate the partial
 * SVC with the formula : (count*count)/STC. 
 *
 * Gini is defined as (STC1/TC) * ( 1- (count1/STC1)^2-(count2/STC1)^2) + 
 * (STC2/TC) * ( 1-(count3/STC2)^2-(count4/STC2)^2 ) = (STC1/TC + STC2/TC) - ( count1^2/STC1 + count2^2/STC1
 * + count3^2/STC2 + count4^2/STC2)/TC = 1- ( count1^2/STC1 + count2^2/STC1
 * + count3^2/STC2 + count4^2/STC2)/TC.
 *
 * As a result, in final function, we can calculate 1-(accumulated value)/TC to get gini. 
 *
 */
Datum scv_aggr_sfunc(PG_FUNCTION_ARGS)
{
    ArrayType*	scv_state_array	= PG_GETARG_ARRAYTYPE_P(0);
    if (!scv_state_array)
    	elog(ERROR, "Invalid aggregation state.");

    int	 array_dim 		= ARR_NDIM(scv_state_array);
    int* p_array_dim	= ARR_DIMS(scv_state_array);
    int  array_length	= ArrayGetNItems(array_dim, p_array_dim);
    if (array_length != SCV_STATE_MAX_CLASS_ELEM_COUNT + 1)
        elog(ERROR, "array_length:%d",array_length);

    float8 *scv_state_data = (float8 *)ARR_DATA_PTR(scv_state_array);
    if (!scv_state_data)
    	elog(ERROR, "Invalid aggregation data array.");

    int    split_criterion		= PG_GETARG_INT32(1);
    float8 feature_val			= PG_GETARG_FLOAT8(2);
    float8 class				= PG_ARGISNULL(3) ? -1 : PG_GETARG_FLOAT8(3);
    bool   is_cont_feature 		= PG_ARGISNULL(4) ? false : PG_GETARG_BOOL(4);
    float8 less					= PG_ARGISNULL(5) ? 0 : PG_GETARG_FLOAT8(5);
    float8 great				= PG_ARGISNULL(6) ? 0 : PG_GETARG_FLOAT8(6);
    float8 init_impurity_val 	= PG_ARGISNULL(7) ? 0 : PG_GETARG_FLOAT8(7);
    float8 true_total_count 	= PG_ARGISNULL(8) ? 0 : PG_GETARG_FLOAT8(8);

    Assert(SC_INFOGAIN  == split_criterion ||
    	   SC_GAINRATIO == split_criterion ||
    	   SC_GINI      == split_criterion
    	  );

    /*
     *  If the count for total element is still zero
     *  it is the first time that step function is
     *  invoked. In that case, we should initialize
     *  several elements.
     */ 
    if (is_float_zero(scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT]))
    {
        scv_state_data[SCV_STATE_SPLIT_CRIT] 		= split_criterion;
        scv_state_data[SCV_STATE_INIT_IMPURITY_VAL] = init_impurity_val;
        scv_state_data[SCV_STATE_IS_CONT] 			= is_cont_feature ? 1 : 0;
        scv_state_data[SCV_STATE_TRUE_TOTAL_COUNT] 	= true_total_count;
        
        /*
         * If feature value is null, we are calculating the entropy/gini 
         * before split. Otherwise, we are calculating the entropy/gini 
         * for certain split.
         */ 
        scv_state_data[SCV_STATE_IS_CALC_PRE_SPLIT] = PG_ARGISNULL(2) ? 1 : 0;
    }

    float8 temp_float = 0;

    /*
     * For the current input row, if the class column is NULL,
     * the variable class will be assigned -1
     */
    if (class < 0)
    {
    	/* a -1 means the current input row contains the
    	 * total number of (attribute, class) pairs
    	 */
        if (!is_cont_feature)
        {
            /* This block calculates for discrete features, which only use the column of less. */
            scv_state_data[SCV_STATE_CURR_FEATURE_VALUE] 		= feature_val;
            scv_state_data[SCV_STATE_CURR_FEATURE_ELEM_COUNT]	= less;

            dtelog(NOTICE,"feature_val:%lf,feature_elem_count:%lf", feature_val, less);

            if (SC_GAINRATIO == split_criterion)
            {
                temp_float = scv_state_data[SCV_STATE_CURR_FEATURE_ELEM_COUNT];
                if (! is_float_zero(temp_float))
                    scv_state_data[SCV_STATE_SPLIT_INFO_DATA] += temp_float*log(temp_float);
            }

            dtelog(NOTICE, "scv_aggr_sfunc before SCV_STATE_TOTAL_ELEM_COUNT:%lf",scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT]);

            scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT] += scv_state_data[SCV_STATE_CURR_FEATURE_ELEM_COUNT];

            dtelog(NOTICE, "scv_aggr_sfunc after SCV_STATE_TOTAL_ELEM_COUNT:%lf",scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT]);
        }
        else
        {
            /* This block calculates for continuous features, which the columns of less/great. */
            scv_state_data[SCV_STATE_LESS_ELEM_COUNT]	= less;
            scv_state_data[SCV_STATE_GREAT_ELEM_COUNT]	= great;

            if (SC_GAINRATIO == split_criterion)
            {
                for (int index=SCV_STATE_LESS_ELEM_COUNT; index <=SCV_STATE_GREAT_ELEM_COUNT; index++)
                {
                    temp_float = scv_state_data[index];
                    if (! is_float_zero(temp_float))
                    {
                        scv_state_data[SCV_STATE_SPLIT_INFO_DATA] += temp_float * log(temp_float);
                    }
                }
            }
            scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT] += scv_state_data[SCV_STATE_LESS_ELEM_COUNT] + scv_state_data[SCV_STATE_GREAT_ELEM_COUNT];
        }
    }
    else
    {
        /* If the class exists, we start to accumulate the value for entropy/gini */
        if (scv_state_data[SCV_STATE_IS_CALC_PRE_SPLIT] > 0)
        {
            /* 
             *  For pre-split calculation, we need to find MAX class. 
             *  Otherwise, we are unable to locate max class. We have
             *  already initialized them to zero.
             */
            if (scv_state_data[SCV_STATE_MAX_CLASS_ELEM_COUNT] < less + great)
            {
                scv_state_data[SCV_STATE_MAX_CLASS_ELEM_COUNT] = less + great;
                scv_state_data[SCV_STATE_MAX_CLASS_ID] = class;
            }
        }

        if (!is_cont_feature)
        {
            /* This block accumulates entropy/gini for discrete features.*/
            if (SC_GAINRATIO == split_criterion ||
                SC_INFOGAIN  == split_criterion)
            {
                if (! is_float_zero(less - scv_state_data[SCV_STATE_CURR_FEATURE_ELEM_COUNT])
                        && less > 0
                        && scv_state_data[SCV_STATE_CURR_FEATURE_ELEM_COUNT] > 0)
                    scv_state_data[SCV_STATE_ENTROPY_DATA] +=
                    		less * log(scv_state_data[SCV_STATE_CURR_FEATURE_ELEM_COUNT]/less);
            }
            else
            if (SC_GINI == split_criterion)
            {
                if (scv_state_data[SCV_STATE_CURR_FEATURE_ELEM_COUNT] > 0)
                    scv_state_data[SCV_STATE_GINI_DATA] +=
                    		less * less / scv_state_data[SCV_STATE_CURR_FEATURE_ELEM_COUNT];
            }
        }
        else
        {
            /* This block accumulates entropy/gini for continuous features.*/
            float8 temp_array[]	 = {less, great};
            float8 count_array[] = {scv_state_data[SCV_STATE_LESS_ELEM_COUNT],
            						scv_state_data[SCV_STATE_GREAT_ELEM_COUNT]};

            for(int index =0; index <2; index++)
            {
                temp_float = temp_array[index];

                if (SC_GAINRATIO == split_criterion ||
                    SC_INFOGAIN  == split_criterion)
                {
                    if (!is_float_zero(temp_float - count_array[index]) &&
                        temp_float > 0									&&
                        count_array[index] > 0)
                        scv_state_data[SCV_STATE_ENTROPY_DATA] += temp_float * log(count_array[index]/temp_float);
                }
                else
                if (SC_GINI == split_criterion)
                {
                    if (count_array[index] > 0)
                        scv_state_data[SCV_STATE_GINI_DATA] += temp_float * temp_float / count_array[index];
                }
            }
        }
    }

    PG_RETURN_ARRAYTYPE_P(scv_state_array);
}
PG_FUNCTION_INFO_V1(scv_aggr_sfunc);


Datum scv_aggr_prefunc(PG_FUNCTION_ARGS)
{
    ArrayType *scv_state_array  	= PG_GETARG_ARRAYTYPE_P(0);
    if (!scv_state_array)
    	elog(ERROR, "Invalid aggregation state.");

    int array_dim 		= ARR_NDIM(scv_state_array);
    int *p_array_dim 	= ARR_DIMS(scv_state_array);
    int array_length 	= ArrayGetNItems(array_dim, p_array_dim);
    if (array_length != SCV_STATE_MAX_CLASS_ELEM_COUNT+1)
        elog(WARNING, "scv_aggr_prefunc array_length:%d",array_length);

    /* the scv state data from a segment */
    float8 *scv_state_data = (float8 *)ARR_DATA_PTR(scv_state_array);
    if (!scv_state_data)
    	elog(ERROR, "Invalid aggregation data array.");

    ArrayType* scv_state_array2	= PG_GETARG_ARRAYTYPE_P(1);
    if (!scv_state_array2)
    	elog(ERROR, "Invalid aggregation state.");

    array_dim 		= ARR_NDIM(scv_state_array2);
    p_array_dim 	= ARR_DIMS(scv_state_array2);
    array_length 	= ArrayGetNItems(array_dim, p_array_dim);
    if (array_length != SCV_STATE_MAX_CLASS_ELEM_COUNT+1)
        elog(WARNING, "scv_aggr_prefunc array_length:%d",array_length);

    /* the scv state data from another segment */
    float8 *scv_state_data2 = (float8 *)ARR_DATA_PTR(scv_state_array2);
    if (!scv_state_data2)
    	elog(ERROR, "Invalid aggregation data array.");

    /*
     * For the following data, such as entropy, gini and split info, we need to combine
     * the accumulated value from multiple segments.
     */ 
    scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT]	+= scv_state_data2[SCV_STATE_TOTAL_ELEM_COUNT];
    scv_state_data[SCV_STATE_ENTROPY_DATA] 		+= scv_state_data2[SCV_STATE_ENTROPY_DATA];
    scv_state_data[SCV_STATE_SPLIT_INFO_DATA]	+= scv_state_data2[SCV_STATE_SPLIT_INFO_DATA];
    scv_state_data[SCV_STATE_GINI_DATA]			+= scv_state_data2[SCV_STATE_GINI_DATA];

    /*
     *  The following elements are just initialized once. If the first scv_state is not initialized,
     *  we copy them from the second scv_state.
     */ 
    if (is_float_zero(scv_state_data[SCV_STATE_SPLIT_CRIT]))
    {
        scv_state_data[SCV_STATE_SPLIT_CRIT]		= scv_state_data2[SCV_STATE_SPLIT_CRIT];
        scv_state_data[SCV_STATE_TRUE_TOTAL_COUNT]	= scv_state_data2[SCV_STATE_TRUE_TOTAL_COUNT];
        scv_state_data[SCV_STATE_INIT_IMPURITY_VAL] = scv_state_data2[SCV_STATE_INIT_IMPURITY_VAL];
        scv_state_data[SCV_STATE_IS_CONT]			= scv_state_data2[SCV_STATE_IS_CONT];
        scv_state_data[SCV_STATE_IS_CALC_PRE_SPLIT] = scv_state_data2[SCV_STATE_IS_CALC_PRE_SPLIT];
    }
    
    /*
     *  We should compare the results from different segments and find the class with maximum cases.
     */ 
    if (scv_state_data[SCV_STATE_MAX_CLASS_ELEM_COUNT] < scv_state_data2[SCV_STATE_MAX_CLASS_ELEM_COUNT])
    {
        scv_state_data[SCV_STATE_MAX_CLASS_ELEM_COUNT]	= scv_state_data2[SCV_STATE_MAX_CLASS_ELEM_COUNT];
        scv_state_data[SCV_STATE_MAX_CLASS_ID]			= scv_state_data2[SCV_STATE_MAX_CLASS_ID];
    }

    PG_RETURN_ARRAYTYPE_P(scv_state_array);
}
PG_FUNCTION_INFO_V1(scv_aggr_prefunc);


Datum scv_aggr_ffunc(PG_FUNCTION_ARGS)
{
    ArrayType*	scv_state_array	= PG_GETARG_ARRAYTYPE_P(0);
    if (!scv_state_array)
    	elog(ERROR, "Invalid aggregation state.");

    int	 array_dim		= ARR_NDIM(scv_state_array);
    int* p_array_dim 	= ARR_DIMS(scv_state_array);
    int  array_length 	= ArrayGetNItems(array_dim, p_array_dim);

    if (array_length != SCV_STATE_MAX_CLASS_ELEM_COUNT+1)
        elog(ERROR, "Bad array length:%d",array_length);

    dtelog(NOTICE, "scv_aggr_ffunc array_length:%d",array_length);

    float8 *scv_state_data = (float8 *)ARR_DATA_PTR(scv_state_array);
    if (!scv_state_data)
    	elog(ERROR, "Invalid aggregation data array.");

    float8 init_impurity_val = scv_state_data[SCV_STATE_INIT_IMPURITY_VAL];

    int result_size = 12;
    float8 *result = palloc(sizeof(float8) * result_size);
    if (!result)
    	elog(ERROR, "Memory allocation failure.");

    memset(result, 0, sizeof(float8) * result_size);

    dtelog(NOTICE, "scv_aggr_ffunc SCV_STATE_TOTAL_ELEM_COUNT:%lf",scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT]);

    /* 
     * For the following elements, such as max class id, we should copy them from step function array
     * to final function array for returning.
     */
    result[SCV_FINAL_SPLIT_CRITERION]   = scv_state_data[SCV_STATE_SPLIT_CRIT];
    result[SCV_FINAL_IS_CONT_FEATURE]	= scv_state_data[SCV_STATE_IS_CONT];
    result[SCV_FINAL_CALC_PRE_SPLIT]	= scv_state_data[SCV_STATE_IS_CALC_PRE_SPLIT];
    result[SCV_FINAL_CLASS_ID]			= scv_state_data[SCV_STATE_MAX_CLASS_ID];
    result[SCV_FINAL_CLASS_COUNT]		= scv_state_data[SCV_STATE_MAX_CLASS_ELEM_COUNT];
    result[SCV_FINAL_TOTAL_COUNT]		= scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT];

    float8 ratio = 1;
    
    /* If there is any missing value, we should multiply a ratio for the computed gain. */
    if (!is_float_zero(scv_state_data[SCV_STATE_TRUE_TOTAL_COUNT]))
        ratio = scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT]/scv_state_data[SCV_STATE_TRUE_TOTAL_COUNT];

    if (!is_float_zero(scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT]))
    {
        if (SC_INFOGAIN  == (int)result[SCV_FINAL_SPLIT_CRITERION] ||
            SC_GAINRATIO == (int)result[SCV_FINAL_SPLIT_CRITERION])
        {
            /* Get the entropy value */
            result[SCV_FINAL_ENTROPY]   = scv_state_data[SCV_STATE_ENTROPY_DATA]/scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT];
            /* Get infogain based on initial entropy and current one */
            result[SCV_FINAL_INFO_GAIN] = (init_impurity_val - result[SCV_FINAL_ENTROPY]) * ratio;

            if (SC_GAINRATIO == (int)result[SCV_FINAL_SPLIT_CRITERION])
            {
                /* Compute the value of split info */
                result[SCV_FINAL_SPLIT_INFO] =
                        log(scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT]) -
                        scv_state_data[SCV_STATE_SPLIT_INFO_DATA] / scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT];

                /* Based on infogain and split info, we can compute gainratio. */
                if (!is_float_zero(result[SCV_FINAL_SPLIT_INFO]))
                    result[SCV_FINAL_GAIN_RATIO] = result[SCV_FINAL_INFO_GAIN] / result[SCV_FINAL_SPLIT_INFO];
                else
                    result[SCV_FINAL_GAIN_RATIO] = 0;
            }
        }
        else if (SC_GINI == (int)result[SCV_FINAL_SPLIT_CRITERION])
        {
            /* Get the value of gini */
            result[SCV_FINAL_GINI]      = 1 - scv_state_data[SCV_STATE_GINI_DATA] / scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT];

            /* Get gain based on initial gini and current gini value */
            result[SCV_FINAL_GINI_GAIN] = (init_impurity_val - result[SCV_FINAL_GINI]) * ratio;
        }
        else
            elog(ERROR,"Bad split criteria : %d", (int)result[SCV_FINAL_SPLIT_CRITERION]);
    }
    else
        elog(ERROR,"Bad number of total element counts : %lld", (int64)scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT]);

    ArrayType* result_array =
        construct_array(
            (Datum *)result,
            result_size,
            FLOAT8OID,
            sizeof(float8),
            true,
            'd'
            );
    if (!result_array)
        elog(ERROR, "Array construction failure.");

    PG_RETURN_ARRAYTYPE_P(result_array);
}
PG_FUNCTION_INFO_V1(scv_aggr_ffunc);