/* ----------------------------------------------------------------------- *//**
 *
 * @file decision_tree.c
 *
 * @brief decision tree related utility functions written with C
 *
 *//* -------------------------------------------------------------------- */
#include "postgres.h"
#include "fmgr.h"
#include "utils/array.h"
#include "math.h"
#include "catalog/pg_type.h"

/*
 * Copy from float_specials.h of svec to deal with the NVP value
 * If we can directly refer to the header file, then we should use it instead.
 */
#define MKINT64(x) (x##ULL)
#define NEG_QUIET_NAN_MIN64    MKINT64(0xFFF8000000000001)
#define NVP_i		NEG_QUIET_NAN_MIN64
static const uint64 COMPVEC_I[] = {NVP_i};
static const double *COMPVEC = (double *)COMPVEC_I;
#define NVP		COMPVEC[0]
#define IS_NVP(x)  (memcmp(x,&(NVP),sizeof(double)) == 0)

#ifndef NO_PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

/*
 * This function judge whether certain element in Array is null.
 * That array should be already converted to bitmap.
 *
 * Parameters:
 *      bitmap: The bitmap converted from the original array.
 *      offset: The index of the element to be tested. 0 is the first 
 *              element in the array.
 * Return:
 *      true:  The specified element is null.
 *      false: The specified element is not null.
 */
static bool array_is_null
    (
        const bits8*    bitmap, 
        int             offset
    )
{
	return bitmap == 0 ? false : !(bitmap[offset / 8] & (1 << (offset % 8)));
}

/*
 * For float value, we cannot directly compare it with zero.
 * This function is used to judge whether a float value is 0.
 *
 * Parameters:
 *      value:      a float value
 *
 * Return:
 *      True:       The value passed in is zero.
 *      False:      The value passed in is not zero.
 *
 */
static bool is_float_zero(float8 value)
{
    //double only has up to 15 valid digits
    float8 margin_value = 1E-13;
    //margin_value *= margin_value;

    //elog(WARNING,"margin_value:%lf",margin_value);
    if( value < margin_value && value > -margin_value )
        return true;
    else
        return false;
}

/*
 * For float value, we cannot directly compare two float values.
 * This function is used to compare them.
 *
 * Parameters:
 *      value1:      a float value
 *      value2:      a float value
 *
 * Return:
 *      True:       a<b.
 *      False:      a>=b.
 *
 */
static bool is_float_less
        (
            float8 value1,
            float8 value2
        )
{
    //double only has up to 15 valid digits
    float8 margin_value = 1E-13;
    //margin_value *= margin_value;

    if( value1-value2 < margin_value )
        return true;
    else
        return false;
}

Datum hash_array( PG_FUNCTION_ARGS);

/*
 * This function computes one hash value from one
 * integer array. 
 *
 * Parameters:
 *      first para: The integer array.
 * Return:
 *      The integer hash value.
 */
PG_FUNCTION_INFO_V1(hash_array);
Datum hash_array( PG_FUNCTION_ARGS)
{
    ArrayType *state  = PG_GETARG_ARRAYTYPE_P(0);
    int dimstate = ARR_NDIM(state);
    int *dimsstate = ARR_DIMS(state);
    int numstate = ArrayGetNItems(dimstate,dimsstate);
    int32 *vals_state=(int32 *)ARR_DATA_PTR(state);

    unsigned long hash = 65599;
    unsigned short c;
    int i = 0;

    for (;i<numstate;i++)
    {
        c = vals_state[i];
        hash = c + (hash << 7) + (hash << 16) - hash;
    }

    PG_RETURN_INT32(hash);
}

/*
 * Below two arrays are taken from Documenta Geigy Scientific
 * Tables (Sixth Edition), p185 (with modifications).)
 */
static float CONFIDENCE_LEVEL[] = {  0,  0.001, 0.005, 0.01, 0.05, 0.10, 0.20, 0.40, 1.00},
      CONFIDENCE_DEV[] = {4.0,  3.09,  2.58,  2.33, 1.65, 1.28, 0.84, 0.25, 0.00};

/*
 * Compute the addition errors for each node. This algorithm is based on
 * the definiton of 'error-based pruning'.
 * The detailed description of that pruning strategy can be found in the paper
 * of 'Error-Based Pruning of Decision Trees Grown on Very Large Data Sets Can Work!'.
 *
 * Parameters:
 *      totalCases:  It specifies how many cases in the training set fall into
 *                   the current branch.
 *      numOfErrors: This argument should be equal to 
 *                   'totalCases - cases belonging to maxClass'.
 *      confLevel:   Please refer to the paper for detailed definition
 * Return:
 *      The computed additional error that should be added to the original error
 *      number.
 *
 */
static float compute_added_errors
    (
        float totalCases, 
        float numOfErrors, 
        float confLevel
    )
{
    static float coeff=0;

    if ( ! coeff )
    {
        int i = 0;
        while ( confLevel > CONFIDENCE_LEVEL[i] ) i++;

        coeff = CONFIDENCE_DEV[i-1] +
                  (CONFIDENCE_DEV[i] - CONFIDENCE_DEV[i-1]) *
                  (confLevel - CONFIDENCE_LEVEL[i-1]) /
                  (CONFIDENCE_LEVEL[i] - CONFIDENCE_LEVEL[i-1]);

        coeff = coeff * coeff;
    }

    if (numOfErrors < 1E-6)
    {
        return totalCases * (1 - exp(log(confLevel) / totalCases));
    }
    else
    if (numOfErrors < 0.9999)
    {
        float tmp = totalCases * (1 - exp(log(confLevel) / totalCases));
        return tmp + numOfErrors * 
            (compute_added_errors(totalCases, 1.0, confLevel) - tmp);
    }
    else
    if (numOfErrors + 0.5 >= totalCases)
    {
        return 0.67 * (totalCases - numOfErrors);
    }
    else
    {
        float tmp = (numOfErrors + 0.5 + coeff/2
                + sqrt(coeff * ((numOfErrors + 0.5) * (1 - (numOfErrors + 0.5)/totalCases) + coeff/4)) )
             / (totalCases + coeff);
        return (totalCases * tmp - numOfErrors);
    }
}

/*
 * Compute the total error used by error based pruning.
 *
 * Parameters:
 *      totalCases: It specifies how many cases in the training set fall into
 *                  the current branch.
 *      maxCases:   This argument specifies how many cases belong to the maxClass
 *                  for all those cases in current branch. maxClass means that class
 *                  has more cases than any other classes.
 *      confLevel:  Please refer to the paper for detailed definition.
 *                  The paper's name is 'Error-Based Pruning of Decision Trees Grown 
 *                  on Very Large Data Sets Can Work!'.
 * Return:
 *      The computed total error
 *      
 */

static float get_ebp_total_error
    (
        float totalCases, 
        float maxCases, 
        float confLevel
    )
{
	float extraError = totalCases - maxCases;
	return compute_added_errors(totalCases, extraError, confLevel) 
        + extraError;
}

/*
 * Compute the chisquare value, which is used in the chisquare 
 * pre-pruning algorithm.
 *
 * Parameters:
 *      values:     This arrays contain all those numbers used for 
 *                  chisquare calculation.
 *      from:       It specifies the index from which the calculation 
 *                  should begin.
 *      size:       It specifies the total number of values used for
 *                  chisquare calculation.
 *      fract:      It specifies the number of cases belonging to this
 *                  fraction.
 *      total:      It specifies the total number of cases.
 * Return:
 *      The computed chisquare value.
 *
 */
static float8 chi_square_statistic
    (
        float8* values, 
        int clsTotalFrom,
        int     from, 
        int     size, 
        float8  fract, 
        float8  total
    )
{
    int i = from, j = clsTotalFrom;
    float8 mult = 0;
    float8 estimate = 0;
    float8 result = 0;

    if(is_float_zero(fract) 
            ||is_float_zero(total))
    {
        return 0;
    }
    mult = fract/total;
    for(; i < (size+from); ++i, ++j){
        estimate = ((float8)values[j])*mult;
        if(estimate > 0){
            result += ((float8)values[i] - estimate)*((float8)values[i] - estimate)/estimate;
        }
    }
    return result;
}




/*
 * Compute the weighted entropy value based on float8 input. It is used to 
 * measure information's the impurity.
 *
 * Parameters:
 *      values:     This arrays contain all those numbers used for 
 *                  entropy calculation.
 *      from:       It specifies the index from which the calculation 
 *                  should begin.
 *      size:       It specifies the total number of values used for
 *                  entropy calculation.
 *      fract:      It specifies the number of cases belonging to this
 *                  fraction.
 *      total:      It specifies the total number of cases.
 * Return:
 *      The computed entropy value.
 */
static float8 entropy_weighted_float
    (
        float8*     values, 
        int         from, 
        int         size, 
        float8      fract, 
        float8      total
    )
{
    int i = from;
    float8 mult = 0;
    float8 result = 0;

    //elog(NOTICE, "entropy_weighted_float: fract: %f, %f, %f, %f, %f", 
    //      fract, values[i], values[i+1], values[i+2], values[i+3]);
    if(is_float_zero(fract)
            ||is_float_zero(total)){
        return 0;
    }
    mult = fract/total;
    for(; i < (size+from); ++i){
        if(values[i] > 0){
            result -= (values[i]/fract)*log(values[i]/fract);
        }
    }
    return result*mult;
}


/*
 * This function acts as the entry for accumulating all the information
 * used for Reduced-Error Pruning to an array.
 *
 * Parameters:
 *      vals_state:         The array used to store the accumulated information.
 *      classifiedClass:    The classified class based on our trained DT model.
 *      originalClass:      The true class value provided in the validation set. 
 * Return:
 *      The array containing all the information for the calculation of 
 *      Reduced-Error pruning. 
 *      The first element is the number of wrongly classified cases.
 *      The following elements are the true number of cases for each class.
 */
Datum aggr_rep(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(aggr_rep);
Datum aggr_rep(PG_FUNCTION_ARGS) {
    ArrayType *state  = PG_GETARG_ARRAYTYPE_P(0);
    int dimstate = ARR_NDIM(state);
    int *dimsstate = ARR_DIMS(state);
    int numstate = ArrayGetNItems(dimstate,dimsstate);
    int64 *vals_state=(int64 *)ARR_DATA_PTR(state);

    int classifiedClass = PG_GETARG_INT32(1);
    int originalClass = PG_GETARG_INT32(2);
    ArrayType *pgarray;

    if(originalClass != classifiedClass)
    	++vals_state[0];

    ++vals_state[originalClass];

    pgarray = construct_array((Datum *)vals_state,
    		numstate,FLOAT8OID,
            sizeof(int64),true,'d');
    PG_RETURN_ARRAYTYPE_P(pgarray);
}

/*
 * This function acts as the entry for the calculation of the 
 * Reduced-Error Pruning based on the accumulated information.
 *
 * Parameters:
 *      vals_state:         The array containing all the information for the 
 *                          calculation of Reduced-Error pruning. 
 * Return:
 *      The array containing  the calculation result of 
 *      Reduced-Error pruning. 
 *      The first element is the id of the class, which has the most cases.
 *      The following element reflect the change of mis-classified cases
 *      if the current branch is pruned. 
 *      If that element is greater than zero, we will consider pruning that 
 *      branch
 */
Datum compute_rep(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(compute_rep);
Datum compute_rep(PG_FUNCTION_ARGS) {
    ArrayType *state  = PG_GETARG_ARRAYTYPE_P(0);
    int dimstate = ARR_NDIM(state);
    int *dimsstate = ARR_DIMS(state);
    int numstate = ArrayGetNItems(dimstate,dimsstate);
    int64 *vals_state=(int64 *)ARR_DATA_PTR(state);

    int64 *result = palloc(sizeof(int64)*2);

    ArrayType *pgarray;

    int64 max = vals_state[1];
    int64 sum = max;
    int maxid = 1;

    for(int i = 2; i < numstate; ++i)
    {
    	if(max < vals_state[i])
    	{
    		max = vals_state[i];
    		maxid = i;
    	}

    	sum += vals_state[i];

    }
    //maxid is the id of the class, which has the most cases
    result[0] = maxid;

    //(sum - max) is the number of mis-classified cases after 
    //pruning the current branch.
    //
    //vals_state[0] is the number of mis-classified cases before
    //pruning the current branch.
    result[1] = vals_state[0] - (sum - max);
    /*elog(NOTICE, "compute_rep: %ld, %ld", result[0], result[1]);*/

    pgarray = construct_array((Datum *)result,
            2,FLOAT8OID,
            sizeof(int64),true,'d');
    PG_RETURN_ARRAYTYPE_P(pgarray);
}

/*
 * Compute the weighted gini value based on float8 input. It is used to 
 * measure information's the impurity.
 *
 * Parameters:
 *      values:     This arrays contain all those numbers used for 
 *                  gini calculation.
 *      from:       It specifies the index from which the calculation 
 *                  should begin.
 *      size:       It specifies the total number of values used for
 *                  gini calculation.
 *      fract:      It specifies the number of cases belonging to this
 *                  fraction.
 *      total:      It specifies the total number of cases.
 * Return:
 *      The computed gini value.
 */
static float8 gini_weighted_float
    (
        float8*     values, 
        int         from, 
        int         size, 
        float8      fract, 
        float8      total
     )
{
    int i = from;
    float8 mult = 0;
    float8 result = 1;

    if(is_float_zero(fract)||is_float_zero(total)){
        return 0;
    }
    mult = fract/total;
    for(; i < (size+from); ++i){
        if(values[i] > 0){
            float8 temp = (values[i]/fract);
            result -= temp*temp;
        }
    }
    return result*mult;
}

Datum aggr_infogain(PG_FUNCTION_ARGS);

/*
 * This function accumulate the distribution information in one big array.
 * The distribution information is used to calculate split gains.
 *
 * Parameters:
 *      0 vals_state:         The array used to store the accumulated information.
 *      1 colValArray:        The array containing the values of various features
 *                            for current record.
 *      2 colValCntArray:     The array containing the number of distinct values
 *                            for each feature.
 *      3 contSplitValArray:  The array containing all the candidate split values
 *                            for each continuous feature.
 *      4 trueweight:         The weight of current record. (We represent multiple
 *                            duplicate records with just one row. We use 'weight' to
 *                            denote how many records there are originally. 
 *      5 numOfClasses:       The total number of distinct classes.   
 *      6 trueclass:          The true class this record belongs to.
 *      7 has_cont_feature:   For all those features, is one of them is continuous.
 * Return:
 *      The array containing all the information for the calculation of 
 *      split gain. 
 */
PG_FUNCTION_INFO_V1(aggr_infogain);
Datum aggr_infogain(PG_FUNCTION_ARGS) {
    ArrayType *state  = PG_GETARG_ARRAYTYPE_P(0);
    int dim = ARR_NDIM(state);
    int *pdims = ARR_DIMS(state);
    int numOfState = ArrayGetNItems(dim, pdims);
    float8 *vals_state=(float8 *)ARR_DATA_PTR(state);


    ArrayType *colValArray  = PG_GETARG_ARRAYTYPE_P(1);
    dim = ARR_NDIM(colValArray);
    pdims = ARR_DIMS(colValArray);
    int numOfVals = ArrayGetNItems(dim, pdims);
    float8 *colVals = (float8 *)ARR_DATA_PTR(colValArray);


    ArrayType *colValCntArray  = PG_GETARG_ARRAYTYPE_P(2);
    dim = ARR_NDIM(colValCntArray);
    pdims = ARR_DIMS(colValCntArray);
    int numOfColValCntVals = ArrayGetNItems(dim, pdims);
    int32 *colValCnt = (int32 *)ARR_DATA_PTR(colValCntArray);

    bool has_cont_feature = PG_GETARG_BOOL(7);

    ArrayType *contSplitValArray  = 0;
    int numOfContSplitVals = 0;
    float8 *contSplitVals = 0;

    if (has_cont_feature)
    {
    	contSplitValArray = PG_GETARG_ARRAYTYPE_P(3);
		dim = ARR_NDIM(contSplitValArray);
		pdims = ARR_DIMS(contSplitValArray);
		numOfContSplitVals = ArrayGetNItems(dim, pdims);
		contSplitVals = (float8 *)ARR_DATA_PTR(contSplitValArray);
    }

    float8 trueweight = PG_GETARG_FLOAT8(4);
    int32 numOfClasses = PG_GETARG_INT32(5);
    int32 trueclass = PG_GETARG_INT32(6);

    ArrayType *pgarray;

    int64 i = 0;
    int64 j = 0;
    int64 begin = 0;
    int64 position = 0;

    int32 numOfContFeatures = 0;
    bool* isContFeatureArray = 0;
    int64 splitBegin = 0;
    int64 totalBegin = 1;
    int32 contFeatIdx = 2;
    int32 numOfValues = 0;

    bits8* colValsBitmap = 0;
    bool hasNullColVal = false;
    float8 columnValue = 0.0;
    bool isNullValue = false;

	if(ARR_HASNULL(colValArray)){
		hasNullColVal = true;
		colValsBitmap = ARR_NULLBITMAP(colValArray);
	}

    //elog(NOTICE, "weight:%f, classes:%d, class:%d, cont:%d",trueweight, numOfClasses, trueclass, has_cont_feature );
    if(has_cont_feature)
    {
    	numOfContFeatures = (int32)contSplitVals[0];
    	isContFeatureArray = palloc(sizeof(bool)*numOfVals);
    	memset(isContFeatureArray, 0, sizeof(bool)*numOfVals);

    	splitBegin = (numOfContFeatures << 1) + 1;;

		for(i = 1; i < (numOfContFeatures << 1); i += 2)
		{
			isContFeatureArray[(int32)contSplitVals[i] - 1] = true;
		}

    }

    if (numOfVals != numOfColValCntVals)
    {
    	elog(ERROR, "invalid num!");
    }
    /*elog(NOTICE, "is_continuous: %d, true_value: %lf, split_value: %lf,true_weight: %lf, posclasses: %d, trueclass: %d",*/
   /*         is_cont_feature, truevalue,split_value, trueweight, posclasses, trueclass);*/

	begin = (numOfClasses + 1) * numOfColValCntVals + totalBegin;

	vals_state[0] += trueweight;

	for (i = 0; i < numOfColValCntVals; ++i)
	{
		isNullValue = IS_NVP(colVals + i);
		columnValue = colVals[i];
		//elog(NOTICE, "0 value: %f, cnt: %d, index: %d, value: %f, NAN: %f", vals_state[0], numOfColValCntVals, i, colVals[i], NAN);

		if (has_cont_feature && isContFeatureArray[i])
		{
			numOfValues = contSplitVals[contFeatIdx];
			contFeatIdx += 2;

			// no values in this column, ignore
			// in the future, we should not pass this column value
			// the aggregate function.
			if (numOfValues <= 0 )
				continue;

			// this cell is null, skip it
			// but adjust the begin position for next feature
			if (isNullValue)
			{
				totalBegin += 1 + numOfClasses;
				begin += (2 + (numOfClasses << 1)) * numOfValues;
				splitBegin += numOfValues;
				continue;
			}

			vals_state[totalBegin] += trueweight;
			vals_state[totalBegin + trueclass] += trueweight;
			totalBegin += 1 + numOfClasses;

			for (j = 0; j < numOfValues; ++j)
			{
				position = begin + 2 + trueclass - 1;

				if (columnValue <= contSplitVals[splitBegin] )
				{
					vals_state[begin] += trueweight;
					vals_state[position] += trueweight;

				}
				else
				{
					vals_state[begin + 1] += trueweight;
					vals_state[position + numOfClasses] += trueweight;
				}

				begin += 2 + (numOfClasses << 1);
				++splitBegin;
			}
		}
		else
		{
			// this cell is null, skip it
			// but adjust the begin position for next feature
			if (isNullValue)
			{
				totalBegin += 1 + numOfClasses;
				begin += colValCnt[i] * (numOfClasses + 1);
				continue;
			}

            if(columnValue<1 || columnValue > colValCnt[i])
            {
                elog(WARNING,"for discrete feature, value should between 1 and %d,"
                        "realvalue is:%f",
                        colValCnt[i],columnValue);
            }

		    vals_state[totalBegin] += trueweight;
		    vals_state[totalBegin + trueclass] += trueweight;
		    totalBegin += 1 + numOfClasses;

			// f(xi, 0)
			vals_state[(int64)(begin + columnValue - 1)] += trueweight;

			begin += colValCnt[i];
			//f(xi, cj)
			position = (int64)(begin + (columnValue - 1) * numOfClasses + trueclass - 1);
			vals_state[position] += trueweight;

			begin += colValCnt[i] * numOfClasses;
		}
	}
	//elog(NOTICE, "aggr_infogain_2: %d, %d", numOfClasses, numOfColValCntVals);

    pgarray = construct_array((Datum *)vals_state,
    		numOfState,FLOAT8OID,
            sizeof(float8),true,'d');
    PG_RETURN_ARRAYTYPE_P(pgarray);
}


/*
 * Compute the split_info used by the gainratio split criterion 
 * (Note: gainratio = infogain/split_info).
 * Please refer http://en.wikipedia.org/wiki/Information_gain_ratio
 * for detailed gainratio definition.
 *
 * Parameters:
 *      vals_state:     This arrays contain all those numbers used for 
 *                      split_info calculation.
 *      from:           It specifies the index from which the calculation 
 *                      should begin.
 *      numOfValues:    It specifies the total number of values used for
 *                      split_info calculation.
 * Return:
 *      The computed value of split_info.
 */
static float8 compute_split_info
    (
        float8 *    vals_state, 
        int32 		totalPos,
        int32       from, 
        int32       numOfValues
    )
{
    float8 sp_info = 0;
    float8 total = vals_state[0];
    float8 numOfNullValues = total - vals_state[totalPos];
    float8 counter = vals_state[totalPos];
    float8 ratio = 0;

    if( is_float_zero(total) )
    {
        elog(ERROR, "total records of train set is: %lf", total);
    }

    for(int32 i = 0; i < numOfValues; ++i)
    {
        float8 fract = vals_state[from + i];
        counter -= fract;
        if(is_float_zero(total-fract))
        {
            //elog(NOTICE, "all elements in one branch");
            return 0;
        }
        else if(!is_float_zero(fract))
        {
            ratio = fract/total;
            sp_info -= (ratio)*log(ratio);
        }
    }

    // process null values
    if(!is_float_zero(numOfNullValues))
    {
		ratio = numOfNullValues / total;
		sp_info -= (ratio)*log(ratio);
    }

    if( ! is_float_zero(counter))
    {
        elog(WARNING,"In compute_split_info. accumulation of part not equal to toal. counter is: %lf",counter);
    }
    return sp_info;
}

/*
 * Get the gainratio based on infogain and split info 
 * (Note: gainratio = infogain/split_info).
 * Please refer http://en.wikipedia.org/wiki/Information_gain_ratio
 * for detailed gainratio definition.
 *
 * Parameters:
 *      gain:           The information gain already calculated.
 *      vals_state:     This arrays contain all those numbers used for 
 *                      split_info calculation.
 *      from:           It specifies the index from which the calculation 
 *                      should begin.
 *      numOfValues:    It specifies the total number of values used for
 *                      split_info calculation.
 * Return:
 *      The computed value of gainratio.
 */
static float8 get_gain_ratio
	(
	    float8      gain,
	    float8*     vals_state,
	    int32 		totalPos,
	    int32       fractFrom,
	    int32       numOfValues
	)
{
	if (is_float_zero(vals_state[totalPos]))
		return 0;

    float8 split_info = compute_split_info(vals_state, totalPos, fractFrom, numOfValues);
   // elog(NOTICE,"split_info: %lf",split_info);
    if( split_info > 0 )
    {
        //elog(NOTICE, "information gain: %lf, gain ratio: %lf", tot, tot/split_info);
    	gain /= split_info;
    }
    else
    {
        //all elements in one branch, no gain at all
    	gain = 0;
    }

    return gain;
}

/*
 * Get the value which measures the weighted impurity of information. 
 * (Impurity can be represented by gini or entropy.)
 *
 * You can find a brief definition for entropy and gini in the link of:
 * http://en.wikipedia.org/wiki/Decision_tree_learning#Formulae.
 *
 * Parameters:
 *      vals_state:     This arrays contain all those numbers used for 
 *                      impurity calculation.
 *      from:           It specifies the index from which the calculation 
 *                      should begin.
 *      size:           It specifies the total number of values used for
 *                      impurity calculation.
 *      fract:          The number of records belonging to the current 
 *                      fraction.
 *      total:          The total number of records.
 *      split_criterion:It defines which split criterion is used to construct
 *                      the decision tree. (1- information gain. 2- gain ratio.
 *                      3- gini impurity). 
 *                      For the first two criteria, they use entropy to measure 
 *                      the impurity.
 *                      For the last one, it use gini to measure impurity.
 * Return:
 *      The computed value of gini or entropy to reflect the information's
 *      impurity.
 */
static float8 get_impurity_value
	(
	    float8*     vals_state,
	    int32       from,
	    int32       size,
	    float8      fract,
	    float8      total,
	    int32       split_criterion
	)
{

    if (3 == split_criterion)
    {
        return gini_weighted_float(vals_state, from, size, fract, total);

    }
    else
    {
    	return entropy_weighted_float(vals_state, from, size, fract, total);

    }

}

/*
 *  For that split with maximum split gain, this function calculate
 *  those values, such as chisquare, maxclass, ebp, etc.
 *
 * Parameters:
 *      vals_state:     This arrays contain all those numbers used for 
 *                      calculation.
 *      numOfClasses:   It specifies the total number of distinct classes.
 *      iFeature:       The index of the feature used to get maximum split
 *                      gain.
 *      maxSplitGain:   The computed maximum split gain.
 *      totalCase:      The total number of records in training set.
 *      result:         The array used to store that function's result.
 *      confLevel:      This parameter is used by the 'Error-Based Pruning'.
 *                      Please refer to the paper for detailed definition.
 *                      The paper's name is 'Error-Based Pruning of Decision  
 *                      Trees Grown on Very Large Data Sets Can Work!'.
 *      splitValue:     If the selected feature is continuous, it specifies
 *                      the split value. Otherwise, it is of no use.
 *      numOfFeatVal:   The number of distinct values for the selected feature.
 *      from:           It specifies the index from which the computation begins.
 *      fract:          The number of records belonging to the current 
 *                      fraction.
 *      featCntIdx:     The index of the 'vals_state' array. The values for that
 *                      feature start from that index.
 *      is_cont_feature:It defines whether the selected feature is continuous.
 * Return:
 *      result[0]:      max split gain.
 *      result[1]:      chisquare.
 *      result[2]:      count(records belonging to maxClass)/count(all records)
 *      result[3]:      the Id of the class containing most records.
 *      result[4]:      total error for error-based pruning.
 *      result[5]:      the index of the selected feature.
 *      result[6]:      The number of distinct values for the selected feature.
 *      result[7]:      If the selected feature is continuous, it specifies
 *                      the split value. Otherwise, it is of no use.
 *      result[8]:      It defines whether the selected feature is continuous.
 */
static void compute_split_gain_max
	(
	    float8 *    vals_state,
	    int32       numOfClasses,
	    int32       iFeature,
	    float8      maxSplitGain,
	    float8      totalCase,
	    float8 *    result,
	    float8      confLevel,
	    float8      splitValue,
	    int32       numOfFeatVal,
	    int32 		clsTotalFrom,
	    int32       from,
	    int32       featCntIdx,
	    bool        is_cont_feature
	)
{
	int32 maxClassIdx = 1;
	int i = 0;
	float8 chiSquare = 0;
	int32 maxBegin = clsTotalFrom - 1;

    for (i = 0; i < numOfFeatVal; ++i)
    {
        chiSquare += chi_square_statistic
        			(
							vals_state,
							clsTotalFrom,
							from + i * numOfClasses,
							numOfClasses,
							vals_state[featCntIdx + i],
							totalCase
        			);
    }


	result[0] = maxSplitGain;
	result[1] = chiSquare;

    for(i = 2; i < numOfClasses + 1; ++i){
        if(vals_state[maxBegin + maxClassIdx] < vals_state[maxBegin + i]){
        	maxClassIdx = i;
        }
    }

    if(totalCase == 0){
        result[2] = 0;
    }else{
        result[2] = vals_state[maxBegin + maxClassIdx] / totalCase;
    }

    result[3] = maxClassIdx;

    if (is_float_zero(1 - confLevel))
    	result[4] = 1;
    else
    	result[4] = get_ebp_total_error(totalCase, vals_state[maxBegin + maxClassIdx], confLevel);

    result[5] = (float8)iFeature;
	result[6] = numOfFeatVal;
	result[7] = splitValue;
	result[8] = is_cont_feature;
	result[9] = vals_state[0];
}

/*
 * This function considers all split plans, find the one with best split
 * gain and returns the results for the best split chosen by us. 
 *
 * Parameters:
 *      vals_state:         This arrays contain all those numbers used for 
 *                          calculation.
 *      numOfClasses:       It specifies the total number of distinct classes.
 *      colValCnt:          The array containing the number of distinct values
 *                          for each feature. 
 *      numOfColValCnt:     It defines how many features that training set has.
 *      contSplitVals:      This is the array containing all the candidate split
 *                          values for various features.
 *      result:             The array used to store that function's result.
 *      confLevel:          This parameter is used by the 'Error-Based Pruning'.
 *                          Please refer to the paper for detailed definition.
 *                          The paper's name is 'Error-Based Pruning of Decision  
 *                          Trees Grown on Very Large Data Sets Can Work!'.
 *      split_criterion:    It defines the split criterion to be used.
 *                          (1- information gain. 2- gain ratio. 3- gini)
 *      has_cont_feature:   It specifies whether there exists one continuous 
 *                          feature.
 * Return:
 *      result[0]:      max split gain.
 *      result[1]:      chisquare.
 *      result[2]:      count(records belonging to maxClass)/count(all records)
 *      result[3]:      the Id of the class containing most records.
 *      result[4]:      total error for error-based pruning.
 *      result[5]:      the index of the selected feature.
 *      result[6]:      The number of distinct values for the selected feature.
 *      result[7]:      If the selected feature is continuous, it specifies
 *                      the split value. Otherwise, it is of no use.
 *      result[8]:      It defines whether the selected feature is continuous.
 */

static float8* compute_split_gain
	(
	    float8 *    vals_state,
	    int32       numOfClasses,
	    int32 *     colValCnt,
	    int32       numOfColValCnt,
	    float8 *    contSplitVals,
	    float8 *    result,
	    float       confLevel,
	    int32       split_criterion,
	    bool        has_cont_feature
	)
{

    int iFeature = 0;
    int i = 1;
    int k = 0;
    int j = 0;

    float8 nullCoeff = 1.0 / vals_state[0];
    float8 total = 0;
    int32 numOfValues = 0;

    result[0] = -1.0;
    int32 totalBegin = 1;
    int32 begin = (numOfClasses + 1) * numOfColValCnt + 1;
    float8 totalentro = 0;

    float8 tot = 0;
    int32 numOfContFeatures = 0;
    bool* isContFeatureArray = 0;
    int64 splitBegin = 0;
    int32 contFeatIdx = 2;

    int32       max_feature = 0;
    float8      max_splitGain = 0;
    float8      max_total = 0;
    float8      max_splitValue = 0;
    int32       max_numOfFeatVal = 0;
    int32 		max_clsTotalFrom = 0;
    int32       max_from = 0;
    int32       max_featCntIdx = 0;
    bool        max_is_cont_feature = false;

    //int z = 0;

    if(has_cont_feature)
    {
		numOfContFeatures = (int32)contSplitVals[0];
		isContFeatureArray = palloc(sizeof(bool)*numOfColValCnt);
		memset(isContFeatureArray, 0, sizeof(bool)*numOfColValCnt);

		splitBegin = (numOfContFeatures << 1) + 1;;

		for(i = 1; i < (numOfContFeatures << 1); i += 2)
		{
			isContFeatureArray[(int32)contSplitVals[i] - 1] = true;
		}
    }


    //elog(NOTICE, "numOfClasses:%d numOfColValCnt:%d confLevel: %f 
    //  split_criterion: %d", numOfClasses, numOfColValCnt, confLevel, split_criterion);
    for (iFeature = 0; iFeature < numOfColValCnt; ++iFeature )
    {

    	if (has_cont_feature && isContFeatureArray[iFeature])
    	{
    		//elog(NOTICE, "feature: %d, idx: %d", iFeature, contFeatIdx);
    		if (numOfContFeatures < (contFeatIdx >> 1))
    		{
    			elog(ERROR, "The number of continuous features is not correct!");
    		}

    		numOfValues = contSplitVals[contFeatIdx];
    		contFeatIdx += 2;
    		// no values in this column, ignore
    		if(numOfValues <= 0)
    			continue;

			total = vals_state[totalBegin];
			totalentro = get_impurity_value(vals_state + totalBegin, 1, 
                    numOfClasses, total, total, split_criterion);

			for (j = 0; j < numOfValues; ++j)
			{
				tot = totalentro;
                float8 counter = total;
				for (k = 0; k < 2; ++k)
				{
                    counter -= vals_state[(int64)(begin + k)];
					tot -= get_impurity_value
							(
									vals_state,
									(int)(begin + 2  + k * numOfClasses),
									numOfClasses,
									vals_state[(int64)(begin + k)],
									total,
									split_criterion
							);
				}

				tot = tot * total * nullCoeff;
				//elog(NOTICE, "compute_infogain(cont): %f, %d, %d, %f, %d, %f, %f", nullCoeff, iFeature + 1, numOfValues, tot, numOfClasses, total, contSplitVals[splitBegin]);

                if( !is_float_zero(counter) )
                {
                    elog(WARNING, "in compute_split_gain, counter:%lf, total:%lf,"
                            "isContFeatureArray[iFeature]:%d,feature_id:%d",
                            counter, total,isContFeatureArray[iFeature],
                            iFeature);
                }
				/*
				elog(NOTICE, 'tot: %f, fract: %f, total: %f', tot, 
                    vals_state[(int64)(begin + (j << 1) + k)], total);
				for(z = 0; z < numOfClasses; ++z)
				{
					elog(NOTICE, 'count: %f', vals_state[(int)(begin + (numOfValues << 1)  + z)]);
				}
				*/
				if (2 == split_criterion)
				{
					tot = get_gain_ratio
							(
								tot,
								vals_state,
								totalBegin,
								begin,
								2
							);
				}

	            if (is_float_less(result[0],tot))
				{
				    //just book marking. 
                    //Only need compute once for chisq 
                    //and some others
					result[0] = tot;
				    max_feature = iFeature + 1;
				    max_splitGain = tot;
				    max_total = total;
				    max_splitValue = contSplitVals[splitBegin];
				    max_numOfFeatVal = 2;
				    max_clsTotalFrom = totalBegin + 1;
				    max_from = begin + 2;
				    max_featCntIdx = begin;
				    max_is_cont_feature = true;
				}

				begin += 2 +  (numOfClasses << 1);
				++splitBegin;
			}
			totalBegin += numOfClasses + 1;

    	}
    	else
    	{
    		numOfValues = colValCnt[iFeature];
			total = vals_state[totalBegin];
			totalentro = get_impurity_value(vals_state + totalBegin, 
                    1, numOfClasses, total, total, split_criterion);
			tot = totalentro;

            float8 counter = total;
			for(i = 0; i < numOfValues; ++i)
			{
                counter -= vals_state[(int64)(begin + i)];
				tot -= get_impurity_value
						(
						vals_state,
						(int)(begin + numOfValues + i * numOfClasses),
						numOfClasses,
						vals_state[(int64)(begin + i)],
						total,
						split_criterion
						);
			}

			tot = tot * total * nullCoeff;
	        //elog(NOTICE, "compute_infogain:  %f, %d, %d, %f, %d, %f", nullCoeff, iFeature + 1, numOfValues, tot, numOfClasses, total);

            if( !is_float_zero(counter) )
            {
                    elog(WARNING, "in compute_split_gain, counter:%lf, total:%lf,"
                            "isContFeatureArray[iFeature]:%d,feature_id:%d",
                            counter, total,isContFeatureArray[iFeature],
                            iFeature);
            }
			if (2 == split_criterion)
			{
				tot = get_gain_ratio
						(
							tot,
							vals_state,
							totalBegin,
							begin,
							numOfValues
						);
			}

	        //elog(NOTICE, "compute_infogain_2: %f, %d, %f, %d, %f", 
            //  totalentro, numOfValues, tot, numOfClasses, total);
	        if (is_float_less(result[0],tot))

	        {
	            //just book marking. 
                //Only need compute once for chisq 
                //and some others
	        	result[0] = tot;
	            max_feature = iFeature + 1;
	            max_splitGain = tot;
	            max_total = total;
	            max_splitValue = 0;
	            max_numOfFeatVal = numOfValues;
	            max_clsTotalFrom = totalBegin + 1;
	            max_from = begin + numOfValues;
	            max_featCntIdx = begin;
	            max_is_cont_feature = false;
	        }

	        begin = begin + numOfValues + numOfValues * numOfClasses;
			totalBegin += numOfClasses + 1;
    	}
    }

    compute_split_gain_max
    	(
			vals_state,
			numOfClasses,
			max_feature,
			max_splitGain,
			max_total,
			result,
			confLevel,
			max_splitValue,
			max_numOfFeatVal,
			max_clsTotalFrom,
			max_from,
			max_featCntIdx,
			max_is_cont_feature
    	);
    return result;
}

/*
 *  This function is the entry point for computing the split gain.
 *
 * Parameters:
 *      0 vals_state:         The array used to store the accumulated information.
 *      1 colValCntArray:     The array containing the number of distinct values
 *                            for each feature.
 *      2 contSplitValArray:  The array containing all the candidate split values
 *                            for each continuous feature.
 *      3 numOfClasses:       The total number of distinct classes.   
 *      4 confLevel:          This parameter is used by the 'Error-Based Pruning'.
 *                            Please refer to the paper for detailed definition.
 *                            The paper's name is 'Error-Based Pruning of Decision  
 *                            Trees Grown on Very Large Data Sets Can Work!'.
 *      5 split_criterion:    It defines the split criterion to be used.
 *                            (1- information gain. 2- gain ratio. 3- gini)
 *      6 has_cont_feature:   It specifies whether there exists one continuous 
 *                            feature.
 * Return:
 *      The array containing all the information for the calculation of 
 *      split gain. 
 *      result[0]:      max split gain.
 *      result[1]:      chisquare.
 *      result[2]:      count(records belonging to maxClass)/count(all records)
 *      result[3]:      the Id of the class containing most records.
 *      result[4]:      total error for error-based pruning.
 *      result[5]:      the index of the selected feature.
 *      result[6]:      The number of distinct values for the selected feature.
 *      result[7]:      If the selected feature is continuous, it specifies
 *                      the split value. Otherwise, it is of no use.
 *      result[8]:      It defines whether the selected feature is continuous.
 */
Datum compute_infogain(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(compute_infogain);
Datum compute_infogain(PG_FUNCTION_ARGS) {
    ArrayType *state  = PG_GETARG_ARRAYTYPE_P(0);
    float8 *vals_state=(float8 *)ARR_DATA_PTR(state);

    ArrayType *colValCntArray  = PG_GETARG_ARRAYTYPE_P(1);
    int dim = ARR_NDIM(colValCntArray);
    int* pdims = ARR_DIMS(colValCntArray);
    int numOfColValCnt = ArrayGetNItems(dim, pdims);
    int32 *colValCnt = (int32 *)ARR_DATA_PTR(colValCntArray);

    bool has_cont_feature = PG_GETARG_BOOL(6);

    ArrayType *contSplitValArray = 0;
    int numOfContSplitVals = 0;
    float8 *contSplitVals = 0;

    if (has_cont_feature)
    {
    	contSplitValArray = PG_GETARG_ARRAYTYPE_P(2);
		dim = ARR_NDIM(contSplitValArray);
		pdims = ARR_DIMS(contSplitValArray);
		numOfContSplitVals = ArrayGetNItems(dim, pdims);
		contSplitVals = (float8 *)ARR_DATA_PTR(contSplitValArray);
    }
    int32 numOfClasses = PG_GETARG_INT32(3);

    float confLevel = PG_GETARG_FLOAT8(4) * 0.01;

    int32 split_criterion = PG_GETARG_INT32(5);

    int32 nResult = 10;

    float8 *result = palloc(sizeof(float8) * nResult);
    memset(result, 0, sizeof(float8) * nResult);

    ArrayType *pgarray;

    compute_split_gain
    	(
			vals_state,
			numOfClasses,
			colValCnt,
			numOfColValCnt,
			contSplitVals,
			result,
			confLevel,
			split_criterion,
			has_cont_feature
    	);


    pgarray = construct_array((Datum *)result,
    		nResult,FLOAT8OID,
            sizeof(float8),true,'d');
    PG_RETURN_ARRAYTYPE_P(pgarray);
}

/*
 * This function allocate float8 array.
 * Parameters:
 *       0 size:    It defines how many float8 values the
 *                  array should contain.
 *       1 value:   It defines the initial value. All the
 *                  elements of that array are initialized
 *                  with that value.
 * Return:
 *      The array requested by user.
 */
Datum malloc_and_set(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(malloc_and_set);
Datum malloc_and_set(PG_FUNCTION_ARGS) {
    int size = PG_GETARG_INT32(0);
    int value = PG_GETARG_INT32(1);
    float8 *result = (float8*)palloc(sizeof(float8)*size); 
    ArrayType *pgarray;

    memset(result, value, sizeof(float8)*size);

    pgarray = construct_array((Datum *)result,
            size, FLOAT8OID,
            sizeof(float8),true,'d');
    PG_RETURN_ARRAYTYPE_P(pgarray);
}

/*
 * This function allocate int64 array.
 * Parameters:
 *       0 size:    It defines how many int64 values the
 *                  array should contain.
 *       1 value:   It defines the initial value. All the
 *                  elements of that array are initialized
 *                  with that value.
 * Return:
 *      The array requested by user.
 */
Datum malloc_and_set_int64(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(malloc_and_set_int64);
Datum malloc_and_set_int64(PG_FUNCTION_ARGS) {
    int size = PG_GETARG_INT32(0);
    int value = PG_GETARG_INT32(1);
    int64 *result = (int64*)palloc(sizeof(int64)*size);
    ArrayType *pgarray;

    memset(result, value, sizeof(int64)*size);
    //
    pgarray = construct_array((Datum *)result,
            size, INT8OID,
            sizeof(int64),true,'d');
    PG_RETURN_ARRAYTYPE_P(pgarray);
}

/*
 * Randomly select number of 'value1' distinct values
 * Those distinct values should between 1 and 'value2'.
 *
 * Parameters:
 *       0 value1:    It defines how many distinct values 
 *                    we should generate.
 *       1 value2:    It defines the upper limit for those
 *                    requested values. It should be greater 
 *                    than 1 since the lower limit is fixed
 *                    to 1 by default. 
 * Return:
 *      The array containing all those values 
 *      requested by user.
 */
Datum weighted_no_replacement(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(weighted_no_replacement);
Datum weighted_no_replacement(PG_FUNCTION_ARGS) {
    int value1 = PG_GETARG_INT32(0);
    int value2 = PG_GETARG_INT32(1);
    int64 *result = (int64*)palloc(sizeof(int64)*value1);
    int32 i = 0;
    int32 k = 0;
    ArrayType *pgarray;
    float to_select = value1;

    for(;i < value2; i++){


        if(rand()%100 < (to_select/(value2 - i))*100){			
            result[k] = i+1;
            k++;
        }

        if(to_select == 0)
            break; 

    }

    pgarray = construct_array((Datum *)result,
            value1, INT8OID,
            sizeof(int64),true,'d');
    PG_RETURN_ARRAYTYPE_P(pgarray);
}

/*
 *  This function returns the min value of two float.
 * Parameters:
 *       0 x:    a float value
 *       1 y:    a float value
 *                  
 * Return:
 *       The value which is smaller than the other.
 */
Datum min(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(min);
Datum min(PG_FUNCTION_ARGS) {
    float8 x = PG_GETARG_FLOAT8(0);
    float8 y = PG_GETARG_FLOAT8(1);

    float8 ret = x;

    if( y < x )
        ret = y;

    PG_RETURN_FLOAT8((float8)ret);
}

/*
 * This function judges whether a float value is less than the other
 *
 * Parameters:
 *       0 x:    a float value
 *       1 y:    a float value
 *                  
 * Return:
 *       0- If x<y; 1- If x>=y.
 */
Datum is_less(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(is_less);
Datum is_less(PG_FUNCTION_ARGS) {
    float8 x = PG_GETARG_FLOAT8(0);
    float8 y = PG_GETARG_FLOAT8(1);

    int32 ret = 0;
    if( is_float_less(x, y) )
        ret = 1;

    PG_RETURN_INT32(ret);
}

/*
 * The function used for gamma calculation. We should leverage other
 * implementation in the boost lib. 
 * We will delete this function after the integration.
 */ 
float8 gammaln(float8 x) {
    float8 xnum = 0;
    float8 xden = 1;
    float8 y = x;
    float8 xm1 = 0;
    float8 minimum = pow(1, -10);
    if( x <= minimum )
    {
        y = -1.0 * log(x);
    }
    else if( x <= 0.5 )
    {
        xnum = xnum * y + 4.945235359296727046734888e0;
        xnum = xnum * y + 2.018112620856775083915565e2;
        xnum = xnum * y + 2.290838373831346393026739e3;
        xnum = xnum * y + 1.131967205903380828685045e4;
        xnum = xnum * y + 2.855724635671635335736389e4;
        xnum = xnum * y + 3.848496228443793359990269e4;
        xnum = xnum * y + 2.637748787624195437963534e4;
        xnum = xnum * y + 7.225813979700288197698961e3;
        xden = xden * y + 6.748212550303777196073036e1;
        xden = xden * y + 1.113332393857199323513008e3;
        xden = xden * y + 7.738757056935398733233834e3;
        xden = xden * y + 2.763987074403340708898585e4;
        xden = xden * y + 5.499310206226157329794414e4;
        xden = xden * y + 6.161122180066002127833352e4;
        xden = xden * y + 3.635127591501940507276287e4;
        xden = xden * y + 8.785536302431013170870835e3;
        y    = -log(y) + (y * (-5.772156649015328605195174e-1 + y * (xnum/xden)));        
    }
    else if (x <=0.6796875) 
    {
        xm1  = (x-0.5) - 0.5;
        xnum = xnum * xm1 + 4.974607845568932035012064e0;
        xnum = xnum * xm1 + 5.424138599891070494101986e2;
        xnum = xnum * xm1 + 1.550693864978364947665077e4;
        xnum = xnum * xm1 + 1.847932904445632425417223e5;
        xnum = xnum * xm1 + 1.088204769468828767498470e6;
        xnum = xnum * xm1 + 3.338152967987029735917223e6;
        xnum = xnum * xm1 + 5.106661678927352456275255e6;
        xnum = xnum * xm1 + 3.074109054850539556250927e6;
        xden = xden * xm1 + 1.830328399370592604055942e2;
        xden = xden * xm1 + 7.765049321445005871323047e3;
        xden = xden * xm1 + 1.331903827966074194402448e5;
        xden = xden * xm1 + 1.136705821321969608938755e6;
        xden = xden * xm1 + 5.267964117437946917577538e6;
        xden = xden * xm1 + 1.346701454311101692290052e7;
        xden = xden * xm1 + 1.782736530353274213975932e7;
        xden = xden * xm1 + 9.533095591844353613395747e6;
        y    = -log(y) + (xm1 * (4.227843350984671393993777e-1 + xm1 * (xnum/xden)));
    }
    else if( x <= 1.5 )
    {
        xm1  = (x-0.5) - 0.5;
        xnum = xnum * xm1 + 4.945235359296727046734888e0;
        xnum = xnum * xm1 + 2.018112620856775083915565e2;
        xnum = xnum * xm1 + 2.290838373831346393026739e3;
        xnum = xnum * xm1 + 1.131967205903380828685045e4;
        xnum = xnum * xm1 + 2.855724635671635335736389e4;
        xnum = xnum * xm1 + 3.848496228443793359990269e4;
        xnum = xnum * xm1 + 2.637748787624195437963534e4;
        xnum = xnum * xm1 + 7.225813979700288197698961e3;
        xden = xden * xm1 + 6.748212550303777196073036e1;
        xden = xden * xm1 + 1.113332393857199323513008e3;
        xden = xden * xm1 + 7.738757056935398733233834e3;
        xden = xden * xm1 + 2.763987074403340708898585e4;
        xden = xden * xm1 + 5.499310206226157329794414e4;
        xden = xden * xm1 + 6.161122180066002127833352e4;
        xden = xden * xm1 + 3.635127591501940507276287e4;
        xden = xden * xm1 + 8.785536302431013170870835e3;
        y    = xm1 * (-5.772156649015328605195174e-1 + xm1 * (xnum/xden));
    }
    else if( x <= 4.0 )
    {
        xm1  = x - 2.0;
        xnum = xnum * xm1 + 4.974607845568932035012064e0;
        xnum = xnum * xm1 + 5.424138599891070494101986e2;
        xnum = xnum * xm1 + 1.550693864978364947665077e4;
        xnum = xnum * xm1 + 1.847932904445632425417223e5;
        xnum = xnum * xm1 + 1.088204769468828767498470e6;
        xnum = xnum * xm1 + 3.338152967987029735917223e6;
        xnum = xnum * xm1 + 5.106661678927352456275255e6;
        xnum = xnum * xm1 + 3.074109054850539556250927e6;
        xden = xden * xm1 + 1.830328399370592604055942e2;
        xden = xden * xm1 + 7.765049321445005871323047e3;
        xden = xden * xm1 + 1.331903827966074194402448e5;
        xden = xden * xm1 + 1.136705821321969608938755e6;
        xden = xden * xm1 + 5.267964117437946917577538e6;
        xden = xden * xm1 + 1.346701454311101692290052e7;
        xden = xden * xm1 + 1.782736530353274213975932e7;
        xden = xden * xm1 + 9.533095591844353613395747e6;
        y    = xm1 * (4.227843350984671393993777e-1 + xm1 * (xnum/xden));
    }
    else if( x <= 12.0 )
    {
        xm1  = x - 4.0;
        xden = -1.0;
        xnum = xnum * xm1 + 1.474502166059939948905062e4;
        xnum = xnum * xm1 + 2.426813369486704502836312e6;
        xnum = xnum * xm1 + 1.214755574045093227939592e8;
        xnum = xnum * xm1 + 2.663432449630976949898078e9;
        xnum = xnum * xm1 + 2.940378956634553899906876e10;
        xnum = xnum * xm1 + 1.702665737765398868392998e11;
        xnum = xnum * xm1 + 4.926125793377430887588120e11;
        xnum = xnum * xm1 + 5.606251856223951465078242e11;
        xden = xden * xm1 + 2.690530175870899333379843e3;
        xden = xden * xm1 + 6.393885654300092398984238e5;
        xden = xden * xm1 + 4.135599930241388052042842e7;
        xden = xden * xm1 + 1.120872109616147941376570e9;
        xden = xden * xm1 + 1.488613728678813811542398e10;
        xden = xden * xm1 + 1.016803586272438228077304e11;
        xden = xden * xm1 + 3.417476345507377132798597e11;
        xden = xden * xm1 + 4.463158187419713286462081e11;
        y    = 1.791759469228055000094023e0 + xm1 * (xnum/xden);
    }
    else
    {
        float8 r   = 5.7083835261e-03;
        xm1 = log(y);
        float8 xx  = x*x;
        r  = r / xx - 1.910444077728e-03;
        r  = r / xx + 8.4171387781295e-04;
        r  = r / xx - 5.952379913043012e-04;
        r  = r / xx + 7.93650793500350248e-04;
        r  = r / xx - 2.777777777777681622553e-03;
        r  = r / xx + 8.333333333333333331554247e-02;
        r  = r / y;
        y  = r + 0.9189385332046727417803297 - 0.5 * xm1 + y * (xm1-1);
    }
    return y;
}


/*
 * The function used for gamma calculation. We should leverage other
 * implementation in the boost lib. 
 * We will delete this function after the integration.
 */ 
Datum gampdf(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(gampdf);
Datum gampdf(PG_FUNCTION_ARGS) {
    float8 X = PG_GETARG_FLOAT8(0);
    float8 A = PG_GETARG_FLOAT8(1);
    float8 B = PG_GETARG_FLOAT8(2);
    float8 result = 0;
    float8 calc = (A - 1) * log(X) - (X/B) - gammaln(A) - A * log(B);
    if (calc < -690) 
        result = 0.0;
    else
        result = exp(calc);
    PG_RETURN_FLOAT8((float8)result);
}

