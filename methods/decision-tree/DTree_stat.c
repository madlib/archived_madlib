#include "postgres.h"
#include "fmgr.h"
#include "utils/array.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

Datum hash_array( PG_FUNCTION_ARGS);

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
    
    for (int i=0;i<numstate;i++)
	{
		c = vals_state[i];
		hash = c + (hash << 7) + (hash << 16) - hash;
	}

	PG_RETURN_INT32(hash);
}

static float entropyWeighted(int* values, int size, float fract, float total){
	int i = 0;
	float mult = fract/total;
	float result = 0;
	
	for(; i < size; ++i){
		if(values[i] > 0){
			result -= (values[i]/fract)*log(values[i]/fract);
		}
	}
	return result*mult;
}

static float8 entropyWeightedFloat(float8* values, int from, int size, float8 fract, float8 total){
	int i = from;
	float8 mult = fract/total;
	float8 result = 0;
	
	for(; i < (size+from); ++i){
		if(values[i] > 0){
			result -= (values[i]/fract)*log(values[i]/fract);
		}
	}
	return result*mult;
}

Datum aggr_InfoGain(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(aggr_InfoGain);
Datum aggr_InfoGain(PG_FUNCTION_ARGS) {
	ArrayType *state  = PG_GETARG_ARRAYTYPE_P(0);
	int dimstate = ARR_NDIM(state);
    int *dimsstate = ARR_DIMS(state);
	int numstate = ArrayGetNItems(dimstate,dimsstate);
    float8 *vals_state=(float8 *)ARR_DATA_PTR(state);
    
	float8 truevalue = PG_GETARG_FLOAT8(1);
	float8 trueweight = PG_GETARG_FLOAT8(2);
	int32 posclasses = PG_GETARG_INT32(3);
	int32 trueclass = PG_GETARG_INT32(5);

	vals_state[0] += trueweight;
	vals_state[trueclass] += trueweight;
	
	vals_state[(int)(truevalue*(posclasses+1))] += trueweight;
	vals_state[(int)(truevalue*(posclasses+1) + trueclass)] += trueweight;
  
   	ArrayType *pgarray;
    pgarray = construct_array((Datum *)vals_state,
		numstate,FLOAT8OID,
		sizeof(float8),true,'d');
    PG_RETURN_ARRAYTYPE_P(pgarray);
}

Datum compute_InfoGain(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(compute_InfoGain);
Datum compute_InfoGain(PG_FUNCTION_ARGS) {
	ArrayType *state  = PG_GETARG_ARRAYTYPE_P(0);
    float8 *vals_state=(float8 *)ARR_DATA_PTR(state);
    
	int32 posclasses = PG_GETARG_INT32(1);
	int32 posvalues = PG_GETARG_INT32(2);

	float8 *result = malloc(sizeof(float8)*4);
	
	int i = 1;
	int max = 1;
	float8 tot = entropyWeightedFloat(vals_state, 1, posclasses, vals_state[0], vals_state[0]);
	for(; i < (posvalues+1); ++i){
		tot -= entropyWeightedFloat(vals_state, (i*(posclasses+1)+1), posclasses, vals_state[i*(posclasses+1)], vals_state[0]);
	}
	result[0] = tot;
	
	i = 1;
	tot = 0;
	for(; i < (posvalues+1); ++i){
		tot += ChiSquareStatistic(vals_state, (i*(posclasses+1)+1), posclasses, vals_state[i*(posclasses+1)], vals_state[0]);
	}
	result[1] = tot;
	
	i = 1;
	for(; i < (posclasses+1); ++i){
		if(vals_state[max] < vals_state[i]){
			max = i;
		}
	}
	result[2] = vals_state[max]/vals_state[0];
	result[3] = max;
	  
	ArrayType *pgarray;  
    pgarray = construct_array((Datum *)result,
		4,FLOAT8OID,
		sizeof(float8),true,'d');
    PG_RETURN_ARRAYTYPE_P(pgarray);
}

Datum mallocset(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(mallocset);
Datum mallocset(PG_FUNCTION_ARGS) {
	int size = PG_GETARG_INT32(0);
	int value = PG_GETARG_INT32(1);
	float8 *result = (float8*)malloc(sizeof(float8)*size); 
	memset(result, value, sizeof(float8)*size);
	
    ArrayType *pgarray;
	pgarray = construct_array((Datum *)result,
		size, FLOAT8OID,
		sizeof(float8),true,'d');
    PG_RETURN_ARRAYTYPE_P(pgarray);
}

Datum WeightedNoReplacement(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(WeightedNoReplacement);
Datum WeightedNoReplacement(PG_FUNCTION_ARGS) {
	int value1 = PG_GETARG_INT32(0);
	int value2 = PG_GETARG_INT32(1);
	int64 *result = (int64*)malloc(sizeof(int64)*value1);
	int32 i = 0;
	
   	for(;i < value1; ++i){
   		result[i] = rand()%value2;
   	}
   
    ArrayType *pgarray;
	pgarray = construct_array((Datum *)result,
		value1, INT8OID,
		sizeof(int64),true,'d');
    PG_RETURN_ARRAYTYPE_P(pgarray);
}

Datum findentropy(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(findentropy);
Datum findentropy(PG_FUNCTION_ARGS) {
	ArrayType *values  = PG_GETARG_ARRAYTYPE_P(0);
	ArrayType *classes = PG_GETARG_ARRAYTYPE_P(1);
	int posvalues = PG_GETARG_INT32(2);
	int posclasses = PG_GETARG_INT32(3);

	int dimvalues = ARR_NDIM(values);
    int *dimsvalues = ARR_DIMS(values);
	int numvalues = ArrayGetNItems(dimvalues,dimsvalues);
	
    int32 *vals_values=(int32 *)ARR_DATA_PTR(values);
    int32 *vals_classes=(int32 *)ARR_DATA_PTR(classes);
  
  	int *pre_entropy = (int*)malloc(sizeof(int)*posclasses);
  	int i;
  	int j;
  	int sum = 0;
  	float8 result = 0;
  	
	for (i=0; i<posvalues; ++i){
       memset(pre_entropy, 0, sizeof(int)*posclasses);
       for(j=0, sum=0; j<numvalues; ++j){
       		if(vals_values[j] == (i+1)){
       			pre_entropy[vals_classes[j]-1]++;
       			sum++;
       		}
       }
       result += entropyWeighted(pre_entropy, posclasses, (float)sum, (float)numvalues);
	}
	free(pre_entropy);
    PG_RETURN_FLOAT8((float8)result);
}

Datum array_add(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(array_add);
Datum array_add(PG_FUNCTION_ARGS) {
	ArrayType *arr1  = PG_GETARG_ARRAYTYPE_P(0);
	ArrayType *arr2 = PG_GETARG_ARRAYTYPE_P(1);

	int dimvalues = ARR_NDIM(arr1);
    int *dimsvalues = ARR_DIMS(arr1);
	int numvalues1 = ArrayGetNItems(dimvalues,dimsvalues);
	int i = 0;
	
    float8 *vals_1=(float8 *)ARR_DATA_PTR(arr1);
    float8 *vals_2=(float8 *)ARR_DATA_PTR(arr2);
  
  	float8 *result = (float8*)malloc(sizeof(float8)*numvalues1);
  	  	
	for (; i<numvalues1; ++i){
      	result[i] = vals_1[i] + vals_2[i];
	}
	ArrayType *pgarray;
	pgarray = construct_array((Datum *)result,
		numvalues1, FLOAT8OID,
		sizeof(float8),true,'d');
    PG_RETURN_ARRAYTYPE_P(pgarray);
}
