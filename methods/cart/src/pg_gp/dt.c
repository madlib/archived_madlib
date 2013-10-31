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
#include "access/tupmacs.h"
#include "utils/array.h"
#include "utils/lsyscache.h"
#include "utils/builtins.h"
#include "utils/typcache.h"
#include "catalog/pg_type.h"
#include "catalog/namespace.h"
#include "lib/stringinfo.h"
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
 * This macro is used to get the mask bit of the given feature
 * id. 
 * fid - ((fid >> power) << power) equals to fid % (2^power)
 */
#define dt_fid_mask(fid, power) \
			(1 << (fid - ((fid >> power) << power)))


/*
 * We use a lot of floating number operations during the training. 
 * For these operations, DBL_EPSILON defined in float.h, leads to error
 * add-up and wrong results. For our calculations, we need to redefine 
 * that to a bigger number. Any floating number whose absolute value is 
 * smaller than the one defined here will be treated as zero. 
 */
#define DT_EPSILON 0.000000001

/*
 * This macro is used to test if a float value is 0.
 * Due to the precision loss of floating numbers, we can not
 * compare them directly with 0.
 */
#define dt_is_float_zero(value)  \
			((value) < DT_EPSILON && (value) > -DT_EPSILON)


/*
 * calculate the value of (val)log(val)
 *
 * @param   val     the value to be calculated
 *
 * NOTE: when x approximates 0, x*log(x) also approximates 0.
 * Therefore, we directly return 0 when v is 0.
 */
#define dt_cal_log(v)  (dt_is_float_zero(v) ? 0.0 : (v) * log(v))

#define dt_cal_sqr(v)  ((v) * (v))

#define dt_cal_sqr_div(v1, v2)  (dt_is_float_zero(v2) ? \
                                0.0 : ((v1) * (v1))/(v2))

/*
 * For Error Based Pruning (EBP), we need to compute the additional errors
 * if the error rate increases to the upper limit of the confidence level.
 * The coefficient is the square of the number of standard deviations
 * corresponding to the selected confidence level.
 * (Excerpt from Documenta Geigy Scientific Tables (Sixth Edition),
 * p185 (with modifications).)
 */
static float8 DT_CONFIDENCE_LEVEL[] =
				{0, 0.001, 0.005, 0.01, 0.05, 0.10, 0.20, 0.40, 1.00};
static float8 DT_CONFIDENCE_DEV[]   =
				{4.0, 3.09, 2.58, 2.33, 1.65, 1.28, 0.84, 0.25, 0.00};


#define MIN_DT_CONFIDENCE_LEVEL 0.001
#define MAX_DT_CONFIDENCE_LEVEL 100.0


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


/* 
 * a forward declaration. 
 */ 
static
float8
dt_ebp_calc_additional_errors
	(
	float8 total_samples,
	float8 num_errors,
	float8 conf_level,
	float8 coeff
	);


/*
 * @brief Calculates the total errors used by Error Based Pruning (EBP).
 *
 * @param total         The number of total samples represented by the node
 *                      being processed.
 * @param probability   The probability to mis-classify samples represented
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
    float8 total_samples 	= PG_GETARG_FLOAT8(0);
    float8 probability 	    = PG_GETARG_FLOAT8(1);
    float8 conf_level 	    = PG_GETARG_FLOAT8(2);
    float8 result 		    = 1.0L;
    float8 coeff 		    = 0.0L;
    unsigned int i 		    = 0;

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
    			total_samples > 0,
    			"invalid number: %lf. "
    			"The number of samples must be greater than 0",
    			total_samples
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

		float8 num_errors = total_samples * (1 - probability);
    	result 			  = dt_ebp_calc_additional_errors
								(
									total_samples,
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
 *        'Error-Based Pruning of Decision Trees Grown on Very Large Data Sets 
 *        Can Work!'.
 *
 * @param total_samples The number of total samples represented by the node
 *                      being processed.
 * @param num_errors    The number of mis-classified samples represented
 *                      by the child nodes if they are pruned with EBP.
 * @param conf_level    A certainty factor to calculate the confidence limits
 *                      for the probability of error using the binomial theorem.
 *
 * @return The additional errors if we prune the node being processed.
 *
 */
static
float8
dt_ebp_calc_additional_errors
    (
	float8 total_samples,
	float8 num_errors,
	float8 conf_level,
	float8 coeff
    )
{
    if (num_errors < 1E-6)
    {
        return total_samples * (1 - exp(log(conf_level) / total_samples));
    }
    else
    if (num_errors < 0.9999)
    {
        float8 tmp = total_samples * (1 - exp(log(conf_level) / total_samples));
        return tmp +
        	   num_errors *
               (
                   dt_ebp_calc_additional_errors
                   	   (total_samples, 1.0, conf_level, coeff) -
                   tmp
               );
    }
    else
    if (num_errors + 0.5 >= total_samples)
    {
        return 0.67 * (total_samples - num_errors);
    }
    else
    {
        float8 tmp =
			(
			num_errors + 0.5 + coeff/2 +
			sqrt(coeff * ((num_errors + 0.5) *
				 (1 - (num_errors + 0.5)/total_samples) + coeff/4))
			)
			/ (total_samples + coeff);

        return (total_samples * tmp - num_errors);
    }
}


/*
 * @brief The step function for aggregating the class counts while
 *        doing Reduce Error Pruning (REP).
 *        The input for this aggregation is the result of an internal join
 *        between validation set's classification result and encoded table.
 *
 * @param class_count_array     The array used to store the accumulated information.
 *                              [0]: the total number of mis-classified samples
 *                              [i]: the number of samples belonging to the ith class
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
        
        dt_check_error
    		(
    			!ARR_HASNULL(pg_class_count),
    			"dt_rep_aggr_class_count_sfunc cannot accept arrays with NULL values"
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
    			"dt_rep_aggr_class_count_sfunc invalid array length: %d. "
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

    /* In any sample, we will update the original class count */
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
 * @brief It takes two bigint arrays and add them together.
 *        If this function is used in an aggregation's context,
 *        we store the added information to 
 *
 * @param 1 arg         The array 1.
 * @param 2 arg         The array 2.
 *
 * @return The array with the added information.
 *
 */
Datum
bigint_array_add
	(
	PG_FUNCTION_ARGS
	)
{
    ArrayType *pg_array1    	= NULL;
    int array_dim               = 0;
    int *p_array_dim            = NULL;
    int array_length            = 0;
    int64 *array1         		= NULL;

    ArrayType *pg_array2   	= NULL;
    int array_dim2          = 0;
    int *p_array_dim2       = NULL;
    int array_length2       = 0;
    int64 *array2        	= NULL;

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
        /* If both arrays are not null, we will add them together */
        if (fcinfo->context && IsA(fcinfo->context, AggState))
        {
            /* We can safely modify the original array in an aggregate */
            pg_array1 = PG_GETARG_ARRAYTYPE_P(0);
        }
        else
        {
            /* 
             * We must not modify the original array out of aggregate's
             * context. We simply use copy here to avoid the tedious work
             * to allocate new arrays. There is no explicit facility to 
             * do that.
             */ 
            pg_array1 = PG_GETARG_ARRAYTYPE_P_COPY(0);
        }
        
        dt_check_error
    		(
    			!ARR_HASNULL(pg_array1),
    			"bigint_array_add cannot accept arrays with NULL values"
    		);

        dt_check_error
    		(
    			pg_array1,
    			"invalid aggregation state array"
    		);

        array_dim = ARR_NDIM(pg_array1);

        dt_check_error_value
    		(
    			array_dim == 1,
    			"invalid array dimension: %d. "
    			"The dimension of array1 must be equal to 1",
    			array_dim
    		);

        p_array_dim     = ARR_DIMS(pg_array1);
        array_length    = ArrayGetNItems(array_dim,p_array_dim);
        array1        	= (int64 *)ARR_DATA_PTR(pg_array1);

        pg_array2      	= PG_GETARG_ARRAYTYPE_P(1);
        array_dim2      = ARR_NDIM(pg_array2);
        dt_check_error_value
    		(
    			array_dim2 == 1,
    			"invalid array dimension: %d. "
    			"The dimension of array2 must be equal to 1",
    			array_dim2
    		);

        p_array_dim2        = ARR_DIMS(pg_array2);
        array_length2       = ArrayGetNItems(array_dim2,p_array_dim2);
        array2              = (int64 *)ARR_DATA_PTR(pg_array2);

        dt_check_error
    		(
    			array_length == array_length2,
    			"the size of the two array must be the same"
    		);

        dt_check_error
    		(
    			!ARR_HASNULL(pg_array2),
    			"bigint_array_add cannot accept arrays with NULL values"
    		);

        for (int index = 0; index < array_length; index++)
            array1[index] += array2[index];

        PG_RETURN_ARRAYTYPE_P(pg_array1);
    }
}
PG_FUNCTION_INFO_V1(bigint_array_add);


/*
 * @brief The final function for aggregating the class counts for REP.
 *        It takes the class count array produced by the step function.
 *        
 * @param class_count_array     The array used to store the accumulated information.
 *                              [0]: the total number of mis-classified samples
 *                              [i]: the number of samples belonging to the ith class
 *
 * @return A two-element array. The first element is the ID of the class that
 *         has the maximum number of samples represented by the root node of
 *         the subtree being processed. The second element is the number of
 *         reduced misclassified samples if the leaf nodes of the subtree are pruned.
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

    dt_check_error
        (
            !ARR_HASNULL(pg_class_count),
            "dt_rep_aggr_class_count_ffunc cannot accept arrays with NULL values"
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

    /* maxid is the id of the class, which has the most samples */
    result[0] = maxid;

    /*
     * (sum - max) is the number of mis-classified samples represented by
     * the root node of the subtree being processed
     * class_count_data[0] the total number of mis-classified samples
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
 * 	Calculating Split Criteria Values (SCVs for short) is a major
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
 *   tid | nid | fid | split_value | is_cont |  le   | total 
 *  -----+-----+-----+-------------+---------+-------+-------
 *     1 |   1 |   4 |             | f       | {2,6} | {5,9}
 *     1 |   1 |   4 |             | f       | {3,3} | {5,9}
 *     1 |   1 |   3 |             | f       | {2,3} | {5,9}
 *     1 |   1 |   3 |             | f       | {0,4} | {5,9}
 *     1 |   1 |   3 |             | f       | {3,2} | {5,9}
 *     1 |   1 |   2 |          64 | t       | {0,1} | {5,9}
 *     1 |   1 |   2 |          65 | t       | {1,1} | {5,9}
 *     1 |   1 |   2 |          68 | t       | {1,2} | {5,9}
 *     1 |   1 |   2 |          69 | t       | {1,3} | {5,9}
 *     1 |   1 |   2 |          70 | t       | {1,4} | {5,9}
 *     1 |   1 |   2 |          71 | t       | {2,4} | {5,9}
 *     1 |   1 |   2 |          72 | t       | {3,5} | {5,9}
 *     1 |   1 |   2 |          75 | t       | {3,7} | {5,9}
 *     1 |   1 |   2 |          80 | t       | {4,7} | {5,9}
 *     1 |   1 |   2 |          81 | t       | {4,8} | {5,9}
 *     1 |   1 |   2 |          83 | t       | {4,9} | {5,9}
 *     1 |   1 |   2 |          85 | t       | {5,9} | {5,9}
 *     1 |   1 |   1 |          65 | t       | {0,1} | {5,9}
 *     1 |   1 |   1 |          70 | t       | {1,3} | {5,9}
 *     1 |   1 |   1 |          75 | t       | {1,4} | {5,9}
 *     1 |   1 |   1 |          78 | t       | {1,5} | {5,9}
 *     1 |   1 |   1 |          80 | t       | {2,7} | {5,9}
 *     1 |   1 |   1 |          85 | t       | {3,7} | {5,9}
 *     1 |   1 |   1 |          90 | t       | {4,8} | {5,9}
 *     1 |   1 |   1 |          95 | t       | {5,8} | {5,9}
 *     1 |   1 |   1 |          96 | t       | {5,9} | {5,9}
 *  (26 rows) 
 *
 * The fields of ACS is explained below. 
 *  tid     The ID of the tree.
 *
 *  nid     The ID of the node in the specified tree.
 *
 *  fid     The ID of the selected feature.
 *
 *  split_value 
 *          For continuous features, each distinct value is one candidate 
 *          split value. For discrete features, this field is always NULL.
 *
 *  is_cont Whether the feature fid is continuous or not. This column can be 
 *          eliminated if we check (split_value IS NOT NULL) 
 *
 *  le      An m-element array, where m is the total number of distinct 
 *          classes. le[i] is the number of samples whose class labels are 
 *          class i and whose feature fid holds a distinct value equal to 
 *          (for discrete features) or less-than or equal to (for continuous 
 *          features) the feature value corresponding to the current row. The 
 *          corresponding value is split_value for a continous feature, or one 
 *          of its distinct values for a discrete feature. 
 *
 *  total   An m-element array, where m is the total number of distinct classes. 
 *          total[i] is the total number of samples whose class labels are class i. 
 *
 * The rows are grouped by (tid, nid, fid, split_value). For a discrete feature,
 * split_value always contains NULL. For a discrete feature with n distinct values,
 * the group for that feature contains n rows. For a continuous feature, 
 * its group has only one row. For each group, we will calculate an SCV based 
 * on the specified splitting criterion and then choose the split with the 
 * maximum scv value. Because groups are independent, calculating SCVs can be done 
 * in parallel. 
 *
 * Given the format of the input data stream, SCV calculation is different
 * from using the SCV formulas directly. There is one row for each distinct 
 * value of a feature. For information gain, we can further transform the 
 * formula as below. We assume there are n distinct values for feature a and
 * m distinct classes. We denote c[j] as the total number of samples whose class
 * labels are class j. The cardinality of S is defined as |S|. |Si| is the 
 * total count of samples whose feature value is the ith distinct value. We 
 * denote d[i][j] as the count of samples whose class is j and feature value is  
 * the ith distinct value.
 *
 * We define the entropy of S, denoted as info(S), as: 
 * 
 *      info(S) = (c[1]/|S|)log(|S|/c[1])+...+(c[m]/|S|)log(|S|/c[m])
 *
 * Suppose using the distinct values of feature a, S is split into n subsets 
 * {S1, S2, ..., Sn}. We define info(S, a) as the weighted entropy of all the 
 * subsets after splitting S using feature a: 
 *
 *      info(S, a) = (|S1|/|S|)info(S1)+...+(|Sn|/|S|)info(Sn)
 * 
 * The information gain of using a to split S can be defined as: 
 *
 *      IG(S, a)= info(S) - info(S, a) 
 *              = log(t) - ( u + v - w ) / t, 
 *
 * where t, u, v and w are defined as:
 *
 *      t = |S|
 *      u = (c[1])log(c[1])+...+(c[m])log(c[m]) 
 *      v = |S1|log(|S1|)+...+|Sn|log(|Sn|) 
 *      w = (d[1][1])log(d[1][1])+(d[1][2])log(d[1][2])+...+(d[n][m])log(d[n][m])
 * 
 * In the above formulas, c[j] actually equals to total[j] within the ACS set. 
 * |S| equals to the sum of all elements in total. For the i-th distinct value 
 * of a discrete feature, d[i][j] equals to le[j] of the ACS row corresponding
 * to the i-th value. With that, |Si| then equals to the sum of all d[i][j]s. 
 * 
 * Therefore, we can define an aggregate function to process the rows in ACS 
 * to calcualte the information gain of all features. The aggregate can calculate 
 * t, u, v, and w incrementally as the rows come in. Their intermediate values will 
 * be kept in the aggregate state variables. In the final function, we can get the
 * information with log(t) - ( u + v - w ) / t. 
 *
 * This way, we successfully remove the need to keep all attribute-class counts 
 * in a possibly very big in-memory array. The calculation process fits quite 
 * well with the aggregate mechanism, which are widely available on most data 
 * processing systems.
 *
 * When using gain ratio as the split criterion, besides IG(S, a), we also need
 * Split_info(S, a), which can be defined as: 
 *
 *      Split_info(S, a) = (|S1|/|S|)log(|S|/|S1|)+...+(|Sn|/|S|)log(|S|/|Sn|)
 *
 * With ACS in place, we can get |S| and |Si| for each incoming row, based on which 
 * part of Split_info can be calculated. Then in the final function, the gain ratio 
 * of using a to split S can be calculated as: 
 *      
 *      GR(S, a) = IG(S, a) / Split_info(s, a)
 *
 * For gini, the computation can be reduced to formula below.
 *
 *      GI(S, a) = (W1/V1+W2/V2+...+Wn/Vn)/t - u/(t^2)
 *
 * where u,t,Wi and Vi is defined below. 
 *
 *      t  = |S|
 *      u  = (c[1])^2+(c[2])^2+...+(c[m])^2.
 *      Wi = (d[i][1])^2+(d[i][2])^2+...+(d[i][m])^2. 
 *      Vi = d[i][1]+d[i][2]+...+d[i][m]
 *
 * We do not need to store Wi and Vi into separate variables. Instead, we only
 * need two variables to keep the accumulated results of Wi and Vi.
 * This way the gini index can also be calculated with aggregates using constant
 * memory.
 *
 * Based on this understanding, we will define the following structures,
 * types, and aggregate functions to calculate SCVs.
 *
 */


/*
 * We use a 9-element array to keep the state of the
 * aggregate for calculating splitting criteria values (SCVs).
 * The enum types defines which element of that array is used
 * for which purpose.
 *
 */
enum DT_SCV_STATE_ARRAY_INDEX
{
    /* 1 infogain, 2 gainratio, 3 gini */
    SCV_CODE = 0,
    
    /* is continuous or not*/
    SCV_IS_CONT,

    /* the u component */
    SCV_U,

    /* the v component */
    SCV_V,

    /* the w component */
    SCV_W,

    /* the t component */
    SCV_T,

    /* the total number of samples in the training set */
    SCV_SAMPLE_TOTAL,  

    /* the ID of the class with the largest number of samples */
    SCV_MAX_CLASS_ID,

    /* the total number of samples belonging to MAX_CLASS */
    SCV_MAX_CLASS_COUNT

};


/*
 * We use a 5-element array to keep the final result of the
 * aggregate for calculating splitting criteria values (SCVs).
 * The enum types defines which element of that array is used
 * for which purpose.
 *
 */
enum DT_SCV_FINAL_ARRAY_INDEX
{
    /* Calculated SCV */
    SCV_FINAL_VALUE     = 0,

    /* Whether the selected feature is continuous or discrete */
    SCV_FINAL_IS_CONT,

    /* The ID of the class with the largest number of samples */
    SCV_FINAL_CLASS_ID,

    /* The percentage of samples belonging to MAX_CLASS */
    SCV_FINAL_CLASS_PROB,

    /* Total count of samples */
    SCV_FINAL_TOTAL_COUNT
};


/* Codes for different split criteria. */
#define DT_SC_INFOGAIN  1
#define DT_SC_GAINRATIO 2
#define DT_SC_GINI      3


/*
 * @brief The step function for the aggregation used to find the best SCV.
 *
 * @param best_scv_array    This array stores the internal aggregation state. Its
 *                          definition is the same as the returned array.
 * @param scv_final_array   This array contains the computed splitting criteria 
 *                          values. Please refer to the definition of 
 *                          DT_SCV_FINAL_ARRAY_INDEX.
 * @param fid               The ID of the feature used by this split.
 * @param split_value       The split_value for this split. For discrete features,
 *                          it is always NULL.
 *
 * @return A seven-element array. Please refer to the definition of 
 *         DT_SCV_FINAL_ARRAY_INDEX for the first five elements. The
 *         last two elements of this array is fid and split_value.
 */
Datum
dt_best_scv_sfunc
	(
	PG_FUNCTION_ARGS
	)
{
    ArrayType*	best_scv_array	= NULL;
    if (fcinfo->context && IsA(fcinfo->context, AggState))
        best_scv_array = PG_GETARG_ARRAYTYPE_P(0);
    else
        best_scv_array = PG_GETARG_ARRAYTYPE_P_COPY(0);

	dt_check_error
		(
			best_scv_array,
			"invalid aggregation state array"
		);
    
    dt_check_error
        (
            !ARR_HASNULL(best_scv_array),
            "the first array passed to dt_best_scv_sfunc cannot contain NULL values"
        );

    int	 array_dim 		= ARR_NDIM(best_scv_array);

    dt_check_error_value
		(
			array_dim == 1,
			"invalid array dimension: %d. "
			"The dimension of scv state array must be equal to 1",
			array_dim
		);

    int* p_array_dim	= ARR_DIMS(best_scv_array);
    int  array_length	= ArrayGetNItems(array_dim, p_array_dim);

    dt_check_error_value
		(
			array_length == SCV_FINAL_TOTAL_COUNT + 3,
			"dt_best_scv_sfunc invalid array length: %d",
			array_length
		);

    float8 *best_scv_data = (float8 *)ARR_DATA_PTR(best_scv_array);
	dt_check_error
		(
			best_scv_data,
			"invalid aggregation data array"
		);

    // scv array
    ArrayType*	scv_array	= PG_GETARG_ARRAYTYPE_P(1);
	dt_check_error(scv_array, "invalid scv array");
    array_dim 	        	= ARR_NDIM(scv_array);
    dt_check_error(array_dim == 1, 
                   "the dimension of scv array must be equal to 1");
    dt_check_error
        (
            !ARR_HASNULL(scv_array),
            "the second array passed to dt_best_scv_sfunc cannot contain NULL values"
        );

    p_array_dim	    = ARR_DIMS(scv_array);
    array_length	= ArrayGetNItems(array_dim, p_array_dim);

    dt_check_error_value
		(
			array_length == SCV_FINAL_TOTAL_COUNT + 1,
			"dt_best_scv_sfunc invalid array length: %d",
			array_length
		);

    float8 *scv_data = (float8 *)ARR_DATA_PTR(scv_array);
	dt_check_error(scv_data, "invalid scv data array");

    float8 scvdiff  = 0.0;
    int i           = 0;
    int fid	        = PG_GETARG_INT32(2);
    float8 sp_val   = PG_GETARG_FLOAT8(3);

    scvdiff = scv_data[SCV_FINAL_VALUE] - best_scv_data[SCV_FINAL_VALUE];

    dtelog( NOTICE, 
            "cur:%lf, %lf, best:%lf, %lf", 
            scv_data[SCV_FINAL_VALUE], 
            fid, 
            best_scv_data[SCV_FINAL_VALUE], 
            best_scv_data[SCV_FINAL_TOTAL_COUNT + 1]);

    /*
     *  When the SCVs for two features tie, we will use the fid and split_value
     *  as the tie breakers. This ensures that we consistently choose the same 
     *  feature/splitting value as the split. 
     */     
    if ( (scvdiff > DT_EPSILON) ||
         (
            dt_is_float_zero(scvdiff) && 
                (
                 (best_scv_data[SCV_FINAL_TOTAL_COUNT + 1] < fid) ||
                    ( dt_is_float_zero
                         (
                         best_scv_data[SCV_FINAL_TOTAL_COUNT + 1]-fid
                         ) && 
                      best_scv_data[SCV_FINAL_TOTAL_COUNT + 2] < sp_val
                    )
                )
         ) 
       )
    {
        for (i = 0; i <= SCV_FINAL_TOTAL_COUNT; ++i)
        {
            best_scv_data[i] = scv_data[i];
        }
        
        best_scv_data[i]        = fid;
        best_scv_data[i + 1]    = sp_val;
    }
          
    PG_RETURN_ARRAYTYPE_P(best_scv_array);
}
PG_FUNCTION_INFO_V1(dt_best_scv_sfunc);


/*
 * @brief The pre-function for finding the best splitting criteria values.
 *
 * @param scv_state_array    The array from sfunc1.
 * @param scv_state_array    The array from sfunc2.
 *
 * @return A seven element array. Please refer to the definition of 
 *         DT_SCV_FINAL_ARRAY_INDEX for the first five elements. The
 *         last two elements of this array is fid and split_value.
 *
 */
Datum
dt_best_scv_prefunc
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

    dt_check_error
        (
            !ARR_HASNULL(scv_state_array),
            "the first array passed to dt_best_scv_prefunc cannot contain NULL values"
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
			array_length == SCV_FINAL_TOTAL_COUNT + 3,
			"dt_scv_aggr_prefunc invalid array length: %d",
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

    dt_check_error
        (
            !ARR_HASNULL(scv_state_array2),
            "the second array passed to dt_best_scv_prefunc cannot contain NULL values"
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
			array_length == SCV_FINAL_TOTAL_COUNT + 3,
			"dt_scv_aggr_prefunc invalid array length: %d",
			array_length
		);

    /* the scv state data from another segment */
    float8 *scv_state_data2 = (float8 *)ARR_DATA_PTR(scv_state_array2);
	dt_check_error
		(
			scv_state_data2,
			"invalid aggregation data array"
		); 

    float8 scvdiff = scv_state_data2[SCV_FINAL_VALUE] - 
                     scv_state_data[SCV_FINAL_VALUE];
    int i       = 0;

    float8  array2_fid      = scv_state_data2[SCV_FINAL_TOTAL_COUNT + 1];
    float8  array2_sp_val   = scv_state_data2[SCV_FINAL_TOTAL_COUNT + 2];

    float8  array1_fid      = scv_state_data[SCV_FINAL_TOTAL_COUNT + 1];
    float8  array1_sp_val   = scv_state_data[SCV_FINAL_TOTAL_COUNT + 2];
    
    /*
     *  When the SCVs for two features tie, we will use the fid and split_value
     *  as the tie breakers. This ensures that we consistently choose the same 
     *  feature/splitting value as the split. 
     */ 
    if ((scvdiff > DT_EPSILON) ||
        (
            dt_is_float_zero(scvdiff) && 
                (
                 (array1_fid < array2_fid) ||
                    ( dt_is_float_zero
                         (
                         array1_fid-array2_fid
                         ) && 
                      array1_sp_val < array2_sp_val
                    )
                )
        )
       )
    {
        for (i = 0; i <= SCV_FINAL_TOTAL_COUNT + 2; ++i)
        {
            scv_state_data[i] = scv_state_data2[i];
        }
    }

    PG_RETURN_ARRAYTYPE_P(scv_state_array);
}
PG_FUNCTION_INFO_V1(dt_best_scv_prefunc);


/*
 * @brief The step function for the aggregation of SCV.
 *        It accumulates all the information for SCV calculation
 *        and stores to a nine-element array.
 *
 * @param scv_state_array   The array used to accumulate all the information
 *                          for the calculation of SCV.
 *                          Please refer to the definition of 
 *                          DT_SCV_STATE_ARRAY_INDEX.
 * @param sc_code           1- infogain; 2- gainratio; 3- gini.
 * @param feature_val       The feature value of current record under processing.
 * @param class             The class of current record under processing.
 * @param is_cont_feature   True  - The feature is continuous. 
 *                          False - The feature is discrete.
 * @param le                The le component of an ACS record.
 * @param total             The total component of an ACS record.
 * @param true_total_count  If there is any missing value, true_total_count is larger
 *      				    than the total count computed in the aggregation. Thus,
 *                          we should multiply a ratio for the computed gain.
 *
 * @return A nine-element array. Please refer to the definition of 
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

    dt_check_error
        (
            !ARR_HASNULL(scv_state_array),
            "the first array passed to dt_scv_aggr_sfunc cannot contain NULL values"
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
			array_length == SCV_MAX_CLASS_COUNT + 1,
			"dt_scv_aggr_sfunc invalid array length: %d",
			array_length
		);

    float8 *scv_state_data = (float8 *)ARR_DATA_PTR(scv_state_array);
	dt_check_error
		(
			scv_state_data,
			"invalid aggregation data array"
		);

    int sc_type	        = PG_GETARG_INT32(1);
    bool is_cont_feat 	= PG_ARGISNULL(2) ? 0 : PG_GETARG_BOOL(2);
    int num_class     	= PG_ARGISNULL(3) ? 0 : PG_GETARG_INT32(3);
    
    // we only read the data from le-array and total-array 
    ArrayType* le_array =  PG_GETARG_ARRAYTYPE_P(4);
    dt_check_error(le_array, "invalid le array");
    array_dim 	    	= ARR_NDIM(le_array);
    dt_check_error(array_dim == 1, "the dimemsion of le array must be 1");
    p_array_dim	        = ARR_DIMS(le_array);
    array_length	    = ArrayGetNItems(array_dim, p_array_dim);
    dt_check_error
        (
            array_length == num_class, 
            "the size of le array must be the number of class"
        );
    float8* le_data     = (float8 *)ARR_DATA_PTR(le_array);
    
    // total array
    ArrayType* total_array  =  PG_GETARG_ARRAYTYPE_P(5);
    dt_check_error(total_array, "invalid total array");
    array_dim 	    	    = ARR_NDIM(total_array);
    dt_check_error(array_dim == 1, "the dimemsion of total array must be 1");
    p_array_dim	            = ARR_DIMS(total_array);
    array_length	        = ArrayGetNItems(array_dim, p_array_dim);
    dt_check_error
        (
            array_length == num_class, 
            "the size of total array must be the number of class"
        );
    float8* total_data  = (float8 *)ARR_DATA_PTR(total_array);

    int i               = 0;
    float8 feat_le      = 0.0;
    float8 feat_cnts    = 0.0;

	dt_check_error_value
		(
			DT_SC_INFOGAIN  == sc_type ||
			DT_SC_GAINRATIO == sc_type ||
			DT_SC_GINI      == sc_type,
			"invalid split criterion: %d. "
			"It must be 1(infogain), 2(gainratio) or 3(gini)",
			sc_type
		);
    
    scv_state_data[SCV_CODE] 		    = sc_type;
    scv_state_data[SCV_SAMPLE_TOTAL]    = PG_ARGISNULL(6) ? 0 : PG_GETARG_INT64(6);
    scv_state_data[SCV_IS_CONT]         = is_cont_feat;

    dtelog(NOTICE, "array: %lf, %lf, %lf, %lf", 
       le_data[0], le_data[1], total_data[0], total_data[1]);

    // processing the continuous feature
    if (is_cont_feat)
    {
        // the definitions of t, u, v and w are the same between IG and GR
        if (DT_SC_INFOGAIN == sc_type || DT_SC_GAINRATIO == sc_type)
        {
            scv_state_data[SCV_MAX_CLASS_COUNT] = 0;
            for (i = 0; i < num_class; ++i)
            {
        	    dt_check_error_value
		        (
                    total_data[i] >= le_data[i],
                    "the difference: %lf",
                    total_data[i] - le_data[i]
                );

                feat_le     += le_data[i];
                feat_cnts   += total_data[i];

                // max class count and ID
                if (scv_state_data[SCV_MAX_CLASS_COUNT] < total_data[i])
                {
                    scv_state_data[SCV_MAX_CLASS_COUNT] = total_data[i];
                    scv_state_data[SCV_MAX_CLASS_ID]    = i + 1;
                }

                // calculate the statistic info for class 
                scv_state_data[SCV_U] += dt_cal_log(total_data[i]);

                // calculate the statistic info for the class label and the feature value
                scv_state_data[SCV_W] += 
                    (dt_cal_log(le_data[i]) + dt_cal_log(total_data[i] - le_data[i]));
            }

            // calculate the statistic info for the feature
            scv_state_data[SCV_V] += 
                (dt_cal_log(feat_le) + dt_cal_log(feat_cnts - feat_le));

            // calculate the number of non-null elements
            scv_state_data[SCV_T] = feat_cnts; 
        }
        else
        {
            // gini index
            scv_state_data[SCV_MAX_CLASS_COUNT] = 0;
            for (i = 0; i < num_class; ++i)
            {
        	    dt_check_error_value
		        (
                    total_data[i] >= le_data[i],
                    "the difference: %lf",
                    total_data[i] - le_data[i]
                );

                feat_le     += le_data[i];
                feat_cnts   += total_data[i];

                // max class count and ID
                if (scv_state_data[SCV_MAX_CLASS_COUNT] < total_data[i])
                {
                    scv_state_data[SCV_MAX_CLASS_COUNT] = total_data[i];
                    scv_state_data[SCV_MAX_CLASS_ID]    = i + 1;
                }

                // calculate the statistic info for class 
                scv_state_data[SCV_U] += dt_cal_sqr(total_data[i]);
            }

            // calculate the number of non-null elements
            scv_state_data[SCV_T] = feat_cnts; 
        
            // calculate the statistic info for the class label and the feature value
            feat_cnts -= feat_le;

            for (i = 0; i < num_class; ++i)
            {
                scv_state_data[SCV_W] += 
                    (
                        dt_cal_sqr_div(le_data[i], feat_le) +
                        dt_cal_sqr_div(total_data[i] - le_data[i], feat_cnts)
                    );
            }
        }
    }
    else // processing the discrete feature
    {
        // the definitions of t, u, v and w are the same between IG and GR
        if (DT_SC_INFOGAIN == sc_type || DT_SC_GAINRATIO == sc_type)
        {
            /*
             * calculate the value of count, the max class and class info
             * we only need to write once
             */
            if (dt_is_float_zero(scv_state_data[SCV_T]))
            {
                scv_state_data[SCV_MAX_CLASS_COUNT] = 0;                
                for (i = 0; i < num_class; ++i)
                {
                    feat_cnts   += total_data[i];
                    
                    // max class count and ID
                    if (scv_state_data[SCV_MAX_CLASS_COUNT] < total_data[i])
                    {
                        scv_state_data[SCV_MAX_CLASS_COUNT] = total_data[i];
                        scv_state_data[SCV_MAX_CLASS_ID]    = i + 1;
                    }
                    
                    // calculate the statistic info for class 
                    scv_state_data[SCV_U] += dt_cal_log(total_data[i]);
                }
                
                // calculate the count
                scv_state_data[SCV_T] = feat_cnts;
            }

            // calculate the statistic info for the class label and the feature value
            for (i = 0; i < num_class; ++i)
            {
                scv_state_data[SCV_W] += dt_cal_log(le_data[i]);
                feat_le     += le_data[i];
            }
                    
            // calculate the statistic info for the feature
            scv_state_data[SCV_V] += dt_cal_log(feat_le);
        }
        else
        {
            /*
             * calculate the value of count, the max class and class info
             * we only need to write once
             */
            if (dt_is_float_zero(scv_state_data[SCV_T]))
            {
                scv_state_data[SCV_MAX_CLASS_COUNT] = 0;                
                for (i = 0; i < num_class; ++i)
                {
                    feat_cnts   += total_data[i];
                    
                    // max class count and ID
                    if (scv_state_data[SCV_MAX_CLASS_COUNT] < total_data[i])
                    {
                        scv_state_data[SCV_MAX_CLASS_COUNT] = total_data[i];
                        scv_state_data[SCV_MAX_CLASS_ID]    = i + 1;
                    }
                   
                    // calculate the statistic info for class 
                    scv_state_data[SCV_U] += dt_cal_sqr(total_data[i]);
                }
                
                // calculate the count
                scv_state_data[SCV_T] = feat_cnts;
            }

            // calculate the statistic info for the class label and the feature value
            for (i = 0; i < num_class; ++i)
            {
                feat_le     += le_data[i];
            }

            for (i = 0; i < num_class; ++i)
            {
                scv_state_data[SCV_W] += dt_cal_sqr_div(le_data[i], feat_le);
            }
        }
    }
    dtelog(NOTICE, "data: %lf, %lf, %lf, %lf", 
        scv_state_data[SCV_W], 
        scv_state_data[SCV_U],
        scv_state_data[SCV_V],
        scv_state_data[SCV_T]);

    PG_RETURN_ARRAYTYPE_P(scv_state_array);
}
PG_FUNCTION_INFO_V1(dt_scv_aggr_sfunc);


/*
 * @brief   The pre-function for the aggregation of SCV. It takes the state 
 *          array produced by two sfunc and combine them together.
 *
 * @param scv_state_array    The array from sfunc1.
 * @param scv_state_array    The array from sfunc2.
 *
 * @return A nine-element array. Please refer to the definition of 
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

    dt_check_error
        (
            !ARR_HASNULL(scv_state_array),
            "the first array passed to dt_scv_aggr_prefunc cannot contain NULL values"
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
			array_length == SCV_MAX_CLASS_COUNT + 1,
			"dt_scv_aggr_prefunc invalid array length: %d",
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

    dt_check_error
        (
            !ARR_HASNULL(scv_state_array2),
            "the second array passed to dt_scv_aggr_prefunc cannot contain NULL values"
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
			array_length == SCV_MAX_CLASS_COUNT + 1,
			"dt_scv_aggr_prefunc invalid array length: %d",
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
    scv_state_data[SCV_W]	+= scv_state_data2[SCV_W];
    scv_state_data[SCV_V] += scv_state_data2[SCV_V];

    if (dt_is_float_zero(scv_state_data[SCV_T]))
    {
        scv_state_data[SCV_T]  = scv_state_data2[SCV_T];
        scv_state_data[SCV_U]  = scv_state_data2[SCV_U];
        scv_state_data[SCV_IS_CONT]     =  scv_state_data2[SCV_IS_CONT];
        scv_state_data[SCV_CODE]        =  scv_state_data2[SCV_CODE];
    }

    /*
     *  We should compare the results from different segments and
     *  find the class with maximum samples.
     */ 
    if (scv_state_data[SCV_MAX_CLASS_COUNT] <
    	scv_state_data2[SCV_MAX_CLASS_COUNT])
    {
        scv_state_data[SCV_MAX_CLASS_COUNT]	=
        		scv_state_data2[SCV_MAX_CLASS_COUNT];
        scv_state_data[SCV_MAX_CLASS_ID]	=
        		scv_state_data2[SCV_MAX_CLASS_ID];
    }

    PG_RETURN_ARRAYTYPE_P(scv_state_array);
}
PG_FUNCTION_INFO_V1(dt_scv_aggr_prefunc);


/*
 * @brief The final function for the aggregation of SCV.
 *        It takes the state array produced by the prefunc and produces
 *        a five-element array.
 *
 * @param scv_state_array  The array containing all the information for the 
 *                         calculation of SCV. 
 *
 * @return A five-element array. Please refer to the definition of 
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

    dt_check_error
        (
            !ARR_HASNULL(scv_state_array),
            "the first array passed to dt_scv_aggr_ffunc cannot contain NULL values"
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
			array_length == SCV_MAX_CLASS_COUNT + 1,
			"dt_scv_aggr_ffunc: invalid array length: %d",
			array_length
		);

    dtelog(NOTICE, "dt_scv_aggr_ffunc array_length:%d",array_length);

    float8 *scv_state_data = (float8 *)ARR_DATA_PTR(scv_state_array);
    dt_check_error
    	(
    		scv_state_data,
    		"invalid aggregation data array"
    	);


    dtelog(NOTICE, "final: %lf, %lf, %lf, %lf", 
        scv_state_data[SCV_W], 
        scv_state_data[SCV_U],
        scv_state_data[SCV_V],
        scv_state_data[SCV_T]);

    int result_size	= SCV_FINAL_TOTAL_COUNT + 1;
    float8 *result	= palloc0(sizeof(float8) * result_size);
    float8 tmp      = 0.0;

    dtelog( NOTICE, 
            "total:%lf, %lf",
            scv_state_data[SCV_SAMPLE_TOTAL], 
            scv_state_data[SCV_T]);

    /* If true total count is 0/null, there is no missing values*/
    if (dt_is_float_zero(scv_state_data[SCV_SAMPLE_TOTAL]))
    {
        scv_state_data[SCV_SAMPLE_TOTAL] =
        		scv_state_data[SCV_T];
    }

    /* true total count should be greater than 0*/
    dt_check_error
    	(
    		scv_state_data[SCV_SAMPLE_TOTAL] > 0 && scv_state_data[SCV_T] > 0,
    		"true total count should be greater than 0"
    	);

    /* 
     * For the following elements, such as max class id, we should copy
     * them from step function array to final function array for returning.
     */
    result[SCV_FINAL_CLASS_ID]	    = scv_state_data[SCV_MAX_CLASS_ID];
    result[SCV_FINAL_IS_CONT]   	= scv_state_data[SCV_IS_CONT];
    result[SCV_FINAL_TOTAL_COUNT]   = scv_state_data[SCV_SAMPLE_TOTAL];
    result[SCV_FINAL_CLASS_PROB]    = 
        scv_state_data[SCV_MAX_CLASS_COUNT] / scv_state_data[SCV_SAMPLE_TOTAL];


    if (DT_SC_INFOGAIN == ((int)scv_state_data[SCV_CODE]))
    {
        // info gain
        result[SCV_FINAL_VALUE] = 
            log(scv_state_data[SCV_T]) - 
             ((scv_state_data[SCV_U] + scv_state_data[SCV_V] -
              scv_state_data[SCV_W]) / scv_state_data[SCV_T]);
    }
    else if (DT_SC_GAINRATIO == ((int)scv_state_data[SCV_CODE]))
    {
        // gain ratio
        tmp = dt_cal_log(scv_state_data[SCV_T]) - scv_state_data[SCV_V];
        result[SCV_FINAL_VALUE] = dt_is_float_zero(tmp) ? 0.0 :
            1 + (scv_state_data[SCV_W] - scv_state_data[SCV_U]) / tmp;
    }
    else
    {
        //gini index
        result[SCV_FINAL_VALUE] = 
            (scv_state_data[SCV_W] / scv_state_data[SCV_T]) -
            (scv_state_data[SCV_U]) / dt_cal_sqr(scv_state_data[SCV_T]);
    }
    
    result[SCV_FINAL_VALUE] *= (scv_state_data[SCV_T] / 
            scv_state_data[SCV_SAMPLE_TOTAL]); 

    dtelog(NOTICE, "final value: %lf", result[SCV_FINAL_VALUE]);

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
 *        The sample method is 'sample with replacement', which means a sample
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

        dt_check_error_value
            (
                !ARR_HASNULL(dp_fids_array),
                "the first array passed to %s cannot contain NULL values",
                __FUNCTION__
            );

        int *p_dim_nids 			= ARR_DIMS(dp_fids_array);
        int len_nids 		  		= ArrayGetNItems(dim_nids, p_dim_nids);
        dt_check_error_value
    		(
    			len_nids <= num_features,
    			"dt_get_node_split_fids invalid array length: %d",
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
			}
            while (0 < (bitmap[fid >> power_uint32] & dt_fid_mask(fid, power_uint32)));

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
	char *p_new_string 	  = str;

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

    dt_check_error_value
        (
            !ARR_HASNULL(args_array),
            "the first array passed to %s cannot contain NULL values",
            __FUNCTION__
        );

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
#if PG_VERSION_NUM >= 90200
    relid = RangeVarGetRelid(makeRangeVarFromNameList(names), NoLock, true);
#else
    relid = RangeVarGetRelid(makeRangeVarFromNameList(names), true);
#endif
    PG_RETURN_BOOL(OidIsValid(relid));
}
PG_FUNCTION_INFO_V1(table_exists);


/*
 * @brief The step function for generating the acc counts.
 *
 * @param class_count_array     The array used to store the count information.
 *                              The length of the array equals max_num_of_classes.
 * @param max_num_of_classes    The total number of distinct class values.
 * @param count                 The count value to be accumulated.
 * @param class                 The current class value.
 *
 * @return The updated version of class_count_array. 
 *
 */
Datum
dt_acc_count_sfunc
	(
	PG_FUNCTION_ARGS
	)
{
    ArrayType *pg_count_array    = NULL;
    int array_dim                = 0;
    int *p_array_dim             = NULL;
    int array_length             = 0;
    int64 *count_array           = NULL;
    dt_check_error_value
		(
			!PG_ARGISNULL(1),
			"In function: %s. "
			"The parameter of 'max_num_of_classes' should not be null",
			__FUNCTION__
		);
    int max_num_of_classes  	 = PG_GETARG_INT32(1);
    int64  count                 = PG_ARGISNULL(2)?0:PG_GETARG_INT64(2);
    int    class                 = PG_ARGISNULL(3)?0:PG_GETARG_INT32(3);
    bool rebuild_array     		 = false;

    dt_check_error_value
		(
			max_num_of_classes >= 2 && max_num_of_classes <= 1e6,
			"invalid value: %d. "
			"The number of classes must be in the range of [2, 1e6]",
			max_num_of_classes
		);

    dt_check_error_value
		(
			class >= 1 && class <= max_num_of_classes,
			"invalid real class value: %d. "
			"It must be in range from 1 to the number of classes",
			class
		);

    /* test if the first argument (class count array) is null */
    if (PG_ARGISNULL(0))
    {
    	/*
    	 * We assume the maximum number of classes is limited (up to millions),
    	 * so that the allocated array won't break our memory limitation.
    	 */
        count_array 	 = palloc0(sizeof(int64) * max_num_of_classes);
        array_length 	 = max_num_of_classes;
        rebuild_array 	 = true;

    }
    else
    {
        if (fcinfo->context && IsA(fcinfo->context, AggState))
            pg_count_array = PG_GETARG_ARRAYTYPE_P(0);
        else
            pg_count_array = PG_GETARG_ARRAYTYPE_P_COPY(0);
        
        dt_check_error
    		(
    			pg_count_array,
    			"invalid aggregation state array"
    		);

        dt_check_error_value
            (
                !ARR_HASNULL(pg_count_array),
                "the first array passed to %s cannot contain NULL values",
                __FUNCTION__
            );

        array_dim = ARR_NDIM(pg_count_array);

        dt_check_error_value
    		(
    			array_dim == 1,
    			"invalid array dimension: %d. "
    			"The dimension of class count array must be equal to 1",
    			array_dim
    		);

        p_array_dim         = ARR_DIMS(pg_count_array);
        array_length        = ArrayGetNItems(array_dim,p_array_dim);
        count_array    		= (int64 *)ARR_DATA_PTR(pg_count_array);

        dt_check_error_value
    		(
    			array_length == max_num_of_classes,
    			"dt_acc_count_sfunc invalid array length: %d. "
    			"The length of class count array must be "
    			"equal to the total number classes",
    			array_length
    		);
    }

    count_array[class - 1] += count;

    if (rebuild_array)
    {
        /* construct a new array to keep the aggr states. */
        pg_count_array =
        	construct_array(
        		(Datum *)count_array,
                array_length,
                INT8OID,
                sizeof(int64),
                true,
                'd'
                );
    }

    PG_RETURN_ARRAYTYPE_P(pg_count_array);
}
PG_FUNCTION_INFO_V1(dt_acc_count_sfunc);


/*
 * @brief Cast a value to text. On some databases, there
 *        are no such casts for certain data types, such as
 *        the cast for bool to text. 
 *
 * @param value   The value with any specific type
 *
 * @note This is a strict function.
 *
 */
Datum
dt_to_text
    (
    PG_FUNCTION_ARGS
    )
{
    Datum   value            = PG_GETARG_DATUM(0);
    Oid     valtype          = get_fn_expr_argtype(fcinfo->flinfo, 0);
    Oid		typoutput        = 0;
    bool    typIsVarlena     = 0;
    char   *result           = NULL;

    getTypeOutputInfo(valtype, &typoutput, &typIsVarlena);

    // call the output function of the type to convert 
    result = OidOutputFunctionCall(typoutput, value);

    PG_RETURN_TEXT_P(cstring_to_text(result));
}
PG_FUNCTION_INFO_V1(dt_to_text);


/*
 * @brief The step function of the aggregate array_indexed_agg.
 *        To avoid allocating memory in each step function and manipulating
 *        the array bitmap for null values, we keep the null values by 
 *        ourself. The solution is that, we use two items in the state 
 *        array to represent one result item. The 2*i-th item in the state 
 *        array represents the actual value of the i-th result item, 
 *        and the 2*i+1-th item in the state array represents whether 
 *        the i-th result item is NULL.
 *
 * @param state         The step state array of the aggregate function.
 * @param elem          The element to be filled into the state array.
 * @param elem_cnt      The number of elements.
 * @param elem_idx      The subscript of "elem" in the state array.
 * 
 */
Datum dt_array_indexed_agg_sfunc(PG_FUNCTION_ARGS)
{
	ArrayType       *state;
	ArrayBuildState build_state;
	Datum           elem;
	Oid             elem_typ = FLOAT8OID;
	int32_t         elem_cnt;
	int32_t         elem_idx;
	int32_t         iterator_idx;
	
    dt_check_error_value
        (
            (fcinfo->context && IsA(fcinfo->context, AggState)),
            "%s can only be used in aggregations",
            __FUNCTION__
        );

	state       = PG_ARGISNULL(0) ? NULL : PG_GETARG_ARRAYTYPE_P(0);
	elem        = PG_ARGISNULL(1) ? (Datum) 0 : PG_GETARG_DATUM(1);    
	elem_cnt    = PG_GETARG_INT64(2);
	elem_idx    = PG_GETARG_INT64(3) - 1;

    dt_check_error_value
        (
            elem_cnt > 0,
            "array_size:%d should be bigger than zero",
            elem_cnt
        );

    dt_check_error_value
        (
            elem_idx >= 0 && elem_idx < elem_cnt,
            "the subscript %d is out of range",
            elem_idx
        );

	get_typlenbyvalalign
        (
         elem_typ, 
         &build_state.typlen, 
         &build_state.typbyval, 
         &build_state.typalign
        );

	if (NULL == state)
	{
		build_state.mcontext = NULL;

		/* 
		 * allocate two element for each index, the first one is the value, 
		 * the second one indicates whether the item is null 
		 */ 
		build_state.alen    = (elem_cnt << 1);
		build_state.dvalues = (Datum *) palloc(build_state.alen * sizeof(Datum));
		build_state.dnulls  = NULL;
		build_state.nelems  = build_state.alen;
		build_state.element_type = elem_typ;

		for (iterator_idx = 0; iterator_idx < build_state.alen; iterator_idx++)
		{
			build_state.dvalues[iterator_idx]   = Float8GetDatum(1);
		}

		/* put the elem into the target slot */
		build_state.dvalues[elem_idx << 1]       = elem;
		build_state.dvalues[(elem_idx << 1) + 1] =  
			Float8GetDatum(PG_ARGISNULL(1) ? 1 : 0);

		state = construct_array(build_state.dvalues, build_state.nelems,
			build_state.element_type, build_state.typlen, 
			build_state.typbyval, build_state.typalign);

		PG_RETURN_ARRAYTYPE_P(state);
	}

    dt_check_error_value
        (
            !ARR_HASNULL(state),
            "the first array passed to %s cannot contain NULL values",
            __FUNCTION__
        );

    dt_check_error_value
        (
            ARR_DIMS(state)[0] == (elem_cnt << 1),
            "The dimension of state array should be %d",
            (elem_cnt << 1)
        );

	((float8*)ARR_DATA_PTR(state))[(elem_idx << 1)]     = DatumGetFloat8(elem);
	((float8*)ARR_DATA_PTR(state))[(elem_idx << 1) + 1] = PG_ARGISNULL(1) ? 1 : 0;
	
	PG_RETURN_ARRAYTYPE_P(state);
}
PG_FUNCTION_INFO_V1(dt_array_indexed_agg_sfunc);


/*
 * @brief The pre-function of the aggregate array_indexed_agg.
 * 
 * @param arg0  The first state array.
 * @param arg1  The second state array.
 *  
 * @return The combined state.  
 *
 */
Datum dt_array_indexed_agg_prefunc(PG_FUNCTION_ARGS)
{
	ArrayType   *arg0,  *arg1;
	int64       iterator_idx;
	int32       elem_cnt;
    int64       elem_idx;

    dt_check_error_value
        (
            (fcinfo->context && IsA(fcinfo->context, AggState)),
            "%s can only be used in aggregations",
            __FUNCTION__
        );

	arg0 = PG_ARGISNULL(0) ? NULL : PG_GETARG_ARRAYTYPE_P(0);
	arg1 = PG_ARGISNULL(1) ? NULL : PG_GETARG_ARRAYTYPE_P(1);

	if (NULL == arg0)
	{
		PG_RETURN_ARRAYTYPE_P(arg1);
	}
	else if (NULL == arg1)
	{
		PG_RETURN_ARRAYTYPE_P(arg0);
	}

    dt_check_error
        (
            ARR_NDIM(arg0) == ARR_NDIM(arg1),
            "the dimension of the two state array should be the same"
        );

    dt_check_error
		(
			 1 == ARR_NDIM(arg0),
			"the dimension of state array must be equal to 1"
		);

    dt_check_error
        (
            ARR_DIMS(arg0)[0] == ARR_DIMS(arg1)[0],
            "the size of the two state array must be the same"
        );

	elem_cnt = (ARR_DIMS(arg0)[0]) >> 1;

	for (iterator_idx = 0; iterator_idx < elem_cnt; iterator_idx++)
	{
        elem_idx = iterator_idx << 1;

		/* 
		 * just taking the non-null one, pre-steps must make 
		 * sure there is no duplicate 
		 */
		if (0 == (int)((float8*)ARR_DATA_PTR(arg1))[elem_idx + 1])
		{
			((float8*)ARR_DATA_PTR(arg0))[elem_idx] = 
                ((float8*)ARR_DATA_PTR(arg1))[elem_idx];
			((float8*)ARR_DATA_PTR(arg0))[elem_idx + 1] = 0;
		}
	}

	PG_RETURN_ARRAYTYPE_P(arg0);
}
PG_FUNCTION_INFO_V1(dt_array_indexed_agg_prefunc);


/*
 * @brief The final function of array_indexed_agg.
 * 
 * @param state  The state array.
 * 
 * @return The aggregate result.
 *
 */
Datum dt_array_indexed_agg_ffunc(PG_FUNCTION_ARGS)
{
	ArrayType   *state, *result;
	ArrayBuildState build_state;
	Oid         elem_typ = FLOAT8OID;
	int32_t     elem_cnt; 
	int32_t     iterator_idx;
	int 		lbs[1];

    dt_check_error_value
        (
            (fcinfo->context && IsA(fcinfo->context, AggState)),
            "%s can only be used in aggregations",
            __FUNCTION__
        );

	state = PG_ARGISNULL(0) ? NULL : PG_GETARG_ARRAYTYPE_P(0); 

    dt_check_error
		(
			 NULL != state,
			"the state array that fed into the final aggregate "
            "should not be null"
		);

    dt_check_error
		(
			 1 == ARR_NDIM(state),
			"the dimension of the state array should be equal to 1"
		);

    dt_check_error
		(
			 0 == (ARR_DIMS(state)[0] & 0x01),
			"invalid state array"
		);

	elem_cnt = (ARR_DIMS(state)[0]) >> 1;

	get_typlenbyvalalign
        (
            elem_typ, 
            &build_state.typlen, 
            &build_state.typbyval, 
            &build_state.typalign
        );

	build_state.mcontext    = NULL;
	build_state.alen        = elem_cnt;
	build_state.dvalues     = (Datum *) palloc(build_state.alen * sizeof(Datum));
	build_state.dnulls      = (bool *) palloc(build_state.alen * sizeof(bool));
	build_state.nelems      = build_state.alen;
	build_state.element_type= elem_typ;

	for (iterator_idx = 0; iterator_idx < elem_cnt; iterator_idx ++)
	{
		build_state.dnulls[iterator_idx] = 
            (int)((float8*)ARR_DATA_PTR(state))[(iterator_idx << 1) + 1]; 
		build_state.dvalues[iterator_idx] = 
            Float8GetDatum(((float8*)ARR_DATA_PTR(state))[(iterator_idx << 1)]);
	}

	lbs[0] = 1;
	result = construct_md_array
        (
            build_state.dvalues, 
            build_state.dnulls, 
            1, 
            &(build_state.nelems), 
            lbs, 
		    build_state.element_type, 
            build_state.typlen, 
            build_state.typbyval, 
            build_state.typalign
        );

	PG_RETURN_ARRAYTYPE_P(result);
}
PG_FUNCTION_INFO_V1(dt_array_indexed_agg_ffunc);
