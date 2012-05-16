/*
 *
 * @file dt.c
 *
 * @brief Aggregate and utility functions written in C for C45 and RF in MADlib
 *
 * @date April 10, 2012
 */

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

#include "postgres.h"
#include "fmgr.h"
#include "utils/array.h"
#include "utils/lsyscache.h"
#include "utils/builtins.h"
#include "catalog/pg_type.h"
#include "catalog/namespace.h"
#include "nodes/execnodes.h"
#include "nodes/nodes.h"
#include "funcapi.h"

#ifndef NO_PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif
/*#define __DT_SHOW_DEBUG_INFO__*/
#ifdef __DT_SHOW_DEBUG_INFO__
#define dtelog(...) elog(__VA_ARGS__)
#else
#define dtelog(...)
#endif


/*
 * Postgres8.4 doesn't have such macro, so we add here
 */
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))
#endif


/*
 * This function is used to get the mask of the given value
 * val - ((val >> power) << power) equals to val % (2^power)
 */
#define dt_fid_mask(val, power) \
			(1 << (val - ((val >> power) << power)))


/*
 * We will do a lot of float number calculations on log, square, 
 * division, and multiplication, etc. Our tests show that the 
 * DBL_EPSILON is too precise for us. Therefore we define our 
 * own precision here.
 */
#define DT_EPSILON 0.000000001

/*
 * This function is used to test if a float value is 0.
 * Due to the precision of floating numbers, we can not
 * compare them directly with 0.
 */
#define dt_is_float_zero(value)  \
			((value) < DT_EPSILON && (value) > -DT_EPSILON)


/*
 * For Error Based Pruning (EBP), we need to compute the additional errors
 * if the error rate increases to the upper limit of the confidence level.
 * The coefficient is the square of the number of standard deviations
 * corresponding to the selected confidence level.
 * (Taken from Documenta Geigy Scientific Tables (Sixth Edition),
 * p185 (with modifications).)
 */
static float8 DT_CONFIDENCE_LEVEL[] =
				{0, 0.001, 0.005, 0.01, 0.05, 0.10, 0.20, 0.40, 1.00};
static float8 DT_CONFIDENCE_DEV[]   =
				{4.0, 3.09, 2.58, 2.33, 1.65, 1.28, 0.84, 0.25, 0.00};


#define MIN_DT_CONFIDENCE_LEVEL 0.001L
#define MAX_DT_CONFIDENCE_LEVEL 100.0L


#define dt_check_error_value(condition, message, value) \
			do { \
				if (!(condition)) \
					ereport(ERROR, \
							(errcode(ERRCODE_RAISE_EXCEPTION), \
							 errmsg(message, (value)) \
							) \
						   ); \
			} while (0)


#define dt_check_error(condition, message) \
			do { \
				if (!(condition)) \
					ereport(ERROR, \
							(errcode(ERRCODE_RAISE_EXCEPTION), \
							 errmsg(message) \
							) \
						   ); \
			} while (0)


static
float8
dt_ebp_calc_errors_internal
	(
	float8 total_cases,
	float8 num_errors,
	float8 conf_level,
	float8 coeff
	);


/*
 * @brief Calculates the total errors used by Error Based Pruning (EBP).
 *        This will be wrapped as a plc function.
 *
 * @param total         The number of total cases represented by the node
 *                      being processed.
 * @param probability   The probability to mis-classify cases represented
 *                      by the child nodes if they are pruned with EBP.
 * @param conf_level    A certainty factor to calculate the confidence limits
 *                      for the probability of error using the binomial theorem.
 * 
 * @return The computed total error.
 *
 */
Datum
dt_ebp_calc_errors
	(
	PG_FUNCTION_ARGS
	)
{
    float8 total_cases 	= PG_GETARG_FLOAT8(0);
    float8 probability 	= PG_GETARG_FLOAT8(1);
    float8 conf_level 	= PG_GETARG_FLOAT8(2);
    float8 result 		= 1.0L;
    float8 coeff 		= 0.0L;
    unsigned int i 		= 0;

    if (!dt_is_float_zero(100 - conf_level))
    {
    	dt_check_error_value
    		(
    			!(
    				conf_level < MIN_DT_CONFIDENCE_LEVEL ||
    				conf_level > MAX_DT_CONFIDENCE_LEVEL
    			 ),
    			"invalid confidence level:  %lf."
    			"Confidence level must be in range from 0.001 to 100",
    			conf_level
    		);

    	dt_check_error_value
    		(
    			total_cases > 0,
    			"invalid number: %lf. "
    			"The number of cases must be greater than 0",
    			total_cases
    		);

    	dt_check_error_value
    		(
    			!(probability < 0 || probability > 1),
    			"invalid probability: %lf. "
    			"The probability must be in range from 0 to 1",
    			probability
    		);

    	/*
    	 * Confidence level value is in range from 0.001 to 1.0.
    	 * It should be divided by 100 when calculate addition error.
    	 * Therefore, the range of conf_level here is [0.00001, 1.0].
    	 */
    	conf_level = conf_level * 0.01;

		/*
		 * Since the conf_level is in [0.00001, 1.0],
		 * the value of i will be in [1, length(DT_CONFIDENCE_LEVEL) - 1]
		 */
		while (conf_level > DT_CONFIDENCE_LEVEL[i]) i++;

    	dt_check_error_value
    		(
    			i > 0 && i < ARRAY_SIZE(DT_CONFIDENCE_LEVEL),
    			"invalid value: %d. "
    			"The index of confidence level must be in range from 0 to 8",
    			i
    		);

		coeff = DT_CONFIDENCE_DEV[i-1] +
				(DT_CONFIDENCE_DEV[i] - DT_CONFIDENCE_DEV[i-1]) *
				(conf_level - DT_CONFIDENCE_LEVEL[i-1]) /
				(DT_CONFIDENCE_LEVEL[i] - DT_CONFIDENCE_LEVEL[i-1]);

		coeff *= coeff;

		float8 num_errors = total_cases * (1 - probability);
    	result 			  = dt_ebp_calc_errors_internal
								(
									total_cases,
									num_errors,
									conf_level,
									coeff
								) + num_errors;
    }

	PG_RETURN_FLOAT8((float8)result);
}
PG_FUNCTION_INFO_V1(dt_ebp_calc_errors);


/*
 * @brief This function calculates the additional errors for EBP.
 *        Detailed description of that pruning strategy can be found in the paper
 *        'Error-Based Pruning of Decision Trees Grown on Very Large Data Sets Can Work!'.
 *
 * @param total_cases   The number of total cases represented by the node
 *                      being processed.
 * @param num_errors    The number of mis-classified cases represented
 *                      by the child nodes if they are pruned with EBP.
 * @param conf_level    A certainty factor to calculate the confidence limits
 *                      for the probability of error using the binomial theorem.
 *
 * @return The additional errors if we prune the node being processed.
 *
 */
static
float8
dt_ebp_calc_errors_internal
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
        return tmp +
        	   num_errors *
               (
                   dt_ebp_calc_errors_internal
                   	   (total_cases, 1.0, conf_level, coeff) -
                   tmp
               );
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
			sqrt(coeff * ((num_errors + 0.5) *
				 (1 - (num_errors + 0.5)/total_cases) + coeff/4))
			)
			/ (total_cases + coeff);

        return (total_cases * tmp - num_errors);
    }
}


/*
 * @brief The step function for aggregating the class counts while
 *        doing Reduce Error Pruning (REP).
 *
 * @param class_count_array     The array used to store the accumulated information.
 *                              [0]: the total number of mis-classified cases
 *                              [i]: the number of cases belonging to the ith class
 * @param classified_class      The predicted class based on our trained DT model.
 * @param original_class        The real class value provided in the validation set.
 * @param max_num_of_classes    The total number of distinct class values.
 *
 * @return An updated state array.
 *
 */
Datum
dt_rep_aggr_class_count_sfunc
	(
	PG_FUNCTION_ARGS
	)
{
    ArrayType *pg_class_count    = NULL;
    int array_dim                = 0;
    int *p_array_dim             = NULL;
    int array_length             = 0;
    int64 *class_count           = NULL;
    int classified_class    	 = PG_GETARG_INT32(1);
    int original_class      	 = PG_GETARG_INT32(2);
    int max_num_of_classes  	 = PG_GETARG_INT32(3);
    bool rebuild_array     		 = false;

    dt_check_error_value
		(
			max_num_of_classes >= 2,
			"invalid value: %d. "
			"The number of classes must be greater than or equal to 2",
			max_num_of_classes
		);

    dt_check_error_value
		(
			original_class > 0 && original_class <= max_num_of_classes,
			"invalid real class value: %d. "
			"It must be in range from 1 to the number of classes",
			original_class
		);

    dt_check_error_value
		(
			classified_class > 0 && classified_class <= max_num_of_classes,
			"invalid classified class value: %d. "
			"It must be in range from 1 to the number of classes",
			classified_class
		);
    
    /* test if the first argument (class count array) is null */
    if (PG_ARGISNULL(0))
    {
    	/*
    	 * We assume the maximum number of classes is limited (up to millions),
    	 * so that the allocated array won't break our memory limitation.
    	 */
        class_count 	 = palloc0(sizeof(int64) * (max_num_of_classes + 1));
        array_length 	 = max_num_of_classes + 1;
        rebuild_array 	 = true;

    }
    else
    {
        if (fcinfo->context && IsA(fcinfo->context, AggState))
            pg_class_count = PG_GETARG_ARRAYTYPE_P(0);
        else
            pg_class_count = PG_GETARG_ARRAYTYPE_P_COPY(0);
        
        dt_check_error
    		(
    			pg_class_count,
    			"invalid aggregation state array"
    		);

        array_dim = ARR_NDIM(pg_class_count);

        dt_check_error_value
    		(
    			array_dim == 1,
    			"invalid array dimension: %d. "
    			"The dimension of class count array must be equal to 1",
    			array_dim
    		);

        p_array_dim         = ARR_DIMS(pg_class_count);
        array_length        = ArrayGetNItems(array_dim,p_array_dim);
        class_count    		= (int64 *)ARR_DATA_PTR(pg_class_count);

        dt_check_error_value
    		(
    			array_length == max_num_of_classes + 1,
    			"invalid array length: %d. "
    			"The length of class count array must be "
    			"equal to the total number classes + 1",
    			array_length
    		);
    }

    /*
     * If the condition is met, then the current record
     * has been mis-classified. Therefore, we will need
     * to increase the first element.
     */
    if (original_class != classified_class)
        ++class_count[0];

    /* In any case, we will update the original class count */
    ++class_count[original_class];

    if (rebuild_array)
    {
        /* construct a new array to keep the aggr states. */
        pg_class_count =
        	construct_array(
        		(Datum *)class_count,
                array_length,
                INT8OID,
                sizeof(int64),
                true,
                'd'
                );
    }

    PG_RETURN_ARRAYTYPE_P(pg_class_count);
}
PG_FUNCTION_INFO_V1(dt_rep_aggr_class_count_sfunc);


/*
 * @brief The pre-function for REP. It takes two class count arrays
 *        produced by the sfunc and combine them together.
 *
 * @param 1 arg         The array returned by sfun1.
 * @param 2 arg         The array returned by sfun2.
 *
 * @return The array with the combined information.
 *
 */
Datum
dt_rep_aggr_class_count_prefunc
	(
	PG_FUNCTION_ARGS
	)
{
    ArrayType *pg_class_count    	= NULL;
    int array_dim                   = 0;
    int *p_array_dim                = NULL;
    int array_length                = 0;
    int64 *class_count         		= NULL;

    ArrayType *pg_class_count_2   	= NULL;
    int array_dim2                  = 0;
    int *p_array_dim2               = NULL;
    int array_length2               = 0;
    int64 *class_count_2        	= NULL;

    if (PG_ARGISNULL(0) && PG_ARGISNULL(1))
        PG_RETURN_NULL();
    else if (PG_ARGISNULL(1) || PG_ARGISNULL(0))
    {
        /*
         * If one of the two array is null,
         * just return the non-null array directly
         */
    	PG_RETURN_ARRAYTYPE_P(PG_ARGISNULL(1) ?
    				PG_GETARG_ARRAYTYPE_P(0) :
    				PG_GETARG_ARRAYTYPE_P(1));
    }
    else
    {
        /* If both arrays are not null, we will merge them together */
        if (fcinfo->context && IsA(fcinfo->context, AggState))
            pg_class_count = PG_GETARG_ARRAYTYPE_P(0);
        else
            pg_class_count = PG_GETARG_ARRAYTYPE_P_COPY(0);
        
        dt_check_error
    		(
    			pg_class_count,
    			"invalid aggregation state array"
    		);

        array_dim = ARR_NDIM(pg_class_count);

        dt_check_error_value
    		(
    			array_dim == 1,
    			"invalid array dimension: %d. "
    			"The dimension of class count array must be equal to 1",
    			array_dim
    		);

        p_array_dim             = ARR_DIMS(pg_class_count);
        array_length            = ArrayGetNItems(array_dim,p_array_dim);
        class_count        		= (int64 *)ARR_DATA_PTR(pg_class_count);

        pg_class_count_2      	= PG_GETARG_ARRAYTYPE_P(1);
        array_dim2              = ARR_NDIM(pg_class_count_2);
        dt_check_error_value
    		(
    			array_dim2 == 1,
    			"invalid array dimension: %d. "
    			"The dimension of class count array must be equal to 1",
    			array_dim2
    		);

        p_array_dim2        = ARR_DIMS(pg_class_count_2);
        array_length2       = ArrayGetNItems(array_dim2,p_array_dim2);
        class_count_2       = (int64 *)ARR_DATA_PTR(pg_class_count_2);

        dt_check_error
    		(
    			array_length == array_length2,
    			"the size of the two array must be the same in prefunction"
    		);

        for (int index = 0; index < array_length; index++)
            class_count[index] += class_count_2[index];

        PG_RETURN_ARRAYTYPE_P(pg_class_count);
    }
}
PG_FUNCTION_INFO_V1(dt_rep_aggr_class_count_prefunc);


/*
 * @brief The final function for aggregating the class counts for REP.
 *        It takes the class count array produced by the sfunc and produces
 *        a two-element array. The first element is the ID of the class that
 *        has the maximum number of cases represented by the root node of
 *        the subtree being processed. The second element is the number of
 *        reduced misclassified cases if the leaf nodes of the subtree are pruned.
 *
 * @param class_count_data  The array containing all the information for the
 *                          calculation of Reduced-Error pruning. 
 *
 * @return A two-element array that contains the most frequent class ID and
 *         the prune flag.
 *
 */
Datum
dt_rep_aggr_class_count_ffunc
	(
	PG_FUNCTION_ARGS
	)
{
    ArrayType *pg_class_count    = PG_GETARG_ARRAYTYPE_P(0);
    int array_dim                = ARR_NDIM(pg_class_count);

    dt_check_error_value
		(
			array_dim == 1,
			"invalid array dimension: %d. "
			"The dimension of class count array must be equal to 1",
			array_dim
		);

    int *p_array_dim           = ARR_DIMS(pg_class_count);
    int array_length           = ArrayGetNItems(array_dim,p_array_dim);
    int64 *class_count         = (int64 *)ARR_DATA_PTR(pg_class_count);
    int64 *result              = palloc(sizeof(int64)*2);

    dt_check_error
		(
			result,
			"memory allocation failure"
		);

    int64 max = class_count[1];
    int64 sum = max;
    int maxid = 1;
    for(int i = 2; i < array_length; ++i)
    {
        if(max < class_count[i])
        {
            max = class_count[i];
            maxid = i;
        }

        sum += class_count[i];
    }

    /* maxid is the id of the class, which has the most cases */
    result[0] = maxid;

    /*
     * (sum - max) is the number of mis-classified cases represented by
     * the root node of the subtree being processed
     * class_count_data[0] the total number of mis-classified cases
     */
    result[1] = class_count[0] - (sum - max);

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
PG_FUNCTION_INFO_V1(dt_rep_aggr_class_count_ffunc);


/*
 * 	Calculating Split Criteria Values (SCVs for short) is a critical
 * 	step for growing a decision tree. While the formulas for different
 * 	criteria are well defined and understood, the process for calculating
 * 	them are not. In the database context, we can not follow the classical
 * 	approach to keep all needed counts data in memory resident structures,
 * 	as the memory requirement is usually proportional to the size of
 * 	the train sets. For big data, this requirement is usually hard to fulfill.
 *
 * 	When building DT in databases, we try to leverage the DB's aggregation
 * 	mechanism to do the same thing. This will also give us the opportunity
 * 	to leverage database's parallelization infrastructure.
 *
 * 	For that purpose, we will process the train set into something we call
 * 	Attribute Class Statistic (ACS for short) with a set of transformations
 * 	and use aggregate functions to work on that. Details of how an ACS is
 * 	generated can be found in DT design doc. The following is an example ACS
 * 	for the golf data set:
 *
 *   fid | fval | class | is_cont | split_value | le | gt | nid | tid
 *   ----+------+-------+---------+-------------+----+----+-----+-----
 *       |      |       |         |             | 14 |    |   1 |   1
 *       |      |     2 |         |             |  9 |    |   1 |   1
 *       |      |     1 |         |             |  5 |    |   1 |   1
 *     4 |      |       |         |             | 14 |    |   1 |   1
 *     4 |    2 |       | f       |             |  6 |    |   1 |   1
 *     4 |    1 |       | f       |             |  8 |    |   1 |   1
 *     4 |      |     2 |         |             |  9 |    |   1 |   1
 *     4 |    2 |     2 | f       |             |  3 |    |   1 |   1
 *     4 |    1 |     2 | f       |             |  6 |    |   1 |   1
 *     4 |      |     1 |         |             |  5 |    |   1 |   1
 *     4 |    2 |     1 | f       |             |  3 |    |   1 |   1
 *     4 |    1 |     1 | f       |             |  2 |    |   1 |   1
 *     3 |      |       |         |             | 14 |    |   1 |   1
 *     3 |    3 |       | f       |             |  5 |    |   1 |   1
 *     3 |    2 |       | f       |             |  5 |    |   1 |   1
 *     3 |    1 |       | f       |             |  4 |    |   1 |   1
 *     3 |      |     2 |         |             |  9 |    |   1 |   1
 *     3 |    3 |     2 | f       |             |  2 |    |   1 |   1
 *     3 |    2 |     2 | f       |             |  3 |    |   1 |   1
 *     3 |    1 |     2 | f       |             |  4 |    |   1 |   1
 *     3 |      |     1 |         |             |  5 |    |   1 |   1
 *     3 |    3 |     1 | f       |             |  3 |    |   1 |   1
 *     3 |    2 |     1 | f       |             |  2 |    |   1 |   1
 *     3 |    1 |     1 | f       |             |    |    |   1 |   1
 *     2 |   96 |       | t       |          96 | 14 |    |   1 |   1
 *     2 |   95 |       | t       |          95 | 13 |  1 |   1 |   1
 *     2 |   90 |       | t       |          90 | 12 |  2 |   1 |   1
 *     2 |   85 |       | t       |          85 | 10 |  4 |   1 |   1
 *     2 |   80 |       | t       |          80 |  9 |  5 |   1 |   1
 *     2 |   78 |       | t       |          78 |  6 |  8 |   1 |   1
 *     2 |   75 |       | t       |          75 |  5 |  9 |   1 |   1
 *     2 |   70 |       | t       |          70 |  4 | 10 |   1 |   1
 *     2 |   65 |       | t       |          65 |  1 | 13 |   1 |   1
 *     2 |   96 |     2 | t       |          96 |  9 |    |   1 |   1
 *     2 |   95 |     2 | t       |          95 |  8 |  1 |   1 |   1
 *     2 |   90 |     2 | t       |          90 |  8 |  1 |   1 |   1
 *     2 |   85 |     2 | t       |          85 |  7 |  2 |   1 |   1
 *     2 |   80 |     2 | t       |          80 |  7 |  2 |   1 |   1
 *     2 |   78 |     2 | t       |          78 |  5 |  4 |   1 |   1
 *     2 |   75 |     2 | t       |          75 |  4 |  5 |   1 |   1
 *     2 |   70 |     2 | t       |          70 |  3 |  6 |   1 |   1
 *     2 |   65 |     2 | t       |          65 |  1 |  8 |   1 |   1
 *     2 |   96 |     1 | t       |          96 |  5 |    |   1 |   1
 *     2 |   95 |     1 | t       |          95 |  5 |    |   1 |   1
 *     2 |   90 |     1 | t       |          90 |  4 |  1 |   1 |   1
 *     2 |   85 |     1 | t       |          85 |  3 |  2 |   1 |   1
 *     2 |   80 |     1 | t       |          80 |  2 |  3 |   1 |   1
 *     2 |   78 |     1 | t       |          78 |  1 |  4 |   1 |   1
 *     2 |   75 |     1 | t       |          75 |  1 |  4 |   1 |   1
 *     2 |   70 |     1 | t       |          70 |  1 |  4 |   1 |   1
 *     2 |   65 |     1 | t       |          65 |    |  5 |   1 |   1
 *     1 |   85 |       | t       |          85 | 14 |    |   1 |   1
 *     1 |   83 |       | t       |          83 | 13 |  1 |   1 |   1
 *     1 |   81 |       | t       |          81 | 12 |  2 |   1 |   1
 *     1 |   80 |       | t       |          80 | 11 |  3 |   1 |   1
 *     1 |   75 |       | t       |          75 | 10 |  4 |   1 |   1
 *     1 |   72 |       | t       |          72 |  8 |  6 |   1 |   1
 *     1 |   71 |       | t       |          71 |  6 |  8 |   1 |   1
 *     1 |   70 |       | t       |          70 |  5 |  9 |   1 |   1
 *     1 |   69 |       | t       |          69 |  4 | 10 |   1 |   1
 *     1 |   68 |       | t       |          68 |  3 | 11 |   1 |   1
 *     1 |   65 |       | t       |          65 |  2 | 12 |   1 |   1
 *     1 |   64 |       | t       |          64 |  1 | 13 |   1 |   1
 *     1 |   85 |     2 | t       |          85 |  9 |    |   1 |   1
 *     1 |   83 |     2 | t       |          83 |  9 |    |   1 |   1
 *     1 |   81 |     2 | t       |          81 |  8 |  1 |   1 |   1
 *     1 |   80 |     2 | t       |          80 |  7 |  2 |   1 |   1
 *     1 |   75 |     2 | t       |          75 |  7 |  2 |   1 |   1
 *     1 |   72 |     2 | t       |          72 |  5 |  4 |   1 |   1
 *     1 |   71 |     2 | t       |          71 |  4 |  5 |   1 |   1
 *     1 |   70 |     2 | t       |          70 |  4 |  5 |   1 |   1
 *     1 |   69 |     2 | t       |          69 |  3 |  6 |   1 |   1
 *     1 |   68 |     2 | t       |          68 |  2 |  7 |   1 |   1
 *     1 |   65 |     2 | t       |          65 |  1 |  8 |   1 |   1
 *     1 |   64 |     2 | t       |          64 |  1 |  8 |   1 |   1
 *     1 |   85 |     1 | t       |          85 |  5 |    |   1 |   1
 *     1 |   83 |     1 | t       |          83 |  4 |  1 |   1 |   1
 *     1 |   81 |     1 | t       |          81 |  4 |  1 |   1 |   1
 *     1 |   80 |     1 | t       |          80 |  4 |  1 |   1 |   1
 *     1 |   75 |     1 | t       |          75 |  3 |  2 |   1 |   1
 *     1 |   72 |     1 | t       |          72 |  3 |  2 |   1 |   1
 *     1 |   71 |     1 | t       |          71 |  2 |  3 |   1 |   1
 *     1 |   70 |     1 | t       |          70 |  1 |  4 |   1 |   1
 *     1 |   69 |     1 | t       |          69 |  1 |  4 |   1 |   1
 *     1 |   68 |     1 | t       |          68 |  1 |  4 |   1 |   1
 *     1 |   65 |     1 | t       |          65 |  1 |  4 |   1 |   1
 *     1 |   64 |     1 | t       |          64 |    |  5 |   1 |   1
 * (87 rows)
 * 
 * The rows are grouped by (tid, nid, fid, split_value). For each group,
 * we will calculate an SCV based on the specified splitting criteria.
 * For discrete attributes, only le (c) is used to store the counts.
 * Both le and gt will be used for continuous values.
 *
 * Given the format of the input data stream, SCV calculation is different
 * from using the SCV formulas directly.
 *
 * For infogain and gainratio, there are (number_of_classes + 1) rows for
 * each distinct value of an attribute. The first row is the sub-total
 * count (STC) of cases for a (fval, class) pair. We will simply record it
 * in the aggregate state. For each of the successive rows in that group, we
 * calculate the partial SCV with this formula:
 *     c*log (STC/c)
 *
 * Assume TC is the total count of records, and there are two groups, whose
 * sub-total counts are STC1 and STC2. We further assume there are two classes
 * and the class counts are (c1, c2) for STC1 and (c3, c4) for STC2.
 * Entropy can then be calculated as:
 * 	   (STC1/TC)*((c1/STC1)*log(STC1/c1)+(c2/STC1)*log(STC1/c2))+
 *     (STC2/TC)*((c3/STC2)*log(STC2/c3)+(c4/STC2)*log(STC2/c4))
 *    =
 *     (
 *      c1*log(STC1/c1)+
 *      c2*log(STC1/c2)+
 *      c3*log(STC2/c3)+
 *      c4*log(STC2/c4)
 *     )/TC.
 *
 * With this formula, the aggregate function can calculate each of the
 * components in the formula when the rows come in and store the partial
 * summary in the aggregate state. In the final function, we can get the
 * entropy by dividing the final summary value with TC. This way, we
 * successfully remove the need to keep all the class count values in a
 * possibly very big array. And the calculation fits quite well with the
 * PG/GP's aggregate infrastructure.
 *
 * For gini, the calculation process is very similar. The only difference
 * is that we calculate the partial SCV with the formula:
 *    (c*c)/STC.
 *
 * Again assume TC, STC1, STC2, c1, c2, c3, and c4 as above. Gini index
 * can then be calculated as:
 * 	  (STC1/TC)*(1-(c1/STC1)^2-(c2/STC1)^2)+
 * 	  (STC2/TC)*(1-(c3/STC2)^2-(c4/STC2)^2)
 * 	 =
 *    (STC1/TC+STC2/TC)-(c1^2/STC1+c2^2/STC1+c3^2/STC2+c4^2/STC2)/TC
 *   =
 *    1-(c1^2/STC1+c2^2/STC1+c3^2/STC2+c4^2/STC2)/TC.
 *
 * Obviously, the gini index can also be calculated with aggregations.
 *
 * Based on this understanding, we will define the following structures,
 * types, and aggregate functions to calculate SCVs.
 *
 */


/*
 * We use a 14-element array to keep the state of the
 * aggregate for calculating splitting criteria values (SCVs).
 * The enum types defines which element of that array is used
 * for which purpose.
 *
 */
enum DT_SCV_STATE_ARRAY_INDEX
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
    /* How many elements are less than or equal to the split value */
    SCV_STATE_LESS_ELEM_COUNT,
    /* How many elements are greater than the split value */
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
    /* Whether the selected feature is continuous or discrete */
    SCV_STATE_IS_CONT,
    /* Init value of entropy/gini before split */
    SCV_STATE_INIT_SCV,
    /*
     * It specifies the total number of records in training set.
     * If there is missing values, the total count accumulated and
     * stored in SCV_STATE_TOTAL_ELEM_COUNT are not equal to
     * this element. In that case, our calculated gain should be
     * discounted by the ratio of
     * SCV_STATE_TOTAL_ELEM_COUNT/SCV_STATE_TRUE_TOTAL_COUNT.
     */
    SCV_STATE_TRUE_TOTAL_COUNT,

    /* The ID of the class with the most elements */
    SCV_STATE_MAX_CLASS_ID,
    /* The total number of elements belonging to MAX_CLASS */
    SCV_STATE_MAX_CLASS_ELEM_COUNT
};


/*
 * We use a 11-element array to keep the final result of the
 * aggregate for calculating splitting criteria values (SCVs).
 * The enum types defines which element of that array is used
 * for which purpose.
 *
 */
enum DT_SCV_FINAL_ARRAY_INDEX
{
    /* Calculated entropy value */
    SCV_FINAL_ENTROPY = 0,
    /* Calculated split info value */
    SCV_FINAL_SPLIT_INFO,
    /* Calculated gini value */
    SCV_FINAL_GINI,
    /* 1 infogain, 2 gainratio, 3 gini */
    SCV_FINAL_SPLIT_CRITERION,
    /* entropy_before_split-SCV_FINAL_ENTROPY */
    SCV_FINAL_INFO_GAIN,
    /* SCV_FINAL_ENTROPY/SCV_FINAL_SPLIT_INFO */
    SCV_FINAL_GAIN_RATIO,
    /* gini_before_split- SCV_FINAL_GINI */
    SCV_FINAL_GINI_GAIN,
    /* Whether the selected feature is continuous or discrete */
    SCV_FINAL_IS_CONT_FEATURE,
    /* The ID of the class with the most elements */
    SCV_FINAL_CLASS_ID,
    /* The percentage of elements belonging to MAX_CLASS */
    SCV_FINAL_CLASS_PROB,
    /* Total count of records whose value is not null */
    SCV_FINAL_TOTAL_COUNT
};


/* Codes for different split criteria. */
#define DT_SC_INFOGAIN  1
#define DT_SC_GAINRATIO 2
#define DT_SC_GINI      3


/*
 * @brief The function is used to calculate pre-split splitting criteria value. 
 *
 * @param scv_state_array     The array containing all the information for the 
 *                            calculation of splitting criteria values. 
 * @param curr_class_count    Total count of elements belonging to current class.
 * @param total_elem_count    Total count of elements.
 * @param split_criterion     1 - infogain; 2 - gainratio; 3 - gini.
 *
 */
static
void
dt_accumulate_pre_split_scv
    (
    float8*     scv_state_array,
    float8      curr_class_count,
    float8      total_elem_count,
    int         split_criterion
    )
{

	dt_check_error
		(
			scv_state_array,
			"invalid aggregation state array"
		);

	dt_check_error_value
		(
			(DT_SC_INFOGAIN  == split_criterion ||
			DT_SC_GAINRATIO  == split_criterion ||
			DT_SC_GINI       == split_criterion),
			"invalid split criterion: %d. "
			"It must be 1(infogain), 2(gainratio) or 3(gini)",
			split_criterion
		);

	dt_check_error_value
		(
			total_elem_count > 0,
			"invalid value: %lf. "
			"The total element count must be greater than 0",
			total_elem_count
		);

	dt_check_error_value
		(
			curr_class_count >= 0,
			"invalid value: %lf. "
			"The current class count must be greater than or equal to 0",
			curr_class_count
		);

    float8 temp_float = curr_class_count / total_elem_count;

    if (DT_SC_INFOGAIN  == split_criterion ||
        DT_SC_GAINRATIO == split_criterion )
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
 * @brief The step function for the aggregation of splitting criteria values.
 *        It accumulates all the information for scv calculation
 *        and stores to a fourteen element array.
 *
 * @param scv_state_array   The array used to accumulate all the information
 *                          for the calculation of splitting criteria values.
 *                          Please refer to the definition of DT_SCV_STATE_ARRAY_INDEX.
 * @param split_criterion   1- infogain; 2- gainratio; 3- gini.
 * @param feature_val       The feature value of current record under processing.
 * @param class             The class of current record under processing.
 * @param is_cont_feature   True- The feature is continuous. False- The feature is
 *                          discrete.
 * @param less              Count of elements less than or equal to feature_val.
 * @param great             Count of elements greater than feature_val.
 * @param true_total_count  If there is any missing value, true_total_count is larger
 *      				    than the total count computed in the aggregation. Thus,
 *                          we should multiply a ratio for the computed gain.
 *
 * @return A fourteen element array. Please refer to the definition of 
 *         DT_SCV_STATE_ARRAY_INDEX for the detailed information of this array.
 */
Datum
dt_scv_aggr_sfunc
	(
	PG_FUNCTION_ARGS
	)
{
    ArrayType*	scv_state_array	= NULL;
    if (fcinfo->context && IsA(fcinfo->context, AggState))
        scv_state_array = PG_GETARG_ARRAYTYPE_P(0);
    else
        scv_state_array = PG_GETARG_ARRAYTYPE_P_COPY(0);

	dt_check_error
		(
			scv_state_array,
			"invalid aggregation state array"
		);

    int	 array_dim 		= ARR_NDIM(scv_state_array);

    dt_check_error_value
		(
			array_dim == 1,
			"invalid array dimension: %d. "
			"The dimension of scv state array must be equal to 1",
			array_dim
		);

    int* p_array_dim	= ARR_DIMS(scv_state_array);
    int  array_length	= ArrayGetNItems(array_dim, p_array_dim);

    dt_check_error_value
		(
			array_length == SCV_STATE_MAX_CLASS_ELEM_COUNT + 1,
			"invalid array length: %d",
			array_length
		);

    float8 *scv_state_data = (float8 *)ARR_DATA_PTR(scv_state_array);
	dt_check_error
		(
			scv_state_data,
			"invalid aggregation data array"
		);

    int    split_criterion	= PG_GETARG_INT32(1);
    bool   is_null_fval     = PG_ARGISNULL(2);
    float8 feature_val		= PG_GETARG_FLOAT8(2);
    float8 class			= PG_ARGISNULL(3) ? -1 : PG_GETARG_FLOAT8(3);
    bool   is_cont_feature 	= PG_ARGISNULL(4) ? false : PG_GETARG_BOOL(4);
    float8 less				= PG_ARGISNULL(5) ? 0 : PG_GETARG_FLOAT8(5);
    float8 great			= PG_ARGISNULL(6) ? 0 : PG_GETARG_FLOAT8(6);
    float8 true_total_count = PG_ARGISNULL(7) ? 0 : PG_GETARG_FLOAT8(7);

	dt_check_error_value
		(
			(DT_SC_INFOGAIN  == split_criterion ||
			DT_SC_GAINRATIO  == split_criterion ||
			DT_SC_GINI       == split_criterion),
			"invalid split criterion: %d. "
			"It must be 1(infogain), 2(gainratio) or 3(gini)",
			split_criterion
		);

    /*
     *  If the count for total element is still zero
     *  it is the first time that step function is
     *  invoked. In that case, we should initialize
     *  several elements.
     */ 
    if (dt_is_float_zero(scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT]))
    {
        scv_state_data[SCV_STATE_SPLIT_CRIT] 		= split_criterion;
        if (DT_SC_GINI == split_criterion)
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

    if (is_null_fval)
    {
        dtelog(NOTICE,"is_null_fval:%d",is_null_fval);
        if (!is_cont_feature)
        {
            if (class < 0)
            {
                scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT] = less;
                dtelog(NOTICE,"SCV_STATE_TOTAL_ELEM_COUNT:%lf",less);
            }
            else
            {
                if (scv_state_data[SCV_STATE_MAX_CLASS_ELEM_COUNT] < less)
                {
                    scv_state_data[SCV_STATE_MAX_CLASS_ELEM_COUNT] 	= less;
                    scv_state_data[SCV_STATE_MAX_CLASS_ID] 			= class;
                }
                dt_accumulate_pre_split_scv
                	(
                        scv_state_data, 
                        less, 
                        scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT],
                        split_criterion
                    );
            }
        }
        else
        {
        	dt_check_error
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
            /*
             * a -1 means the current input row contains the
             * total number of (attribute, class) pairs
             */
            if (!is_cont_feature)
            {
                /*
                 * This block calculates for discrete features, which only
                 * use the column of less.
                 */
                scv_state_data[SCV_STATE_CURR_FEATURE_VALUE] 	  = feature_val;
                scv_state_data[SCV_STATE_CURR_FEATURE_ELEM_COUNT] = less;

                dtelog(NOTICE,"feature_val:%lf,feature_elem_count:%lf",
                	   feature_val, less);

                if (DT_SC_GAINRATIO == split_criterion)
                {
                    temp_float = scv_state_data[SCV_STATE_CURR_FEATURE_ELEM_COUNT];
                    if (!dt_is_float_zero(temp_float))
                        scv_state_data[SCV_STATE_SPLIT_INFO_DATA] +=
                        temp_float*log(temp_float);
                }
            }
            else
            {
                /*
                 * This block calculates for continuous features,
                 * which the columns of less/great.
                 */
                scv_state_data[SCV_STATE_LESS_ELEM_COUNT]	= less;
                scv_state_data[SCV_STATE_GREAT_ELEM_COUNT]	= great;

                if (DT_SC_GAINRATIO == split_criterion)
                {
                    for (int index=SCV_STATE_LESS_ELEM_COUNT;
                    	 index <=SCV_STATE_GREAT_ELEM_COUNT;
                    	 index++
                    	)
                    {
                        temp_float = scv_state_data[index];
                        if (!dt_is_float_zero(temp_float))
                        {
                            scv_state_data[SCV_STATE_SPLIT_INFO_DATA] +=
                            temp_float * log(temp_float);
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
                if (DT_SC_GAINRATIO == split_criterion ||
                    DT_SC_INFOGAIN  == split_criterion)
                {
                    if (!dt_is_float_zero
                    		(less -
                    		 scv_state_data[SCV_STATE_CURR_FEATURE_ELEM_COUNT]
                    		) 		&&
                    	less > 0	&&
                    	scv_state_data[SCV_STATE_CURR_FEATURE_ELEM_COUNT] > 0
                       )
                        scv_state_data[SCV_STATE_ENTROPY_DATA] +=
                        less *
                        log(scv_state_data[SCV_STATE_CURR_FEATURE_ELEM_COUNT]/
                            less);
                }
                else
                {
                	/* DT_SC_GINI == split_criterion */
                    if (scv_state_data[SCV_STATE_CURR_FEATURE_ELEM_COUNT] > 0)
						scv_state_data[SCV_STATE_GINI_DATA] +=
						less * less /
						scv_state_data[SCV_STATE_CURR_FEATURE_ELEM_COUNT];
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

                dt_accumulate_pre_split_scv
                	(
                        scv_state_data, 
                        temp_float, 
                        scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT],
                        split_criterion
                    );

                /* This block accumulates entropy/gini for continuous features.*/
                float8 temp_array[]	 = {less, great};
                float8 count_array[] = {scv_state_data[SCV_STATE_LESS_ELEM_COUNT],
                						scv_state_data[SCV_STATE_GREAT_ELEM_COUNT]
                					   };

                for (int index = 0; index < 2; index++)
                {
                    temp_float = temp_array[index];

                    if (DT_SC_GAINRATIO == split_criterion ||
                        DT_SC_INFOGAIN  == split_criterion)
                    {
                        if (!dt_is_float_zero(temp_float - count_array[index])	&&
                            temp_float > 0										&&
                            count_array[index] > 0)
                            scv_state_data[SCV_STATE_ENTROPY_DATA] +=
                            temp_float * log(count_array[index]/temp_float);
                    }
                    else
                    {
                        /* DT_SC_GINI == split_criterion */
						if (count_array[index] > 0)
							scv_state_data[SCV_STATE_GINI_DATA] +=
							temp_float * temp_float / count_array[index];
                    }
                }
            }
        }
    }

    PG_RETURN_ARRAYTYPE_P(scv_state_array);
}
PG_FUNCTION_INFO_V1(dt_scv_aggr_sfunc);


/*
 * @brief The pre-function for the aggregation of splitting criteria values.
 *        It takes the state array produced by two sfunc and combine them together.
 *
 * @param scv_state_array    The array from sfunc1.
 * @param scv_state_array    The array from sfunc2.
 *
 * @return A fourteen element array. Please refer to the definition of 
 *         DT_SCV_STATE_ARRAY_INDEX for the detailed information of this array.
 *
 */
Datum
dt_scv_aggr_prefunc
	(
	PG_FUNCTION_ARGS
	)
{
    ArrayType*	scv_state_array	= NULL;
    if (fcinfo->context && IsA(fcinfo->context, AggState))
        scv_state_array = PG_GETARG_ARRAYTYPE_P(0);
    else
        scv_state_array = PG_GETARG_ARRAYTYPE_P_COPY(0);

	dt_check_error
		(
			scv_state_array,
			"invalid aggregation state array"
		);

    int array_dim 		= ARR_NDIM(scv_state_array);
    dt_check_error_value
		(
			array_dim == 1,
			"invalid array dimension: %d. "
			"The dimension of scv state array must be equal to 1",
			array_dim
		);

    int *p_array_dim 	= ARR_DIMS(scv_state_array);
    int array_length 	= ArrayGetNItems(array_dim, p_array_dim);
    dt_check_error_value
		(
			array_length == SCV_STATE_MAX_CLASS_ELEM_COUNT + 1,
			"invalid array length: %d",
			array_length
		);

    /* the scv state data from a segment */
    float8 *scv_state_data = (float8 *)ARR_DATA_PTR(scv_state_array);
	dt_check_error
		(
			scv_state_data,
			"invalid aggregation data array"
		);    

    ArrayType* scv_state_array2	= PG_GETARG_ARRAYTYPE_P(1);
	dt_check_error
		(
			scv_state_array2,
			"invalid aggregation state array"
		);

    array_dim 		= ARR_NDIM(scv_state_array2);
    dt_check_error_value
		(
			array_dim == 1,
			"invalid array dimension: %d. "
			"The dimension of scv state array must be equal to 1",
			array_dim
		);    
    p_array_dim 	= ARR_DIMS(scv_state_array2);
    array_length 	= ArrayGetNItems(array_dim, p_array_dim);
    dt_check_error_value
		(
			array_length == SCV_STATE_MAX_CLASS_ELEM_COUNT + 1,
			"invalid array length: %d",
			array_length
		);

    /* the scv state data from another segment */
    float8 *scv_state_data2 = (float8 *)ARR_DATA_PTR(scv_state_array2);
	dt_check_error
		(
			scv_state_data2,
			"invalid aggregation data array"
		); 

    /*
     * For the following data, such as entropy, gini and split info,
     * we need to combine the accumulated value from multiple segments.
     */ 
    scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT]	+=
    		scv_state_data2[SCV_STATE_TOTAL_ELEM_COUNT];
    scv_state_data[SCV_STATE_ENTROPY_DATA] 		+=
    		scv_state_data2[SCV_STATE_ENTROPY_DATA];
    scv_state_data[SCV_STATE_SPLIT_INFO_DATA]	+=
    		scv_state_data2[SCV_STATE_SPLIT_INFO_DATA];
    scv_state_data[SCV_STATE_GINI_DATA]			+=
    		scv_state_data2[SCV_STATE_GINI_DATA];

    /*
     * The following elements are just initialized once. If the first
     * scv_state is not initialized, we copy them from the second scv_state.
     */ 
    if (dt_is_float_zero(scv_state_data[SCV_STATE_SPLIT_CRIT]))
    {
        scv_state_data[SCV_STATE_SPLIT_CRIT]		=
        		scv_state_data2[SCV_STATE_SPLIT_CRIT];
        scv_state_data[SCV_STATE_TRUE_TOTAL_COUNT]	=
        		scv_state_data2[SCV_STATE_TRUE_TOTAL_COUNT];
        scv_state_data[SCV_STATE_IS_CONT]			=
        		scv_state_data2[SCV_STATE_IS_CONT];
    }
    
    /*
     *  We should compare the results from different segments and
     *  find the class with maximum cases.
     */ 
    if (scv_state_data[SCV_STATE_MAX_CLASS_ELEM_COUNT] <
    	scv_state_data2[SCV_STATE_MAX_CLASS_ELEM_COUNT])
    {
        scv_state_data[SCV_STATE_MAX_CLASS_ELEM_COUNT]	=
        		scv_state_data2[SCV_STATE_MAX_CLASS_ELEM_COUNT];
        scv_state_data[SCV_STATE_MAX_CLASS_ID]			=
        		scv_state_data2[SCV_STATE_MAX_CLASS_ID];
    }

    PG_RETURN_ARRAYTYPE_P(scv_state_array);
}
PG_FUNCTION_INFO_V1(dt_scv_aggr_prefunc);


/*
 * @brief The final function for the aggregation of splitting criteria values.
 *        It takes the state array produced by the sfunc and produces
 *        an eleven-element array.
 *
 * @param scv_state_array  The array containing all the information for the 
 *                         calculation of splitting criteria values. 
 *
 * @return An eleven element array. Please refer to the definition of 
 *         DT_SCV_FINAL_ARRAY_INDEX for the detailed information of this array.
 *
 */
Datum
dt_scv_aggr_ffunc
	(
	PG_FUNCTION_ARGS
	)
{
    ArrayType*	scv_state_array	= PG_GETARG_ARRAYTYPE_P(0);
	dt_check_error
		(
			scv_state_array,
			"invalid aggregation state array"
		);

    int	 array_dim		= ARR_NDIM(scv_state_array);
    dt_check_error_value
		(
			array_dim == 1,
			"invalid array dimension: %d. "
			"The dimension of state array must be equal to 1",
			array_dim
		);

    int* p_array_dim 	= ARR_DIMS(scv_state_array);
    int  array_length 	= ArrayGetNItems(array_dim, p_array_dim);

    dt_check_error_value
		(
			array_length == SCV_STATE_MAX_CLASS_ELEM_COUNT + 1,
			"invalid array length: %d",
			array_length
		);

    dtelog(NOTICE, "dt_scv_aggr_ffunc array_length:%d",array_length);

    float8 *scv_state_data = (float8 *)ARR_DATA_PTR(scv_state_array);
    dt_check_error
    	(
    		scv_state_data,
    		"invalid aggregation data array"
    	);

    float8 init_scv = scv_state_data[SCV_STATE_INIT_SCV];

    int result_size	= 11;
    float8 *result	= palloc0(sizeof(float8) * result_size);
    dt_check_error
    	(
    		result,
    		"memory allocation failure"
    	);

    dtelog
    	(
    		NOTICE,
    		"dt_scv_aggr_ffunc SCV_STATE_TOTAL_ELEM_COUNT:%lf",
            scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT]
        );

    /* 
     * For the following elements, such as max class id, we should copy
     * them from step function array to final function array for returning.
     */
    result[SCV_FINAL_SPLIT_CRITERION]   = scv_state_data[SCV_STATE_SPLIT_CRIT];
    result[SCV_FINAL_IS_CONT_FEATURE]	= scv_state_data[SCV_STATE_IS_CONT];
    result[SCV_FINAL_CLASS_ID]			= scv_state_data[SCV_STATE_MAX_CLASS_ID];

    /* If true total count is 0/null, there is no missing values*/
    if (!(scv_state_data[SCV_STATE_TRUE_TOTAL_COUNT]>0))
    {
        scv_state_data[SCV_STATE_TRUE_TOTAL_COUNT] =
        		scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT];
    }

    /* true total count should be greater than 0*/
    dt_check_error
    	(
    		scv_state_data[SCV_STATE_TRUE_TOTAL_COUNT] > 0,
    		"true total count should be greater than 0"
    	);

    /* We use the true total count here in case of missing value. */
    result[SCV_FINAL_TOTAL_COUNT] = scv_state_data[SCV_STATE_TRUE_TOTAL_COUNT];
    float8 ratio 				  = 1;

    /*
     * If there is any missing value, we should multiply a ratio for
     * the computed gain. We already checked
     * scv_state_data[SCV_STATE_TRUE_TOTAL_COUNT] is greater than 0.
     */
    ratio = scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT]/
    		scv_state_data[SCV_STATE_TRUE_TOTAL_COUNT];

    /* We use the true total count to compute the probability */
    result[SCV_FINAL_CLASS_PROB] = 
        scv_state_data[SCV_STATE_MAX_CLASS_ELEM_COUNT]/
        scv_state_data[SCV_STATE_TRUE_TOTAL_COUNT];

    if (!dt_is_float_zero(scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT]))
    {
		dt_check_error_value
		   (
				DT_SC_INFOGAIN  == (int)result[SCV_FINAL_SPLIT_CRITERION] ||
				DT_SC_GAINRATIO == (int)result[SCV_FINAL_SPLIT_CRITERION] ||
				DT_SC_GINI		== (int)result[SCV_FINAL_SPLIT_CRITERION],
				"invalid split criterion: %d. It must be 1, 2 or 3",
				(int)result[SCV_FINAL_SPLIT_CRITERION]
		   );

        if (DT_SC_INFOGAIN  == (int)result[SCV_FINAL_SPLIT_CRITERION] ||
            DT_SC_GAINRATIO == (int)result[SCV_FINAL_SPLIT_CRITERION])
        {
            /* Get the entropy value */
            result[SCV_FINAL_ENTROPY]  = 
                scv_state_data[SCV_STATE_ENTROPY_DATA]/
                scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT];

            /* Get infogain based on initial entropy and current one */
            result[SCV_FINAL_INFO_GAIN] =
            	(init_scv - result[SCV_FINAL_ENTROPY]) * ratio;

            if (DT_SC_GAINRATIO == (int)result[SCV_FINAL_SPLIT_CRITERION])
            {
                /* Compute the value of split info */
                result[SCV_FINAL_SPLIT_INFO] =
                        log(scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT]) -
                        scv_state_data[SCV_STATE_SPLIT_INFO_DATA] /
                        scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT];

                /* Based on infogain and split info, we can compute gainratio */
                if (!dt_is_float_zero(result[SCV_FINAL_SPLIT_INFO]) &&
                    !dt_is_float_zero(result[SCV_FINAL_INFO_GAIN]))
                {
                    dtelog(NOTICE,
                    	   "SCV_FINAL_SPLIT_INFO:%lf,SCV_FINAL_INFO_GAIN:%lf",
                           result[SCV_FINAL_SPLIT_INFO],
                           result[SCV_FINAL_INFO_GAIN]);
                    result[SCV_FINAL_GAIN_RATIO] =
                    	result[SCV_FINAL_INFO_GAIN] / result[SCV_FINAL_SPLIT_INFO];
                }
                else
                {
                    dtelog(NOTICE,
                    	   "zero SCV_FINAL_SPLIT_INFO:%lf,SCV_FINAL_INFO_GAIN:%lf",
                           result[SCV_FINAL_SPLIT_INFO],
                           result[SCV_FINAL_INFO_GAIN]);
                    result[SCV_FINAL_GAIN_RATIO] = 0;
                }
            }
        }
        else
        {
        	/* DT_SC_GINI == (int)result[SCV_FINAL_SPLIT_CRITERION] */

            /* Get the value of gini */
            result[SCV_FINAL_GINI]      =
            	1 - scv_state_data[SCV_STATE_GINI_DATA] /
            	scv_state_data[SCV_STATE_TOTAL_ELEM_COUNT];

            /* Get gain based on initial gini and current gini value */
            result[SCV_FINAL_GINI_GAIN] =
            	(init_scv - result[SCV_FINAL_GINI]) * ratio;
        }
    }
    else
        dt_check_error_value
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
PG_FUNCTION_INFO_V1(dt_scv_aggr_ffunc);


/*
 * @brief The function samples a set of integer values between low and high.
 *        The sample method is 'sample with replacement', which means a case
 *        could be chosen multiple times.
 *
 * @param sample_size     Number of records to be sampled.
 * @param low             Low limit of sampled values.
 * @param high            High limit of sampled values.
 * @param seed            Seed for random number.
 *
 * @return A set of integer values sampled randomly between [low, high].
 *
 */
Datum
dt_sample_within_range
	(
	PG_FUNCTION_ARGS
	)
{
    FuncCallContext     *funcctx 	= NULL;
    int64               call_cntr	= 0;
    int64               max_calls	= 0;

    /* stuff done only on the first call of the function */
    if (SRF_IS_FIRSTCALL())
    {
        MemoryContext   oldcontext;

        /* create a function context for cross-call persistence */
        funcctx = SRF_FIRSTCALL_INIT();

        /* switch to memory context appropriate for multiple function calls */
        oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);
        int64  low = PG_GETARG_INT64(1);
        int64 high = PG_GETARG_INT64(2);

		dt_check_error
			(
				low<=high && low>=0,
				"The low margin must not be greater than the high margin. "
           		"And negative numbers are not accepted"
			);

        /* total number of samples to be returned */
        funcctx->max_calls = PG_GETARG_INT64(0);
        MemoryContextSwitchTo(oldcontext);
    }

    /* stuff done on every call of the function */
    funcctx   = SRF_PERCALL_SETUP();
    call_cntr = funcctx->call_cntr;
    max_calls = funcctx->max_calls;
 
    /* when there is more records to return */
    if (call_cntr < max_calls)
    {
        int64       low			= PG_GETARG_INT64(1);
        int64       high		= PG_GETARG_INT64(2);
        float8      rand_num	= (random()/(float8)(RAND_MAX+1.0));
        int64       selection	= (int64)(rand_num*(high-low+1)+low);
        SRF_RETURN_NEXT(funcctx, Int64GetDatum(selection));
    }

    /* when there is no more records left */
    SRF_RETURN_DONE(funcctx);
}
PG_FUNCTION_INFO_V1(dt_sample_within_range);


/*
 * @brief Retrieve the specified number of unique features for a node.
 *        Discrete features used by ancestor nodes will be excluded.
 *        If the number of remaining features is less or equal than the
 *        requested number of features, then all the remaining features
 *        will be returned. Otherwise, we will sample the requested 
 *        number of features from the remaining features.
 *
 * @param num_req_features  The number of requested features.
 * @param num_features      The total number of features.
 * @param nid               The ID of the node for which the
 *                          features are sampled.
 * @param dp_fids           The IDs of the discrete features
 *                          used by the ancestors.
 *
 * @return An array containing all the IDs of sampled features.
 *
 */
Datum
dt_get_node_split_fids
	(
	PG_FUNCTION_ARGS
	)
{
	int32 num_req_features 	= PG_ARGISNULL(0) ? 0 : PG_GETARG_INT32(0);
	int32 num_features 		= PG_ARGISNULL(1) ? 0 : PG_GETARG_INT32(1);
	int32 nid 				= PG_ARGISNULL(2) ? 0 : PG_GETARG_INT32(2);

    dt_check_error
		(
			num_req_features > 0 && num_features > 0 && nid > 0,
			"the first three arguments can not be null"
		);

	int32 n_remain_fids 	= num_features;
	int32 *dp_fids 			= NULL;
	Datum *result 			= NULL;
	ArrayType *result_array = NULL;
	int32 power_uint32		= 5;

	/* bit map for whether a feature was chosen before or not */
	uint32 n_bitmap = (num_features + (1 << power_uint32) - 1) >> power_uint32;
	uint32 *bitmap 	= (uint32*)palloc0(n_bitmap * sizeof(uint32));

	if (!PG_ARGISNULL(3))
	{
    	ArrayType *dp_fids_array	= PG_GETARG_ARRAYTYPE_P(3);
        int	 dim_nids				= ARR_NDIM(dp_fids_array);
        dt_check_error_value
    		(
    			1 == dim_nids,
    			"invalid array dimension: %d. "
    			"The dimension of the array must be equal to 1",
    			dim_nids
    		);

        int *p_dim_nids 			= ARR_DIMS(dp_fids_array);
        int len_nids 		  		= ArrayGetNItems(dim_nids, p_dim_nids);
        dt_check_error_value
    		(
    			len_nids <= num_features,
    			"invalid array length: %d",
    			len_nids
    		);

        dp_fids 					= (int *)ARR_DATA_PTR(dp_fids_array);
        dt_check_error (dp_fids, "invalid data array");

        /*
         * the feature ID starts from 1
         * if the feature was already chosen, then set the bit to 1
         */
        for (int i = 0; i < len_nids; ++i)
        	bitmap[(dp_fids[i] - 1) >> power_uint32] |=
        			dt_fid_mask((dp_fids[i] - 1), power_uint32);

        n_remain_fids = num_features - len_nids;
	}

	result = palloc0
				(
					((n_remain_fids > num_req_features) ?
							num_req_features :
							n_remain_fids ? n_remain_fids : 1) * sizeof(Datum)
				);
    /*
     * Sample the features if the number of remaining features is greater
	 * than the request number 
     */
    if (n_remain_fids > num_req_features)
    {
		for (int i = 0; i < num_req_features; ++i)
		{
			int32 fid = 0;

			/*
			 * if sample a duplicated number, then sample again until
			 * we found a unique number
			 */
			do
			{
				fid = random() % num_features;
			}while (0 < (bitmap[fid >> power_uint32] & dt_fid_mask(fid, power_uint32)));

			result[i] = Int32GetDatum(fid + 1);

			/* set the bit to true for the sampled number*/
			bitmap[fid >> power_uint32] |= dt_fid_mask(fid, power_uint32);
		}
    }
    else if (0 == n_remain_fids)
    {
    	/*
    	 * if no features left, then simply return any one of the features
    	 * so that the best split information can be retrieved
    	 */
    	num_req_features = 1;
    	result[0] 		 = Int32GetDatum(1);
    }
    else
    {
	    /*
         * If the number of remain features are less than or equal randomly
         * chosen features then return the remain features directly.
         * n_remain_fids <= num_req_features
         */

    	num_req_features = n_remain_fids;

		/* if the features weren't chosen, then choose them */
		for (int32 i = 0; i < num_features; ++i)
			if (0 == (bitmap[i >> power_uint32] & dt_fid_mask(i, power_uint32)))
				result[--n_remain_fids] = Int32GetDatum(i + 1);

		dt_check_error_value
			(
				0 == n_remain_fids,
				"the number of random chosen features is wrong, total:%d",
				n_remain_fids
			);

    }

    /* free the bitmap */
    pfree(bitmap);

    /*
     * the number of elements in the result array must be
     * greater than or equal to 1
     */
	dt_check_error_value
		(
			num_req_features > 0,
			"the number of chosen features for node %d is zero",
			nid
		);

    result_array =
        construct_array(
            result,
            num_req_features,
            INT4OID,
            sizeof(int32),
            true,
            'i'
            );

    PG_RETURN_ARRAYTYPE_P(result_array);
}
PG_FUNCTION_INFO_V1(dt_get_node_split_fids);


/*
 * @brief Use % as the delimiter to split the given string. The char '\' is used
 *        to escape %. We will not change the default behavior of '\' in PG/GP.
 *        For example, assume the given string is E"\\\\\\\\\\%123%123". Then it only
 *        has one delimiter; the string will be split to two substrings:
 *        E'\\\\\\\\\\%123' and '123'; the position array size is 1, where position[0] = 9;
 *        ; (*len) = 13.
 *
 * @param str       The string to be split.
 * @param position  An array to store the position of each un-escaped % in the string.
 * @param num_pos   The expected number of un-escaped %s in the string.
 * @param len       The length of the string. It doesn't include the terminal.
 *
 * @return The position array which records the positions of all un-escaped %s
 *         in the give string.
 *
 * @note If the number of %s in the string is not equal to the expected number,
 *       we will report error via elog.
 */
static
int*
dt_split_string
	(
	char *str,
	int  *position,
    int  num_pos,
	int  *len
	)
{
	int i 				  = 0;
	int j 				  = 0;
	
	/* the number of the escape chars which occur continuously */
	int num_cont_escapes  = 0;

	for(; str != NULL && *str != '\0'; ++str, ++j)
	{
		if ('%' == *str)
		{
			/*
			 * if the number of the escapes is even number
			 * then no need to escape. Otherwise escape the delimiter
			 */
			if (!(num_cont_escapes & 0x01))
			{
            	dt_check_error
	            	(
                        i < num_pos,
                        "the number of the elements in the array is less than "
                        "the format string expects."
                    );

				/*  convert the char '%' to '\0' */
				position[i++] 	= j;
				*str 			= '\0';
			}

			/* reset the number of the continuous escape chars */
			num_cont_escapes = 0;
		}
		else if ('\\' == *str)
		{
			/* increase the number of continuous escape chars */
			++num_cont_escapes;
		}
		else
		{
			/* reset the number of the continuous escape chars */
			num_cont_escapes = 0;
		}
	}

	*len      = j;
	
    dt_check_error
        (
            i == num_pos,
            "the number of the elements in the array is greater than "
            "the format string expects. "
        );

	return position;
}


/*
 * @brief Change all occurrences of '\%' in the give string to '%'. Our split
 *        method will ensure that the char immediately before a '%' must be a '\'.
 *        We traverse the string from left to right, if we meet a '%', then
 *        move the substring after the current '\%' to the right place until
 *        we meet next '\%' or the '\0'. Finally, set the terminal symbol for
 *        the replaced string.
 *
 * @param str   The null terminated string to be escaped.
 *              The char immediately before a '%' must be a '\'.
 *
 * @return The new string with \% changed to %.
 *
 */
static
char*
dt_escape_pct_sym
	(
	char *str
	)
{
	int num_escapes		  = 0;

	/* remember the start address of the escaped string */
	char *p_new_string 	 = str;

	while(str != NULL && *str != '\0')
	{
		if ('%' == *str)
		{
			dt_check_error_value
				(
					(str - 1) && ('\\' == *(str - 1)),
					"The char immediately before a %s must be a \\",
					"%"
				);

			/*
			 * The char immediately before % is \
			 * increase the number of escape chars
			 */
			++num_escapes;
			do
			{
				/*
				 * move the string which is between the current "\%"
				 * and next "\%"
				 */
				*(str - num_escapes) = *str;
				++str;
			} while (str != NULL && *str != '\0' && *str != '%');
		}
		else
		{
			++str;
		}
	}

	/* if there is no escapes, then set the end symbol for the string */
	if (num_escapes > 0)
		*(str - num_escapes) = '\0';

	return p_new_string;
}


/*
 * @brief We need to build a lot of query strings based on a set of arguments. For that
 *        purpose, this function will take a format string (the template) and an array
 *        of values, scan through the format string, and replace the %s in the format
 *        string with the corresponding values in the array. The result string is
 *        returned as a PG/GP text Datum. The escape char for '%' is '\'. And we will
 *        not change it's default behavior in PG/GP. For example, assume that
 *        fmt = E'\\\\\\\\ % \\% %', args[] = {"100", "20"}, then the returned text
 *        of this function is E'\\\\\\\\ 100 % 20'
 *
 * @param fmt       The format string. %s are used to indicate a position
 *                  where a value should be filled in.
 * @param args      An array of values that should be used for replacements.
 *                  args[i] replaces the i-th % in fmt. The array length should
 *                  equal to the number of %s in fmt.
 *
 * @return A string with all %s which were not escaped in first argument replaced
 *         with the corresponding values in the second argument.
 *
 */
Datum
dt_text_format
	(
	PG_FUNCTION_ARGS
	)
{
	dt_check_error
		(
			!(PG_ARGISNULL(0) || PG_ARGISNULL(1)),
			"the format string and its arguments must not be null"
		);

	char	   *fmt 		= text_to_cstring(PG_GETARG_TEXT_PP(0));
	ArrayType  *args_array 	= PG_GETARG_ARRAYTYPE_P(1);

    dt_check_error
		(
			!ARR_NULLBITMAP(args_array),
			"the argument array must not has null value"
		);

	int			nitems		= 0;
	int		   *dims		= NULL;
	int         ndims       = 0;
	Oid			element_type= 0;
	int			typlen		= 0;
	bool		typbyval	= false;
	char		typalign	= '\0';
	char	   *p			= NULL;
	int			i			= 0;

	ArrayMetaState *my_extra= NULL;
	StringInfoData buf;

	ndims  = ARR_NDIM(args_array);
	dims   = ARR_DIMS(args_array);
	nitems = ArrayGetNItems(ndims, dims);

	/* if there are no elements, return the format string directly */
	if (nitems == 0)
		PG_RETURN_TEXT_P(cstring_to_text(fmt));

	int *position 	= (int*)palloc0(nitems * sizeof(int));

	int last_pos    = 0;
	int len_fmt     = 0;

	/*
	 * split the format string, so that later we can replace the delimiters
	 * with the given arguments
	 */
	dt_split_string(fmt, position, nitems, &len_fmt);

	element_type = ARR_ELEMTYPE(args_array);
	initStringInfo(&buf);

	/*
	 * We arrange to look up info about element type, including its output
	 * conversion proc, only once per series of calls, assuming the element
	 * type doesn't change underneath us.
	 */
	my_extra = (ArrayMetaState *) fcinfo->flinfo->fn_extra;
	if (my_extra == NULL)
	{
		fcinfo->flinfo->fn_extra = MemoryContextAlloc
									(
										fcinfo->flinfo->fn_mcxt,
										sizeof(ArrayMetaState)
									);
		my_extra = (ArrayMetaState *) fcinfo->flinfo->fn_extra;
		my_extra->element_type = ~element_type;
	}

	if (my_extra->element_type != element_type)
	{
		/*
		 * Get info about element type, including its output conversion proc
		 */
		get_type_io_data
			(
				element_type,
				IOFunc_output,
				&my_extra->typlen,
				&my_extra->typbyval,
				&my_extra->typalign,
				&my_extra->typdelim,
				&my_extra->typioparam,
				&my_extra->typiofunc
			);
		fmgr_info_cxt
			(
				my_extra->typiofunc,
				&my_extra->proc,
				fcinfo->flinfo->fn_mcxt
			);
		my_extra->element_type = element_type;
	}
	typlen 		= my_extra->typlen;
	typbyval 	= my_extra->typbyval;
	typalign 	= my_extra->typalign;
	p 			= ARR_DATA_PTR(args_array);

	for (i = 0; i < nitems; i++)
	{
		Datum		itemvalue;
		char	   *value;

		itemvalue = fetch_att(p, typbyval, typlen);
		value 	  = OutputFunctionCall(&my_extra->proc, itemvalue);

		/* there is no string before the delimiter */
		if (last_pos == position[i])
		{
			appendStringInfo(&buf, "%s", value);
			++last_pos;
		}
		else
		{
			/*
			 * has a string before the delimiter
			 * we replace "\%" in the string to "%", since "%" is escaped
			 * then combine the string and argument string together
			 */
			appendStringInfo
				(
					&buf,
					"%s%s",
					dt_escape_pct_sym(fmt + last_pos),
					value
				);

			last_pos = position[i] + 1;
		}

		p = att_addlength_pointer(p, typlen, p);
		p = (char *) att_align_nominal(p, typalign);
	}

	/* the last char in the format string is not delimiter */
	if (last_pos < len_fmt)
		appendStringInfo(&buf, "%s", fmt + last_pos);

	PG_RETURN_TEXT_P(cstring_to_text_with_len(buf.data, buf.len));
}
PG_FUNCTION_INFO_V1(dt_text_format);


/*
 * @brief This function checks whether the specified table exists or not.
 *
 * @param input     The table name to be tested.	    
 *
 * @return A boolean value indicating whether the table exists or not.
 */
Datum table_exists(PG_FUNCTION_ARGS)
{
    text*           input;
    List*           names;
    Oid             relid;

    if (PG_ARGISNULL(0))
        PG_RETURN_BOOL(false);
    
    input = PG_GETARG_TEXT_PP(0);

    names = textToQualifiedNameList(input);
    relid = RangeVarGetRelid(makeRangeVarFromNameList(names), true);
    PG_RETURN_BOOL(OidIsValid(relid));
}
PG_FUNCTION_INFO_V1(table_exists);

