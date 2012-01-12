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
#include "nodes/execnodes.h"
#include "nodes/nodes.h"

#ifndef NO_PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

#ifdef __DT_SHOW_DEBUG_INFO__
#define dtelog(...) elog(__VA_ARGS__)
#else
#define dtelog(...)
#endif

/*
 * Postgres8.4 doesn't have such macro, so we add here
 */
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))

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
static float8 CONFIDENCE_LEVEL[] = {0, 0.001, 0.005, 0.01, 0.05, 0.10, 0.20, 0.40, 1.00};
static float8 CONFIDENCE_DEV[]   = {4.0, 3.09, 2.58, 2.33, 1.65, 1.28, 0.84, 0.25, 0.00};

#define MIN_CONFIDENCE_LEVEL 0.001L
#define MAX_CONFIDENCE_LEVEL 100.0L

#define check_error_value(condition, message, value) \
			if (!(condition)) \
				ereport(ERROR, \
						(errcode(ERRCODE_RAISE_EXCEPTION), \
						 errmsg(message, (value)) \
						) \
					   );

#define check_error(condition, message) \
			if (!(condition)) \
				ereport(ERROR, \
						(errcode(ERRCODE_RAISE_EXCEPTION), \
						 errmsg(message) \
						) \
					   );

float8 ebp_calc_errors_internal
(
	float8 total_cases,
	float8 num_errors,
	float8 conf_level,
	float8 coeff
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
Datum ebp_calc_errors(PG_FUNCTION_ARGS)
{
    float8 total_cases 	= PG_GETARG_FLOAT8(0);
    float8 probability 	= PG_GETARG_FLOAT8(1);
    float8 conf_level 	= PG_GETARG_FLOAT8(2);
    float8 result 		= 1.0L;
    float8 coeff 		= 0.0L;
    unsigned int i 		= 0;

    if (!is_float_zero(100 - conf_level))
    {
    	check_error_value
    		(
    			!(conf_level < MIN_CONFIDENCE_LEVEL || conf_level > MAX_CONFIDENCE_LEVEL),
    			"invalid confidence level:  %lf. Confidence level must be in range from 0.001 to 100",
    			conf_level
    		);

    	check_error_value
    		(
    			total_cases > 0,
    			"invalid number: %lf. The number of cases must be greater than 0",
    			total_cases
    		);

    	check_error_value
    		(
    			!(probability < 0 || probability > 1),
    			"invalid probability: %lf. The probability must be in range from 0 to 1",
    			probability
    		);

    	/*
    	 * confidence level value is in range from 0.001 to 1.0 for API c45_train
    	 * it should be divided by 100 when calculate addition error, therefore,
    	 * the range of conf_level here is [0.00001, 1.0].
    	 */
    	conf_level = conf_level * 0.01;

		/* since the conf_level is in [0.00001, 1.0], the i will be in [1, length(CONFIDENCE_LEVEL) - 1]*/
		while (conf_level > CONFIDENCE_LEVEL[i]) i++;

    	check_error_value
    		(
    			i > 0 && i < ARRAY_SIZE(CONFIDENCE_LEVEL),
    			"invalid value: %d. The index of confidence level must be in range from 0 to 8",
    			i
    		);

		coeff = CONFIDENCE_DEV[i-1] +
				(CONFIDENCE_DEV[i] - CONFIDENCE_DEV[i-1]) *
				(conf_level - CONFIDENCE_LEVEL[i-1]) /
				(CONFIDENCE_LEVEL[i] - CONFIDENCE_LEVEL[i-1]);

		coeff *= coeff;

		check_error_value
    		(
    			coeff > 0,
    			"invalid coefficiency: %lf. It must be greater than 0",
    			coeff
    		);

		float8 num_errors = total_cases * (1 - probability);
    	result = ebp_calc_errors_internal(total_cases, num_errors, conf_level, coeff) + num_errors;
    }

	PG_RETURN_FLOAT8((float8)result);
}
PG_FUNCTION_INFO_V1(ebp_calc_errors);

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
float8 ebp_calc_errors_internal
    (
	float8 total_cases,
	float8 num_errors,
	float8 conf_level,
	float8 coeff
    )
{
    if (num_errors < 1E-6)
    {
        return total_cases * (1 - exp(log(conf_level) / total_cases));
    }
    else
    if (num_errors < 0.9999)
    {
        float8 tmp = total_cases * (1 - exp(log(conf_level) / total_cases));
        return tmp + num_errors * 
            (ebp_calc_errors_internal(total_cases, 1.0, conf_level, coeff) - tmp);
    }
    else
    if (num_errors + 0.5 >= total_cases)
    {
        return 0.67 * (total_cases - num_errors);
    }
    else
    {
        float8 tmp =
			(
			num_errors + 0.5 + coeff/2 +
			sqrt(coeff * ((num_errors + 0.5) * (1 - (num_errors + 0.5)/total_cases) + coeff/4))
			)
			/ (total_cases + coeff);

        return (total_cases * tmp - num_errors);
    }
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
    bool need_reconstruct_array     = false;


    check_error_value
		(
			max_num_of_classes >= 2,
			"invalid value: %d. The number of classes must be greater than or equal to 2",
			max_num_of_classes
		);

    check_error_value
		(
			original_class > 0 && original_class <= max_num_of_classes,
			"invalid real class value: %d. It must be in range from 1 to the number of classes",
			original_class
		);

    check_error_value
		(
			classified_class > 0 && classified_class <= max_num_of_classes,
			"invalid classified class value: %d. It must be in range from 1 to the number of classes",
			classified_class
		);
    
    /* test if the first argument (class count array) is null */
    if (PG_ARGISNULL(0))
    {
    	/*
    	 * We assume the maximum number of classes is limited (up to millions),
    	 * so that the allocated array won't break our memory limitation.
    	 */
        class_count_data		= palloc0(sizeof(int64) * (max_num_of_classes + 1));
        array_length 			= max_num_of_classes + 1;
        need_reconstruct_array 	= true;

    }
    else
    {
        if (fcinfo->context && IsA(fcinfo->context, AggState))
            class_count_array = PG_GETARG_ARRAYTYPE_P(0);
        else
            class_count_array = PG_GETARG_ARRAYTYPE_P_COPY(0);
        
        check_error
    		(
    			class_count_array,
    			"invalid aggregation state array"
    		);

        array_dim = ARR_NDIM(class_count_array);

        check_error_value
    		(
    			array_dim == 1,
    			"invalid array dimension: %d. The dimension of class count array must be equal to 1",
    			array_dim
    		);

        p_array_dim         = ARR_DIMS(class_count_array);
        array_length        = ArrayGetNItems(array_dim,p_array_dim);
        class_count_data    = (int64 *)ARR_DATA_PTR(class_count_array);

        check_error_value
    		(
    			array_length == max_num_of_classes + 1,
    			"invalid array length: %d. The length of class count array must be equal to the total number classes + 1",
    			array_length
    		);
    }

    /*
     * If the condition is met, then the current record has been mis-classified.
     * Therefore, we will need to increase the first element.
     */
    if(original_class != classified_class)
        ++class_count_data[0];

    /* In any case, we will update the original class count */
    ++class_count_data[original_class];

    if( need_reconstruct_array )
    {
        /* construct a new array to keep the aggr states. */
        class_count_array =
        	construct_array(
        		(Datum *)class_count_data,
                array_length,
                INT8OID,
                sizeof(int64),
                true,
                'd'
                );

    }
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
        if (fcinfo->context && IsA(fcinfo->context, AggState))
            class_count_array = PG_GETARG_ARRAYTYPE_P(0);
        else
            class_count_array = PG_GETARG_ARRAYTYPE_P_COPY(0);
        
        check_error
    		(
    			class_count_array,
    			"invalid aggregation state array"
    		);

        array_dim = ARR_NDIM(class_count_array);

        check_error_value
    		(
    			array_dim == 1,
    			"invalid array dimension: %d. The dimension of class count array must be equal to 1",
    			array_dim
    		);

        p_array_dim             = ARR_DIMS(class_count_array);
        array_length            = ArrayGetNItems(array_dim,p_array_dim);
        class_count_data        = (int64 *)ARR_DATA_PTR(class_count_array);

        class_count_array2      = PG_GETARG_ARRAYTYPE_P(1);
        array_dim2              = ARR_NDIM(class_count_array2);
        check_error_value
    		(
    			array_dim2 == 1,
    			"invalid array dimension: %d. The dimension of class count array must be equal to 1",
    			array_dim2
    		);

        p_array_dim2            = ARR_DIMS(class_count_array2);
        array_length2           = ArrayGetNItems(array_dim2,p_array_dim2);
        class_count_data2       = (int64 *)ARR_DATA_PTR(class_count_array2);

        check_error
    		(
    			array_length == array_length2,
    			"the size of the two array must be the same in prefunction"
    		);

        for (int index = 0; index < array_length; index++)
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

    check_error_value
		(
			array_dim == 1,
			"invalid array dimension: %d. The dimension of class count array must be equal to 1",
			array_dim
		);

    int *p_array_dim                = ARR_DIMS(class_count_array);
    int array_length                = ArrayGetNItems(array_dim,p_array_dim);
    int64 *class_count_data         = (int64 *)ARR_DATA_PTR(class_count_array);
    int64 *result                   = palloc(sizeof(int64)*2);

    check_error
		(
			result,
			"memory allocation failure"
		);

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
         INT8OID,
         sizeof(int64),
         true,
         'd'
        );

    PG_RETURN_ARRAYTYPE_P(result_array);
}
PG_FUNCTION_INFO_V1(rep_aggr_class_count_ffunc);

/*
 * We use a 14-element array to keep the state of the
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
    SCV_STATE_INIT_SCV,
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
     * The ID of the class with the most elements.
     */ 
    SCV_STATE_MAX_CLASS_ID,
    /* The total number of elements belonging to MAX_CLASS */
    SCV_STATE_MAX_CLASS_ELEM_COUNT
};

/*
 * We use a 11-element array to keep the final result of the
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
 *  The following table contains the original data used for the first  
 *  calculation of golf dataset.
 *
 *  fid | fval | class | is_cont | split_value | le | gt | assigned_nid 
 * -----+------+-------+---------+-------------+----+----+--------------
 *      |      |       |         |             | 14 |    |            1
 *      |      |     2 |         |             |  9 |    |            1
 *      |      |     1 |         |             |  5 |    |            1
 *    4 |      |       |         |             | 14 |    |            1
 *    4 |      |     2 |         |             |  9 |    |            1
 *    4 |      |     1 |         |             |  5 |    |            1
 *    4 |    2 |       | f       |             |  6 |    |            1
 *    4 |    2 |     2 | f       |             |  3 |    |            1
 *    4 |    2 |     1 | f       |             |  3 |    |            1
 *    4 |    1 |       | f       |             |  8 |    |            1
 *    4 |    1 |     2 | f       |             |  6 |    |            1
 *    4 |    1 |     1 | f       |             |  2 |    |            1
 *    3 |      |       |         |             | 14 |    |            1
 *    3 |      |     2 |         |             |  9 |    |            1
 *    3 |      |     1 |         |             |  5 |    |            1
 *    3 |    3 |       | f       |             |  5 |    |            1
 *    3 |    3 |     2 | f       |             |  2 |    |            1
 *    3 |    3 |     1 | f       |             |  3 |    |            1
 *    3 |    2 |       | f       |             |  5 |    |            1
 *    3 |    2 |     2 | f       |             |  3 |    |            1
 *    3 |    2 |     1 | f       |             |  2 |    |            1
 *    3 |    1 |       | f       |             |  4 |    |            1
 *    3 |    1 |     2 | f       |             |  4 |    |            1
 *    3 |    1 |     1 | f       |             |    |    |            1
 *    2 |   96 |       | t       |          96 | 14 |    |            1
 *    2 |   96 |     2 | t       |          96 |  9 |    |            1
 *    2 |   96 |     1 | t       |          96 |  5 |    |            1
 *    2 |   95 |       | t       |          95 | 13 |  1 |            1
 *    2 |   95 |     2 | t       |          95 |  8 |  1 |            1
 *    2 |   95 |     1 | t       |          95 |  5 |    |            1
 *    2 |   90 |       | t       |          90 | 12 |  2 |            1
 *    2 |   90 |     2 | t       |          90 |  8 |  1 |            1
 *    2 |   90 |     1 | t       |          90 |  4 |  1 |            1
 *    2 |   85 |       | t       |          85 | 10 |  4 |            1
 *    2 |   85 |     2 | t       |          85 |  7 |  2 |            1
 *    2 |   85 |     1 | t       |          85 |  3 |  2 |            1
 *    2 |   80 |       | t       |          80 |  9 |  5 |            1
 *    2 |   80 |     2 | t       |          80 |  7 |  2 |            1
 *    2 |   80 |     1 | t       |          80 |  2 |  3 |            1
 *    2 |   78 |       | t       |          78 |  6 |  8 |            1
 *    2 |   78 |     2 | t       |          78 |  5 |  4 |            1
 *    2 |   78 |     1 | t       |          78 |  1 |  4 |            1
 *    2 |   75 |       | t       |          75 |  5 |  9 |            1
 *    2 |   75 |     2 | t       |          75 |  4 |  5 |            1
 *    2 |   75 |     1 | t       |          75 |  1 |  4 |            1
 *    2 |   70 |       | t       |          70 |  4 | 10 |            1
 *    2 |   70 |     2 | t       |          70 |  3 |  6 |            1
 *    2 |   70 |     1 | t       |          70 |  1 |  4 |            1
 *    2 |   65 |       | t       |          65 |  1 | 13 |            1
 *    2 |   65 |     2 | t       |          65 |  1 |  8 |            1
 *    2 |   65 |     1 | t       |          65 |    |  5 |            1
 *    1 |   85 |       | t       |          85 | 14 |    |            1
 *    1 |   85 |     2 | t       |          85 |  9 |    |            1
 *    1 |   85 |     1 | t       |          85 |  5 |    |            1
 *    1 |   83 |       | t       |          83 | 13 |  1 |            1
 *    1 |   83 |     2 | t       |          83 |  9 |    |            1
 *    1 |   83 |     1 | t       |          83 |  4 |  1 |            1
 *    1 |   81 |       | t       |          81 | 12 |  2 |            1
 *    1 |   81 |     2 | t       |          81 |  8 |  1 |            1
 *    1 |   81 |     1 | t       |          81 |  4 |  1 |            1
 *    1 |   80 |       | t       |          80 | 11 |  3 |            1
 *    1 |   80 |     2 | t       |          80 |  7 |  2 |            1
 *    1 |   80 |     1 | t       |          80 |  4 |  1 |            1
 *    1 |   75 |       | t       |          75 | 10 |  4 |            1
 *    1 |   75 |     2 | t       |          75 |  7 |  2 |            1
 *    1 |   75 |     1 | t       |          75 |  3 |  2 |            1
 *    1 |   72 |       | t       |          72 |  8 |  6 |            1
 *    1 |   72 |     2 | t       |          72 |  5 |  4 |            1
 *    1 |   72 |     1 | t       |          72 |  3 |  2 |            1
 *    1 |   71 |       | t       |          71 |  6 |  8 |            1
 *    1 |   71 |     2 | t       |          71 |  4 |  5 |            1
 *    1 |   71 |     1 | t       |          71 |  2 |  3 |            1
 *    1 |   70 |       | t       |          70 |  5 |  9 |            1
 *    1 |   70 |     2 | t       |          70 |  4 |  5 |            1
 *    1 |   70 |     1 | t       |          70 |  1 |  4 |            1
 *    1 |   69 |       | t       |          69 |  4 | 10 |            1
 *    1 |   69 |     2 | t       |          69 |  3 |  6 |            1
 *    1 |   69 |     1 | t       |          69 |  1 |  4 |            1
 *    1 |   68 |       | t       |          68 |  3 | 11 |            1
 *    1 |   68 |     2 | t       |          68 |  2 |  7 |            1
 *    1 |   68 |     1 | t       |          68 |  1 |  4 |            1
 *    1 |   65 |       | t       |          65 |  2 | 12 |            1
 *    1 |   65 |     2 | t       |          65 |  1 |  8 |            1
 *    1 |   65 |     1 | t       |          65 |  1 |  4 |            1
 *    1 |   64 |       | t       |          64 |  1 | 13 |            1
 *    1 |   64 |     2 | t       |          64 |  1 |  8 |            1
 *    1 |   64 |     1 | t       |          64 |    |  5 |            1
 * (87 rows)
 * 

 *
 * The rows are grouped by (assigned_nid, fid, splitvalue). For each group, we will
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

/*
 * The function is used to calculate pre-split splitting criteria value. 
 *
 * Parameters:
 *      scv_state_array     The array containing all the information for the 
 *                          calculation of splitting criteria values. 
 *      curr_class_count    Total count of elements belonging to current class.
 *      total_elem_count    Total count of elements.
 *      split_criterion     1- infogain; 2- gainratio; 3- gini.
 */
static void accumulate_pre_split_scv
    (
    float8*     scv_state_array,
    float8      curr_class_count,
    float8      total_elem_count,
    int         split_criterion
    )
{

	check_error
		(
			scv_state_array,
			"invalid aggregation state array"
		);

	check_error_value
		(
			(SC_INFOGAIN  == split_criterion ||
			SC_GAINRATIO  == split_criterion ||
			SC_GINI       == split_criterion),
			"invalid split criterion: %d. It must be 1(infogain), 2(gainratio) or 3(gini)",
			split_criterion
		);

	check_error_value
		(
			total_elem_count > 0,
			"invalid value: %lf. The total element count must be greater than 0",
			total_elem_count
		);

	check_error_value
		(
			curr_class_count >= 0,
			"invalid value: %lf. The current class count must be greater than or equal to 0",
			curr_class_count
		);

    float8 temp_float = curr_class_count / total_elem_count;

    if (SC_INFOGAIN  == split_criterion ||
        SC_GAINRATIO == split_criterion )
    {
        if (temp_float>0)
            temp_float = temp_float*log(1/temp_float);
        else
            temp_float = 0;
        scv_state_array[SCV_STATE_INIT_SCV] += temp_float;
    }
    else
    {
        temp_float *= temp_float;
        scv_state_array[SCV_STATE_INIT_SCV] -= temp_float;
    }
}

/*
 * The step function for the aggregation of splitting criteria values. It accumulates
 * all the information for scv calculation and stores to a fourteen element array.  
 *
 * Parameters:
 *      scv_state_array    The array used to accumulate all the information for the 
 *                         calculation of splitting criteria values. Please refer to
 *                         the definition of SCV_STATE_ARRAY_INDEX.
 *      split_criterion    1- infogain; 2- gainratio; 3- gini.
 *      feature_val        The feature value of current record under processing.
 *      class              The class of current record under processing.
 *      is_cont_feature    True- The feature is continuous. False- The feature is
 *                         discrete.
 *      less               Count of elements less than or equal to feature_val.
 *      great              Count of elements greater than feature_val.
 *      true_total_count   If there is any missing value, true_total_count is larger than
 *                         the total count computed in the aggregation. Thus, we should  
 *                         multiply a ratio for the computed gain.
 * Return:
 *      A fourteen element array. Please refer to the definition of 
 *      SCV_STATE_ARRAY_INDEX for the detailed information of this
 *      array.
 */
Datum scv_aggr_sfunc(PG_FUNCTION_ARGS)
{
    ArrayType*	scv_state_array	= NULL;
    if (fcinfo->context && IsA(fcinfo->context, AggState))
        scv_state_array = PG_GETARG_ARRAYTYPE_P(0);
    else
        scv_state_array = PG_GETARG_ARRAYTYPE_P_COPY(0);

	check_error
		(
			scv_state_array,
			"invalid aggregation state array"
		);

    int	 array_dim 		= ARR_NDIM(scv_state_array);

    check_error_value
		(
			array_dim == 1,
			"invalid array dimension: %d. The dimension of scv state array must be equal to 1",
			array_dim
		);

    int* p_array_dim	= ARR_DIMS(scv_state_array);
    int  array_length	= ArrayGetNItems(array_dim, p_array_dim);

    check_error_value
		(
			array_length == SCV_STATE_MAX_CLASS_ELEM_COUNT + 1,
			"invalid array length: %d",
			array_length
		);

    float8 *scv_state_data = (float8 *)ARR_DATA_PTR(scv_state_array);
	check_error
		(
			scv_state_data,
			"invalid aggregation data array"
		);

    int    split_criterion		= PG_GETARG_INT32(1);
    bool   is_null_fval         = PG_ARGISNULL(2);
    float8 feature_val			= PG_GETARG_FLOAT8(2);
    float8 class				= PG_ARGISNULL(3) ? -1 : PG_GETARG_FLOAT8(3);
    bool   is_cont_feature 		= PG_ARGISNULL(4) ? false : PG_GETARG_BOOL(4);
    float8 less					= PG_ARGISNULL(5) ? 0 : PG_GETARG_FLOAT8(5);
    float8 great				= PG_ARGISNULL(6) ? 0 : PG_GETARG_FLOAT8(6);
    float8 true_total_count 	= PG_ARGISNULL(7) ? 0 : PG_GETARG_FLOAT8(7);

	check_error_value
		(
			(SC_INFOGAIN  == split_criterion ||
			SC_GAINRATIO  == split_criterion ||
			SC_GINI       == split_criterion),
			"invalid split criterion: %d. It must be 1(infogain), 2(gainratio) or 3(gini)",
			split_criterion
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
        if (SC_GINI      == split_criterion)
        {
            scv_state_data[SCV_STATE_INIT_SCV] 	    = 1;
        }
        else
        {
            scv_state_data[SCV_STATE_INIT_SCV] 	    = 0;
        }
        scv_state_data[SCV_STATE_IS_CONT] 			= is_cont_feature ? 1 : 0;
        scv_state_data[SCV_STATE_TRUE_TOTAL_COUNT] 	= true_total_count;
        dtelog(NOTICE,"true_total_count:%lf",true_total_count);
    }

    float8 temp_float = 0;

    if( is_null_fval )
    {
        dtelog(NOTICE,"is_null_fval:%d",is_null_fval);
        if( !is_cont_feature ) 
        {
            if(  class <0 )
            {
                scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT] = less;
                dtelog(NOTICE,"SCV_STATE_TOTAL_ELEM_COUNT:%lf",less);
            }
            else
            {
                if (scv_state_data[SCV_STATE_MAX_CLASS_ELEM_COUNT] < less)
                {
                    scv_state_data[SCV_STATE_MAX_CLASS_ELEM_COUNT] = less;
                    scv_state_data[SCV_STATE_MAX_CLASS_ID] = class;
                }
                accumulate_pre_split_scv(
                        scv_state_data, 
                        less, 
                        scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT],
                        split_criterion);
            }
        }
        else
        {
        	check_error
        		(
        			false,
        			"continuous features must not have null feature value"
        		);
        }
    }
    else
    {
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
                scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT] = less+great;
                dtelog(NOTICE,"cont SCV_STATE_TOTAL_ELEM_COUNT:%lf",less+great);
            }
        }
        else
        {
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
                temp_float = less+great;
                if (scv_state_data[SCV_STATE_MAX_CLASS_ELEM_COUNT] < temp_float)
                {
                    scv_state_data[SCV_STATE_MAX_CLASS_ELEM_COUNT] = temp_float;
                    scv_state_data[SCV_STATE_MAX_CLASS_ID] = class;
                }

                accumulate_pre_split_scv(
                        scv_state_data, 
                        temp_float, 
                        scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT],
                        split_criterion);

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
    }

    PG_RETURN_ARRAYTYPE_P(scv_state_array);
}
PG_FUNCTION_INFO_V1(scv_aggr_sfunc);

/*
 * The pre-function for the aggregation of splitting criteria values. It takes the 
 * state array produced by two sfunc and combine them together.  
 *
 * Parameters:
 *      scv_state_array    The array from sfunc1
 *      scv_state_array    The array from sfunc2
 * Return:
 *      A fourteen element array. Please refer to the definition of 
 *      SCV_STATE_ARRAY_INDEX for the detailed information of this
 *      array.
 */
Datum scv_aggr_prefunc(PG_FUNCTION_ARGS)
{
    ArrayType*	scv_state_array	= NULL;
    if (fcinfo->context && IsA(fcinfo->context, AggState))
        scv_state_array = PG_GETARG_ARRAYTYPE_P(0);
    else
        scv_state_array = PG_GETARG_ARRAYTYPE_P_COPY(0);

	check_error
		(
			scv_state_array,
			"invalid aggregation state array"
		);

    int array_dim 		= ARR_NDIM(scv_state_array);
    check_error_value
		(
			array_dim == 1,
			"invalid array dimension: %d. The dimension of scv state array must be equal to 1",
			array_dim
		);

    int *p_array_dim 	= ARR_DIMS(scv_state_array);
    int array_length 	= ArrayGetNItems(array_dim, p_array_dim);
    check_error_value
		(
			array_length == SCV_STATE_MAX_CLASS_ELEM_COUNT + 1,
			"invalid array length: %d",
			array_length
		);

    /* the scv state data from a segment */
    float8 *scv_state_data = (float8 *)ARR_DATA_PTR(scv_state_array);
	check_error
		(
			scv_state_data,
			"invalid aggregation data array"
		);    

    ArrayType* scv_state_array2	= PG_GETARG_ARRAYTYPE_P(1);
	check_error
		(
			scv_state_array2,
			"invalid aggregation state array"
		);

    array_dim 		= ARR_NDIM(scv_state_array2);
    check_error_value
		(
			array_dim == 1,
			"invalid array dimension: %d. The dimension of scv state array must be equal to 1",
			array_dim
		);    
    p_array_dim 	= ARR_DIMS(scv_state_array2);
    array_length 	= ArrayGetNItems(array_dim, p_array_dim);
    check_error_value
		(
			array_length == SCV_STATE_MAX_CLASS_ELEM_COUNT + 1,
			"invalid array length: %d",
			array_length
		);

    /* the scv state data from another segment */
    float8 *scv_state_data2 = (float8 *)ARR_DATA_PTR(scv_state_array2);
	check_error
		(
			scv_state_data2,
			"invalid aggregation data array"
		); 

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
        scv_state_data[SCV_STATE_IS_CONT]			= scv_state_data2[SCV_STATE_IS_CONT];
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

/*
 * The final function for the aggregation of splitting criteria values. It takes the 
 * state array produced by the sfunc and produces an eleven-element array.  
 *
 * Parameters:
 *      scv_state_array    The array containing all the information for the 
 *                         calculation of splitting criteria values. 
 * Return:
 *      An eleven element array. Please refer to the definition of 
 *      SCV_FINAL_ARRAY_INDEX for the detailed information of this
 *      array.
 */
Datum scv_aggr_ffunc(PG_FUNCTION_ARGS)
{
    ArrayType*	scv_state_array	= PG_GETARG_ARRAYTYPE_P(0);
	check_error
		(
			scv_state_array,
			"invalid aggregation state array"
		);

    int	 array_dim		= ARR_NDIM(scv_state_array);
    check_error_value
		(
			array_dim == 1,
			"invalid array dimension: %d. The dimension of state array must be equal to 1",
			array_dim
		);

    int* p_array_dim 	= ARR_DIMS(scv_state_array);
    int  array_length 	= ArrayGetNItems(array_dim, p_array_dim);

    check_error_value
		(
			array_length == SCV_STATE_MAX_CLASS_ELEM_COUNT + 1,
			"invalid array length: %d",
			array_length
		);

    dtelog(NOTICE, "scv_aggr_ffunc array_length:%d",array_length);

    float8 *scv_state_data = (float8 *)ARR_DATA_PTR(scv_state_array);
    check_error
    	(
    		scv_state_data,
    		"invalid aggregation data array"
    	);

    float8 init_scv = scv_state_data[SCV_STATE_INIT_SCV];

    int result_size = 11;
    float8 *result = palloc(sizeof(float8) * result_size);
    check_error
    	(
    		result,
    		"memory allocation failure"
    	);

    memset(result, 0, sizeof(float8) * result_size);

    dtelog(NOTICE, "scv_aggr_ffunc SCV_STATE_TOTAL_ELEM_COUNT:%lf",
            scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT]);

    /* 
     * For the following elements, such as max class id, we should copy them from step function array
     * to final function array for returning.
     */
    result[SCV_FINAL_SPLIT_CRITERION]   = scv_state_data[SCV_STATE_SPLIT_CRIT];
    result[SCV_FINAL_IS_CONT_FEATURE]	= scv_state_data[SCV_STATE_IS_CONT];
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
            result[SCV_FINAL_ENTROPY]  = 
                scv_state_data[SCV_STATE_ENTROPY_DATA]/scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT];
            /* Get infogain based on initial entropy and current one */
            result[SCV_FINAL_INFO_GAIN] = (init_scv - result[SCV_FINAL_ENTROPY]) * ratio;

            if (SC_GAINRATIO == (int)result[SCV_FINAL_SPLIT_CRITERION])
            {
                /* Compute the value of split info */
                result[SCV_FINAL_SPLIT_INFO] =
                        log(scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT]) -
                        scv_state_data[SCV_STATE_SPLIT_INFO_DATA] / scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT];

                /* Based on infogain and split info, we can compute gainratio. */
                if (!is_float_zero(result[SCV_FINAL_SPLIT_INFO]) &&
                    !is_float_zero(result[SCV_FINAL_INFO_GAIN]))
                {
                    dtelog(NOTICE,"SCV_FINAL_SPLIT_INFO:%lf,SCV_FINAL_INFO_GAIN:%lf",
                        result[SCV_FINAL_SPLIT_INFO],result[SCV_FINAL_INFO_GAIN]);
                    result[SCV_FINAL_GAIN_RATIO] = result[SCV_FINAL_INFO_GAIN] / result[SCV_FINAL_SPLIT_INFO];
                }
                else
                {
                    dtelog(NOTICE,"zero SCV_FINAL_SPLIT_INFO:%lf,SCV_FINAL_INFO_GAIN:%lf",
                        result[SCV_FINAL_SPLIT_INFO],result[SCV_FINAL_INFO_GAIN]);                    
                    result[SCV_FINAL_GAIN_RATIO] = 0;
                }
            }
        }
        else if (SC_GINI == (int)result[SCV_FINAL_SPLIT_CRITERION])
        {
            /* Get the value of gini */
            result[SCV_FINAL_GINI]      = 1 - scv_state_data[SCV_STATE_GINI_DATA] / scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT];

            /* Get gain based on initial gini and current gini value */
            result[SCV_FINAL_GINI_GAIN] = (init_scv - result[SCV_FINAL_GINI]) * ratio;
        }
        else
            check_error_value
            	(
            		false,
            		"invalid split criterion: %d. It must be 1, 2 or 3",
            		(int)result[SCV_FINAL_SPLIT_CRITERION]
            	);
    }
    else
        check_error_value
        	(
        		false,
        		"number of total element counts: %lld. ",
        		(int64)scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT]
        	);

    ArrayType* result_array =
        construct_array(
            (Datum *)result,
            result_size,
            FLOAT8OID,
            sizeof(float8),
            true,
            'd'
            );

    PG_RETURN_ARRAYTYPE_P(result_array);
}
PG_FUNCTION_INFO_V1(scv_aggr_ffunc);
