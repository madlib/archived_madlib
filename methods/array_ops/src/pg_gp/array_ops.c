#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"
#include "libpq/pqformat.h"
#include "parser/parse_coerce.h"
#include "catalog/pg_type.h"
#include "utils/array.h"
#include "utils/numeric.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"
#include "utils/typcache.h"
#include "access/hash.h"

#ifndef NO_PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

/*
This module provides c implementations for several postgres array operations.

There is a 3 tier structure to each function calls. This is done to take advantage of some common type
checking. All the functions are classified into 4 types based on the input/output types:

function(array,array)->array (calls: General_2Array_to_Array)
function(array,scalar)->array (calls: General_Array_to_Array)
function(array,array)->scalar (calls: General_2Array_to_Element)
function(array,scalar)->scalar (calls: General_Array_to_Element)

Assuming that this input is flexible enough for some new function, implementer needs to provide 2 functions. First is the top level function
that is being exposed to SQL and takes the necessary parameters. This function makes a call to one of the 4 intermediate functions, passing pointer to the low level function (or functions) as the argument. In case of the single function, this low level function, it will specify the operations to be executed against each cell in the array. If two functions are to be passed the second is the finalization function that takes the result of the execution on each cell and produces final result. In case not final functions is necessary a generic 'noop_finalize' can be used - which does nothing to the intermediate result.
*/

Datum General_2Array_to_Array(ArrayType *v1, ArrayType *v2, char*(*element_function)(Datum,Datum,Oid,char*));
Datum General_Array_to_Array(ArrayType *v1, Datum value, char*(*element_function)(Datum,Datum,Oid,char*));
Datum General_2Array_to_Element(ArrayType *v1, ArrayType *v2, Datum(*element_function)(Datum,Datum,Oid,Datum), Datum(*finalize_function)(Datum,int,Oid), int flag);
Datum General_Array_to_Element(ArrayType *v, Datum exta_val, Datum(*element_function)(Datum,Datum*,Oid,Datum), Datum(*finalize_function)(Datum,int,Oid), int flag);
Datum noop_finalize(Datum elt,int size,Oid element_type);
Datum average_finalize(Datum elt,int size,Oid element_type);
Datum average_root_finalize(Datum elt,int size,Oid element_type);

char* element_add(Datum elt1, Datum elt2, Oid element_type, char* result);
char* element_sub(Datum elt1, Datum elt2, Oid element_type, char* result);
char* element_mult(Datum elt1, Datum elt2, Oid element_type, char* result);
char* element_div(Datum elt1, Datum elt2, Oid element_type, char* result);
char* element_set(Datum elt1, Datum elt2, Oid element_type, char* result);
char* element_sqrt(Datum elt1, Datum elt2, Oid element_type, char* result);
Datum element_dot(Datum elt1, Datum elt2, Oid element_type, Datum result);
Datum element_contains(Datum elt1, Datum elt2, Oid element_type, Datum result);
Datum element_max(Datum elt1, Datum *flag, Oid element_type, Datum result);
Datum element_min(Datum elt1, Datum *flag, Oid element_type, Datum result);
Datum element_sum(Datum elt1, Datum *flag, Oid element_type, Datum result);
Datum element_diff(Datum elt1, Datum *flag, Oid element_type, Datum result);

Datum array_add(PG_FUNCTION_ARGS);
Datum array_sub(PG_FUNCTION_ARGS);
Datum array_mult(PG_FUNCTION_ARGS);
Datum array_div(PG_FUNCTION_ARGS);
Datum array_dot(PG_FUNCTION_ARGS);
Datum array_contains(PG_FUNCTION_ARGS);
Datum array_max(PG_FUNCTION_ARGS);
Datum array_min(PG_FUNCTION_ARGS);
Datum array_sum(PG_FUNCTION_ARGS);
Datum array_sum_big(PG_FUNCTION_ARGS);
Datum array_mean(PG_FUNCTION_ARGS);
Datum array_stddev(PG_FUNCTION_ARGS);
Datum array_of_float(PG_FUNCTION_ARGS);
Datum array_of_bigint(PG_FUNCTION_ARGS);
Datum array_fill(PG_FUNCTION_ARGS);
Datum array_scalar_mult(PG_FUNCTION_ARGS);
Datum array_sqrt(PG_FUNCTION_ARGS);
Datum array_normalize(PG_FUNCTION_ARGS);


PG_FUNCTION_INFO_V1(array_of_float);
Datum array_of_float(PG_FUNCTION_ARGS){
	ArrayType *pgarray;
	unsigned int size = PG_GETARG_INT32(0);
	float8* array = (float8*) palloc (sizeof(float8)*size);
	memset(array, 0, sizeof(float8)*size);
	pgarray = construct_array((Datum *)array,size,FLOAT8OID,sizeof(float8),true,'d');
    PG_RETURN_ARRAYTYPE_P(pgarray);
}

PG_FUNCTION_INFO_V1(array_of_bigint);
Datum array_of_bigint(PG_FUNCTION_ARGS){
	ArrayType *pgarray;
	unsigned int size = PG_GETARG_INT32(0);
	int64* array = (int64*) palloc (sizeof(int64)*size);
	memset(array, 0, sizeof(int64)*size);
	pgarray = construct_array((Datum *)array,size,INT8OID,sizeof(int64),true,'d');
    PG_RETURN_ARRAYTYPE_P(pgarray);
}

Datum noop_finalize(Datum elt,int size,Oid element_type){
	(void) size; /* avoid warning about unused parameter */
	(void) element_type; /* avoid warning about unused parameter */
	return elt;
}

Datum average_finalize(Datum elt,int size,Oid element_type){
	(void) element_type; /* avoid warning about unused parameter */
	return Float8GetDatum(DatumGetFloat8(elt)/(float8)size);
}

Datum average_root_finalize(Datum elt,int size,Oid element_type){
	(void) element_type; /* avoid warning about unused parameter */
	return Float8GetDatum(sqrt(DatumGetFloat8(elt)/(float8)size));
}

Datum element_diff(Datum elt1, Datum *elt2, Oid element_type, Datum result){
	switch(element_type){
		case INT2OID:
			return Int16GetDatum(DatumGetInt16(result)+(DatumGetInt16(elt1)-DatumGetInt16(*elt2))*(DatumGetInt16(elt1)-DatumGetInt16(*elt2)));break;
		case INT4OID:
			return Int32GetDatum(DatumGetInt32(result)+(DatumGetInt32(elt1)-DatumGetInt32(*elt2))*(DatumGetInt32(elt1)-DatumGetInt32(*elt2)));break;
		case INT8OID:
			return Int64GetDatum(DatumGetInt64(result)+(DatumGetInt64(elt1)-DatumGetInt64(*elt2))*(DatumGetInt64(elt1)-DatumGetInt64(*elt2)));break;
		case FLOAT4OID:
			return Float4GetDatum(DatumGetFloat4(result)+(DatumGetFloat4(elt1)-DatumGetFloat4(*elt2))*(DatumGetFloat4(elt1)-DatumGetFloat4(*elt2)));break;
		case FLOAT8OID:
			return Float8GetDatum(DatumGetFloat8(result)+(DatumGetFloat8(elt1)-DatumGetFloat8(*elt2))*(DatumGetFloat8(elt1)-DatumGetFloat8(*elt2)));break;
		default:
			ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
							errmsg("type is not supported"),
							errdetail("Arrays with element type %d are not supported.", (int)element_type)));
			break;
	}
	return result;
}

Datum element_sum(Datum elt1, Datum *flag, Oid element_type, Datum result){
	(void) flag; /* avoid warning about unused parameter */
	switch(element_type){
		case INT2OID:
			return Int16GetDatum(DatumGetInt16(elt1) + DatumGetInt16(result));break;
		case INT4OID:
			return Int32GetDatum(DatumGetInt32(elt1) + DatumGetInt32(result));break;
		case INT8OID:
			return Int64GetDatum(DatumGetInt64(elt1) + DatumGetInt64(result));break;
		case FLOAT4OID:
			return Float4GetDatum(DatumGetFloat4(elt1) + DatumGetFloat4(result));break;
		case FLOAT8OID:
			return Float8GetDatum(DatumGetFloat8(elt1) + DatumGetFloat8(result));break;
		default:
			ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
							errmsg("type is not supported"),
							errdetail("Arrays with element type %d are not supported.", (int)element_type)));
			break;
	}
	return result;
}

Datum element_min(Datum elt1, Datum *flag, Oid element_type, Datum result){
	if(DatumGetInt16(*flag) == 0){
		*flag = DatumGetInt16(1);
		return elt1;
	}
	switch(element_type){
		case INT2OID:
			if(DatumGetInt16(elt1) < DatumGetInt16(result)){
				return elt1;
			}break;
		case INT4OID:
			if(DatumGetInt32(elt1) <  DatumGetInt32(result)){
				return elt1;
			}break;
		case INT8OID:
			if(DatumGetInt64(elt1) <  DatumGetInt64(result)){
				return elt1;
			}break;
		case FLOAT4OID:
			if(DatumGetFloat4(elt1) < DatumGetFloat4(result)){
				return elt1;
			}break;
		case FLOAT8OID:
			if(DatumGetFloat8(elt1) < DatumGetFloat8(result)){
				return elt1;
			}break;
		default:
			ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
							errmsg("type is not supported"),
							errdetail("Arrays with element type %d are not supported.", (int)element_type)));
			break;
	}
	return result;
}

Datum element_max(Datum elt1, Datum *flag, Oid element_type, Datum result){
	if(DatumGetInt16(*flag) == 0){
		*flag = DatumGetInt16(1);
		return elt1;
	}
	switch(element_type){
		case INT2OID:
			if(DatumGetInt16(elt1) > DatumGetInt16(result)){
				return elt1;
			}break;
		case INT4OID:
			if(DatumGetInt32(elt1) >  DatumGetInt32(result)){
				return elt1;
			}break;
		case INT8OID:
			if(DatumGetInt64(elt1) >  DatumGetInt64(result)){
				return elt1;
			}break;
		case FLOAT4OID:
			if(DatumGetFloat4(elt1) > DatumGetFloat4(result)){
				return elt1;
			}break;
		case FLOAT8OID:
			if(DatumGetFloat8(elt1) > DatumGetFloat8(result)){
				return elt1;
			}break;
		default:
			ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
							errmsg("type is not supported"),
							errdetail("Arrays with element type %d are not supported.", (int)element_type)));
			break;
	}
	return result;
}

Datum element_dot(Datum elt1, Datum elt2, Oid element_type, Datum result){
	float8 temp = 0;
	switch(element_type){
		case INT2OID:
			temp = DatumGetFloat8(result)+DatumGetInt16(elt1)*DatumGetInt16(elt2);break;
		case INT4OID:
			temp = DatumGetFloat8(result)+DatumGetInt32(elt1)*DatumGetInt32(elt2);break;
		case INT8OID:
			temp = DatumGetFloat8(result)+DatumGetInt64(elt1)*DatumGetInt64(elt2);break;
		case FLOAT4OID:
			temp = DatumGetFloat8(result)+DatumGetFloat4(elt1)*DatumGetFloat4(elt2);break;
		case FLOAT8OID:
			temp = DatumGetFloat8(result)+DatumGetFloat8(elt1)*DatumGetFloat8(elt2);break;
		default:
			ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
							errmsg("type is not supported"),
							errdetail("Arrays with element type %d are not supported.", (int)element_type)));
			break;
	}
	PG_RETURN_FLOAT8(temp);
}

Datum element_contains(Datum elt1, Datum elt2, Oid element_type, Datum result){
	switch(element_type){
		case INT2OID:
			return Float8GetDatum(DatumGetFloat8(result)+(((DatumGetInt16(elt1) == DatumGetInt16(elt2))||(DatumGetInt16(elt2)==0))?0:1));break;
		case INT4OID:
			return Float8GetDatum(DatumGetFloat8(result)+(((DatumGetInt32(elt1) == DatumGetInt32(elt2))||(DatumGetInt32(elt2)==0))?0:1));break;
		case INT8OID:
			return Float8GetDatum(DatumGetFloat8(result)+(((DatumGetInt64(elt1) == DatumGetInt64(elt2))||(DatumGetInt64(elt2)==0))?0:1));break;
		case FLOAT4OID:
			return Float8GetDatum(DatumGetFloat8(result)+(((DatumGetFloat4(elt1) == DatumGetFloat4(elt2))||(DatumGetFloat4(elt2)==0))?0:1));break;
		case FLOAT8OID:
			return Float8GetDatum(DatumGetFloat8(result)+(((DatumGetFloat8(elt1) == DatumGetFloat8(elt2))||(DatumGetFloat8(elt2)==0))?0:1));break;
		default:
			ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
							errmsg("type is not supported"),
							errdetail("Arrays with element type %d are not supported.", (int)element_type)));
			break;
	}
	return result;
}

inline char* element_set(Datum elt1, Datum elt2, Oid element_type, char* result){
	(void) elt1; /* avoid warning about unused parameter */
	switch(element_type){
		case INT2OID:
			*((int16*)(result)) = DatumGetInt16(elt2);break;
		case INT4OID:
			*((int32*)(result)) = DatumGetInt32(elt2);break;
		case INT8OID:
			*((int64*)(result)) = DatumGetInt64(elt2);break;
		case FLOAT4OID:
			*((float4*)(result)) = DatumGetFloat4(elt2);break;
		case FLOAT8OID:
			*((float8*)(result)) = DatumGetFloat8(elt2);break;
		default:
			ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
							errmsg("type is not supported"),
							errdetail("Arrays with element type %d are not supported.", (int)element_type)));
			break;
	}
	return result;
}

inline char* element_add(Datum elt1, Datum elt2, Oid element_type, char* result){
	switch(element_type){
		case INT2OID:
			*((int16*)(result)) = DatumGetInt16(elt1)+DatumGetInt16(elt2);break;
		case INT4OID:
			*((int32*)(result)) = DatumGetInt32(elt1)+DatumGetInt32(elt2);break;
		case INT8OID:
			*((int64*)(result)) = DatumGetInt64(elt1)+DatumGetInt64(elt2);break;
		case FLOAT4OID:
			*((float4*)(result)) = DatumGetFloat4(elt1)+DatumGetFloat4(elt2);break;
		case FLOAT8OID:
			*((float8*)(result)) = DatumGetFloat8(elt1)+DatumGetFloat8(elt2);break;
		default:
			ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
							errmsg("type is not supported"),
							errdetail("Arrays with element type %d are not supported.", (int)element_type)));
			break;
	}
	return result;
}

inline char* element_sub(Datum elt1, Datum elt2, Oid element_type, char* result){
	switch(element_type){
		case INT2OID:
			*((int16*)(result)) = DatumGetInt16(elt1)-DatumGetInt16(elt2);break;
		case INT4OID:
			*((int32*)(result)) = DatumGetInt32(elt1)-DatumGetInt32(elt2);break;
		case INT8OID:
			*((int64*)(result)) = DatumGetInt64(elt1)-DatumGetInt64(elt2);break;
		case FLOAT4OID:
			*((float4*)(result)) = DatumGetFloat4(elt1)-DatumGetFloat4(elt2);break;
		case FLOAT8OID:
			*((float8*)(result)) = DatumGetFloat8(elt1)-DatumGetFloat8(elt2);break;
		default:
			ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
							errmsg("type is not supported"),
							errdetail("Arrays with element type %d are not supported.", (int)element_type)));
			break;
	}
	return result;
}

inline char* element_mult(Datum elt1, Datum elt2, Oid element_type, char* result){
	switch(element_type){
		case INT2OID:
			*((int16*)(result)) = DatumGetInt16(elt1)*DatumGetInt16(elt2);break;
		case INT4OID:
			*((int32*)(result)) = DatumGetInt32(elt1)*DatumGetInt32(elt2);break;
		case INT8OID:
			*((int64*)(result)) = DatumGetInt64(elt1)*DatumGetInt64(elt2);break;
		case FLOAT4OID:
			*((float4*)(result)) = DatumGetFloat4(elt1)*DatumGetFloat4(elt2);break;
		case FLOAT8OID:
			*((float8*)(result)) = DatumGetFloat8(elt1)*DatumGetFloat8(elt2);break;
		default:
			ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
							errmsg("type is not supported"),
							errdetail("Arrays with element type %d are not supported.", (int)element_type)));
			break;
	}
	return result;
}

char* element_div(Datum elt1, Datum elt2, Oid element_type, char* result){
	switch(element_type){
		case INT2OID:
			if(DatumGetInt16(elt2) == 0){
				ereport(ERROR, (errcode(ERRCODE_DIVISION_BY_ZERO),
							errmsg("division by zero is not allowed"),
							errdetail("Arrays with element 0 can not be use in the denominator")));
			}
			*((int16*)(result)) = DatumGetInt16(elt1)/DatumGetInt16(elt2);break;
		case INT4OID:
			if(DatumGetInt32(elt2) == 0){
				ereport(ERROR, (errcode(ERRCODE_DIVISION_BY_ZERO),
								errmsg("division by zero is not allowed"),
								errdetail("Arrays with element 0 can not be use in the denominator")));
			}
			*((int32*)(result)) = DatumGetInt32(elt1)/DatumGetInt32(elt2);break;
		case INT8OID:
			if(DatumGetInt64(elt2) == 0){
				ereport(ERROR, (errcode(ERRCODE_DIVISION_BY_ZERO),
								errmsg("division by zero is not allowed"),
								errdetail("Arrays with element 0 can not be use in the denominator")));
			}
			*((int64*)(result)) = DatumGetInt64(elt1)/DatumGetInt64(elt2);break;
		case FLOAT4OID:
			if(DatumGetFloat4(elt2) == 0){
				ereport(ERROR, (errcode(ERRCODE_DIVISION_BY_ZERO),
								errmsg("division by zero is not allowed"),
								errdetail("Arrays with element 0 can not be use in the denominator")));
			}
			*((float4*)(result)) = DatumGetFloat4(elt1)/DatumGetFloat4(elt2);break;
		case FLOAT8OID:
			if(DatumGetFloat8(elt2) == 0){
				ereport(ERROR, (errcode(ERRCODE_DIVISION_BY_ZERO),
								errmsg("division by zero is not allowed"),
								errdetail("Arrays with element 0 can not be use in the denominator")));
			}
			*((float8*)(result)) = DatumGetFloat8(elt1)/DatumGetFloat8(elt2);break;
		default:
			ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
							errmsg("type is not supported"),
							errdetail("Arrays with element type %d are not supported.", (int)element_type)));
			break;
	}
	return result;
}

char* element_sqrt(Datum elt1, Datum elt2, Oid element_type, char* result){
	(void) elt2; /* avoid warning about unused parameter */
	switch(element_type){
		case INT2OID:
			*((int16*)(result)) = sqrt(DatumGetInt16(elt1));break;
		case INT4OID:
			*((int32*)(result)) = sqrt(DatumGetInt32(elt1));break;
		case INT8OID:
			*((int64*)(result)) = sqrt(DatumGetInt64(elt1));break;
		case FLOAT4OID:
			*((float4*)(result)) = sqrt(DatumGetFloat4(elt1));break;
		case FLOAT8OID:
			*((float8*)(result)) = sqrt(DatumGetFloat8(elt1));break;
		default:
			ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
							errmsg("type is not supported"),
							errdetail("Arrays with element type %d are not supported.", (int)element_type)));
			break;
	}
	return result;
}


PG_FUNCTION_INFO_V1(array_stddev);
Datum array_stddev(PG_FUNCTION_ARGS){
	ArrayType *v;
	Datum mean, res = Float8GetDatum(0);

	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();

	v = PG_GETARG_ARRAYTYPE_P(0);

	mean = General_Array_to_Element(v, res, element_sum, average_finalize, 1);
	res = General_Array_to_Element(v, mean, element_diff, average_root_finalize, 1);

	PG_FREE_IF_COPY(v, 0);

	return(res);
}

PG_FUNCTION_INFO_V1(array_mean);
Datum array_mean(PG_FUNCTION_ARGS){
	ArrayType *v;
	Datum res;
	Datum flag = Int16GetDatum(0);

	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();

	v = PG_GETARG_ARRAYTYPE_P(0);

	res = General_Array_to_Element(v, flag, element_sum, average_finalize, 1);

	PG_FREE_IF_COPY(v, 0);

	return(res);
}

PG_FUNCTION_INFO_V1(array_sum_big);
Datum array_sum_big(PG_FUNCTION_ARGS){
	ArrayType *v;
	Datum res;
	Datum flag = Int16GetDatum(0);

	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();

	v = PG_GETARG_ARRAYTYPE_P(0);

	res = General_Array_to_Element(v, flag, element_sum, noop_finalize, 1);

	PG_FREE_IF_COPY(v, 0);

	return(res);
}

PG_FUNCTION_INFO_V1(array_sum);
Datum array_sum(PG_FUNCTION_ARGS){
	ArrayType *v;
	Datum res;
	Datum flag = Int16GetDatum(0);

	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();

	v = PG_GETARG_ARRAYTYPE_P(0);

	res = General_Array_to_Element(v, flag, element_sum, noop_finalize, 0);

	PG_FREE_IF_COPY(v, 0);

	return(res);
}

PG_FUNCTION_INFO_V1(array_min);
Datum array_min(PG_FUNCTION_ARGS){
	ArrayType *v;
	Datum res;
	Datum flag = Int16GetDatum(0);

	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();

	v = PG_GETARG_ARRAYTYPE_P(0);

	res = General_Array_to_Element(v, flag, element_min, noop_finalize, 0);

	PG_FREE_IF_COPY(v, 0);

	return(res);
}

PG_FUNCTION_INFO_V1(array_max);
Datum array_max(PG_FUNCTION_ARGS){
	ArrayType *v;
	Datum res;
	Datum flag = Int16GetDatum(0);

	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();

	v = PG_GETARG_ARRAYTYPE_P(0);

	res = General_Array_to_Element(v, flag, element_max, noop_finalize, 0);

	PG_FREE_IF_COPY(v, 0);

	return(res);
}

PG_FUNCTION_INFO_V1(array_dot);
Datum array_dot(PG_FUNCTION_ARGS){
	ArrayType *v1;
	ArrayType *v2;
	Datum res;

	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();
	if (PG_ARGISNULL(1))
		PG_RETURN_NULL();

	v1 = PG_GETARG_ARRAYTYPE_P(0);
	v2 = PG_GETARG_ARRAYTYPE_P(1);

	res = General_2Array_to_Element(v1, v2, element_dot, noop_finalize, 1);

	PG_FREE_IF_COPY(v1, 0);
	PG_FREE_IF_COPY(v2, 1);

	return(res);
}

PG_FUNCTION_INFO_V1(array_contains);
Datum array_contains(PG_FUNCTION_ARGS){
	ArrayType *v1;
	ArrayType *v2;
	Datum res;

	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();
	if (PG_ARGISNULL(1))
		PG_RETURN_NULL();

	v1 = PG_GETARG_ARRAYTYPE_P(0);
	v2 = PG_GETARG_ARRAYTYPE_P(1);

	res = General_2Array_to_Element(v1, v2, element_contains, noop_finalize, 0);

	PG_FREE_IF_COPY(v1, 0);
	PG_FREE_IF_COPY(v2, 1);

	if(res == 0){
		PG_RETURN_BOOL(TRUE);
	}
	PG_RETURN_BOOL(FALSE);
}


PG_FUNCTION_INFO_V1(array_add);
Datum array_add(PG_FUNCTION_ARGS){
	ArrayType *v1;
	ArrayType *v2;
	Datum res;

	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();
	if (PG_ARGISNULL(1))
		PG_RETURN_NULL();

	v1 = PG_GETARG_ARRAYTYPE_P(0);
	v2 = PG_GETARG_ARRAYTYPE_P(1);

	res = General_2Array_to_Array(v1, v2, element_add);

	PG_FREE_IF_COPY(v1, 0);
	PG_FREE_IF_COPY(v2, 1);

	return(res);
}

PG_FUNCTION_INFO_V1(array_sub);
Datum array_sub(PG_FUNCTION_ARGS){
	ArrayType *v1;
	ArrayType *v2;
	Datum res;

	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();
	if (PG_ARGISNULL(1))
		PG_RETURN_NULL();

	v1 = PG_GETARG_ARRAYTYPE_P(0);
	v2 = PG_GETARG_ARRAYTYPE_P(1);

	res = General_2Array_to_Array(v1, v2, element_sub);

	PG_FREE_IF_COPY(v1, 0);
	PG_FREE_IF_COPY(v2, 1);

	return(res);
}

PG_FUNCTION_INFO_V1(array_mult);
Datum array_mult(PG_FUNCTION_ARGS){
	ArrayType *v1;
	ArrayType *v2;
	Datum res;

	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();
	if (PG_ARGISNULL(1))
		PG_RETURN_NULL();

	v1 = PG_GETARG_ARRAYTYPE_P(0);
	v2 = PG_GETARG_ARRAYTYPE_P(1);

	res = General_2Array_to_Array(v1, v2, element_mult);

	PG_FREE_IF_COPY(v1, 0);
	PG_FREE_IF_COPY(v2, 1);

	return(res);
}

PG_FUNCTION_INFO_V1(array_div);
Datum array_div(PG_FUNCTION_ARGS){
	ArrayType *v1;
	ArrayType *v2;
	Datum res;

	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();
	if (PG_ARGISNULL(1))
		PG_RETURN_NULL();

	v1 = PG_GETARG_ARRAYTYPE_P(0);
	v2 = PG_GETARG_ARRAYTYPE_P(1);

	res = General_2Array_to_Array(v1, v2, element_div);

	PG_FREE_IF_COPY(v1, 0);
	PG_FREE_IF_COPY(v2, 1);

	return(res);
}

PG_FUNCTION_INFO_V1(array_fill);
Datum array_fill(PG_FUNCTION_ARGS){
	ArrayType *v1;
	Datum v2;
	Datum res;

	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();
	if (PG_ARGISNULL(1))
		PG_RETURN_NULL();

	v1 = PG_GETARG_ARRAYTYPE_P(0);
	v2 = PG_GETARG_DATUM(1);

	res = General_Array_to_Array(v1, v2, element_set);

	PG_FREE_IF_COPY(v1, 0);
	return(res);
}

PG_FUNCTION_INFO_V1(array_scalar_mult);
Datum array_scalar_mult(PG_FUNCTION_ARGS){
	ArrayType *v1;
	Datum v2;
	Datum res;

	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();
	if (PG_ARGISNULL(1))
		PG_RETURN_NULL();

	v1 = PG_GETARG_ARRAYTYPE_P(0);
	v2 = PG_GETARG_DATUM(1);

	res = General_Array_to_Array(v1, v2, element_mult);

	PG_FREE_IF_COPY(v1, 0);
	return(res);
}

PG_FUNCTION_INFO_V1(array_sqrt);
Datum array_sqrt(PG_FUNCTION_ARGS){
	ArrayType *v1;
	Datum v2 = 0;
	Datum res;

	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();

	v1 = PG_GETARG_ARRAYTYPE_P(0);

	res = General_Array_to_Array(v1, v2, element_sqrt);

	PG_FREE_IF_COPY(v1, 0);
	return(res);
}

Datum General_Array_to_Element(ArrayType *v, Datum exta_val, Datum(*element_function)(Datum,Datum*,Oid,Datum), Datum(*finalize_function)(Datum,int,Oid), int flag){
	Datum result = Float8GetDatum((float8) 0.0);
	//in the future to add support for NUMERICOID (DatumGetFloat8(DirectFunctionCall1(numeric_float8_no_overflow,)), not supported at the moment

	int *dims, ndims,nitems;
	int i, type_size, bitmask, null_count = 0;
	bool typbyval;
	char *dat;
	bits8 *bitmap;

	Oid element_type;
	TypeCacheEntry *typentry;

	Datum elt;
	dims = ARR_DIMS(v);
	ndims = ARR_NDIM(v);

	element_type = ARR_ELEMTYPE(v);

	if (ndims == 0){
		Datum ret = 0;
		return ret;
	}

	dat = ARR_DATA_PTR(v);
	nitems = ArrayGetNItems(ndims, dims);

	typentry = lookup_type_cache(element_type,TYPECACHE_CMP_PROC_FINFO);
	type_size = typentry->typlen;
	typbyval = typentry->typbyval;

	if(flag == 0){
		switch(element_type){
			case INT2OID:
				result = Int16GetDatum((int16) 0.0);break;
			case INT4OID:
				result = Int32GetDatum((int32) 0.0);break;
			case INT8OID:
				result = Int64GetDatum((int64) 0.0);break;
			case FLOAT4OID:
				result = Float4GetDatum((float4) 0.0);break;
			case FLOAT8OID:
				result = Float8GetDatum((float8) 0.0);break;
			default:
				ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
								errmsg("type is not supported"),
								errdetail("Arrays with element type %d are not supported.", (int)element_type)));
				break;
		}
	}

	bitmap = ARR_NULLBITMAP(v);
	bitmask = 1;
	if(ARR_HASNULL(v)){
		for (i = 0; i < nitems; i++)
		{
			/* Get elements, checking for NULL */
			if (!(bitmap && (*bitmap & bitmask) == 0))
			{
				elt = fetch_att(dat, typbyval, type_size);
				dat = att_addlength_pointer(dat, type_size, dat);
				dat = (char *) att_align_nominal(dat, typentry->typalign);

				switch(element_type){
					case INT2OID:
						result = element_function(elt,&exta_val,element_type,result);break;
					case INT4OID:
						result = element_function(elt,&exta_val,element_type,result);break;
					case INT8OID:
						result = element_function(elt,&exta_val,element_type,result);break;
					case FLOAT4OID:
						result = element_function(elt,&exta_val,element_type,result);break;
					case FLOAT8OID:
						result = element_function(elt,&exta_val,element_type,result);break;
					default:
						ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
										errmsg("type is not supported"),
										errdetail("Arrays with element type %d are not supported.", (int)element_type)));
						break;
				}

				/* advance bitmap pointers if any */
				bitmask <<= 1;
				if (bitmask == 0x100)
				{
					if (bitmap)
						bitmap++;
					bitmask = 1;
				}
			}else{
				null_count++;
			}
		}
	}else{
		for (i = 0; i < nitems; i++){
			elt = fetch_att(dat, typbyval, type_size);
			dat = att_addlength_pointer(dat, type_size, dat);
			dat = (char *) att_align_nominal(dat, typentry->typalign);

			switch(element_type){
				case INT2OID:
					result = element_function(elt,&exta_val,element_type,result);break;
				case INT4OID:
					result = element_function(elt,&exta_val,element_type,result);break;
				case INT8OID:
					result = element_function(elt,&exta_val,element_type,result);break;
				case FLOAT4OID:
					result = element_function(elt,&exta_val,element_type,result);break;
				case FLOAT8OID:
					result = element_function(elt,&exta_val,element_type,result);break;
				default:
					ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
									errmsg("type is not supported"),
									errdetail("Arrays with element type %d are not supported.", (int)element_type)));
					break;
			}
		}
	}

	return finalize_function(result,(nitems-null_count), DatumGetObjectId(result));
}

Datum General_2Array_to_Element(ArrayType *v1, ArrayType *v2, Datum(*element_function)(Datum,Datum,Oid,Datum), Datum(*finalize_function)(Datum,int,Oid), int flag){
	Datum result = Float8GetDatum((float8) 0.0);

	int ndims,nitems;
	int *dims1,*lbs1,ndims1,nitems1;
	int *dims2,*lbs2,ndims2;
	int i, type_size;
	bool typbyval;
	char *dat1,*dat2;

	Oid element_type;
	TypeCacheEntry *typentry;

	Datum elt1;
	Datum elt2;

	ndims1 = ARR_NDIM(v1);
	ndims2 = ARR_NDIM(v2);

	if(ndims1 != ndims2){
		ereport(ERROR, (errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR),
						errmsg("cannot operate on arrays of different dimention"),
						errdetail("Arrays with element dimention %d and %d are not compatible for addition.", ndims1, ndims2)));
	}

	element_type = ARR_ELEMTYPE(v1);

	if ((ndims1 == 0) || (ndims2 == 0)){
		Datum ret = 0;
		return ret;
	}


	lbs1 = ARR_LBOUND(v1);
	lbs2 = ARR_LBOUND(v2);
	dims1 = ARR_DIMS(v1);
	dims2 = ARR_DIMS(v2);

	dat1 = ARR_DATA_PTR(v1);
	dat2 = ARR_DATA_PTR(v2);
	nitems1 = ArrayGetNItems(ndims1, dims1);

	ndims = ndims1;
	for (i = 0; i < ndims; i++)
	{
		if (dims1[i] != dims2[i] || lbs1[i] != lbs2[i]){
			ereport(ERROR, (errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR),
							errmsg("cannot compute this operation on arrays of different length"),
							errdetail("Arrays with element length %d and %d are not compatible for this operation.", dims1[i], dims2[i])));
		}
	}
	nitems =nitems1;
	if(ARR_HASNULL(v1)||ARR_HASNULL(v2)){
		ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
						errmsg("arrays cannot contain nulls"),
						errdetail("Arrays with element value NULL are not allowed.")));
	}
	typentry = lookup_type_cache(element_type,TYPECACHE_CMP_PROC_FINFO);
	type_size = typentry->typlen;
	typbyval = typentry->typbyval;

	if(flag == 0){
		switch(element_type){
			case INT2OID:
				result = Int16GetDatum((int16) 0.0);break;
			case INT4OID:
				result = Int32GetDatum((int32) 0.0);break;
			case INT8OID:
				result = Int64GetDatum((int64) 0.0);break;
			case FLOAT4OID:
				result = Float4GetDatum((float4) 0.0);break;
			case FLOAT8OID:
				result = Float8GetDatum((float8) 0.0);break;
			default:
				ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
								errmsg("type is not supported"),
								errdetail("Arrays with element type %d are not supported.", (int)element_type)));
				break;
		}
	}

	for (i = 0; i < nitems; ++i){
		elt1 = fetch_att(dat1, typbyval, type_size);
		dat1 = att_addlength_pointer(dat1, type_size, dat1);
		dat1 = (char *) att_align_nominal(dat1, typentry->typalign);
		elt2 = fetch_att(dat2, typbyval, type_size);
		dat2 = att_addlength_pointer(dat2, type_size, dat2);
		dat2 = (char *) att_align_nominal(dat2, typentry->typalign);

		switch(element_type){
			case INT2OID:
				result = element_function(elt1,elt2,element_type,result);break;
			case INT4OID:
				result = element_function(elt1,elt2,element_type,result);break;
			case INT8OID:
				result = element_function(elt1,elt2,element_type,result);break;
			case FLOAT4OID:
				result = element_function(elt1,elt2,element_type,result);break;
			case FLOAT8OID:
				result = element_function(elt1,elt2,element_type,result);break;
			default:
				ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
								errmsg("type is not supported"),
								errdetail("Arrays with element type %d are not supported.", (int)element_type)));
				break;
		}
	}

	return finalize_function(result, nitems, DatumGetObjectId(result));
}

Datum General_2Array_to_Array(ArrayType *v1, ArrayType *v2, char*(*element_function)(Datum,Datum,Oid,char*)){
	ArrayType *pgarray = NULL;
	char *result = NULL, *result_point;

	int *dims,*lbs,ndims,nitems;
	int *dims1,*lbs1,ndims1;
	int *dims2,*lbs2,ndims2;
	int i, type_size;
	bool typbyval;
	char *dat1,*dat2;

	Oid element_type;
	TypeCacheEntry *typentry;

	Datum elt1;
	Datum elt2;

	ndims1 = ARR_NDIM(v1);
	ndims2 = ARR_NDIM(v2);

	if(ndims1 != ndims2){
		ereport(ERROR, (errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR),
						errmsg("cannot perform operation arrays of different dimention"),
						errdetail("Arrays with element dimention %d and %d are not compatible for addition.", ndims1, ndims2)));
	}

	element_type = ARR_ELEMTYPE(v1);

	if (ndims1 == 0 && ndims2 > 0)
		PG_RETURN_ARRAYTYPE_P(v2);
	if (ndims2 == 0)
		PG_RETURN_ARRAYTYPE_P(v1);


	lbs1 = ARR_LBOUND(v1);
	lbs2 = ARR_LBOUND(v2);
	dims1 = ARR_DIMS(v1);
	dims2 = ARR_DIMS(v2);

	dat1 = ARR_DATA_PTR(v1);
	dat2 = ARR_DATA_PTR(v2);

	ndims = ndims1;
	dims = (int *) palloc(ndims * sizeof(int));
	lbs = (int *) palloc(ndims * sizeof(int));

	for (i = 0; i < ndims; i++)
	{
		if (dims1[i] != dims2[i] || lbs1[i] != lbs2[i]){
			ereport(ERROR, (errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR),
							errmsg("cannot operate on arrays of different length"),
							errdetail("Arrays with element length %d and %d are not compatible for operations.", dims1[i], dims2[i])));
		}
		dims[i] = dims1[i];
		lbs[i] = lbs1[i];
	}
	nitems = ArrayGetNItems(ndims, dims);
	if(ARR_HASNULL(v1)||ARR_HASNULL(v2)){
		ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
						errmsg("arrays cannot contain nulls"),
						errdetail("Arrays with element value NULL are not allowed.")));
	}
	typentry = lookup_type_cache(element_type,TYPECACHE_CMP_PROC_FINFO);

	type_size = typentry->typlen;
	typbyval = typentry->typbyval;

	switch(element_type){
		case INT2OID:
			result = (char *)palloc(type_size*nitems);break;
		case INT4OID:
			result = (char *)palloc(type_size*nitems);break;
		case INT8OID:
			result = (char *)palloc(type_size*nitems);break;
		case FLOAT4OID:
			result = (char *)palloc(type_size*nitems);break;
		case FLOAT8OID:
			result = (char *)palloc(type_size*nitems);break;
		default:
			ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
							errmsg("type is not supported"),
							errdetail("Arrays with element type %d are not supported.", (int)element_type)));
			break;
	}
	memset(result, 0, type_size*nitems);
    result_point = result;

	char* result_val = (char *)palloc(type_size);
	for (i = 0; i < nitems; ++i){
		elt1 = fetch_att(dat1, typbyval, type_size);
		dat1 = att_addlength_pointer(dat1, type_size, dat1);
		dat1 = (char *) att_align_nominal(dat1, typentry->typalign);
		elt2 = fetch_att(dat2, typbyval, type_size);
		dat2 = att_addlength_pointer(dat2, type_size, dat2);
		dat2 = (char *) att_align_nominal(dat2, typentry->typalign);

		switch(element_type){
			case INT2OID:
				*((int16*)(result_point)) = *(int16*)(element_function(elt1,elt2,element_type,result_val));break;
			case INT4OID:
				*((int32*)(result_point)) = *(int32*)(element_function(elt1,elt2,element_type,result_val));break;
			case INT8OID:
				*((int64*)(result_point)) = *(int64*)(element_function(elt1,elt2,element_type,result_val));break;
			case FLOAT4OID:
				*((float4*)(result_point)) = *(float4*)(element_function(elt1,elt2,element_type,result_val));break;
			case FLOAT8OID:
				*((float8*)(result_point)) = *(float8*)(element_function(elt1,elt2,element_type,result_val));break;
			default:
				ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
								errmsg("type is not supported"),
								errdetail("Arrays with element type %d are not supported.", (int)element_type)));
				break;
		}
		result_point+=type_size;
	}

	switch(element_type){
		case INT2OID:
			pgarray = construct_array((Datum *)result,nitems,INT2OID,sizeof(int64),true,'d');break;
		case INT4OID:
			pgarray = construct_array((Datum *)result,nitems,INT4OID,sizeof(int64),true,'d');break;
		case INT8OID:
			pgarray = construct_array((Datum *)result,nitems,INT8OID,sizeof(int64),true,'d');break;
		case FLOAT4OID:
			pgarray = construct_array((Datum *)result,nitems,FLOAT4OID,sizeof(float8),true,'d');break;
		case FLOAT8OID:
			pgarray = construct_array((Datum *)result,nitems,FLOAT8OID,sizeof(float8),true,'d');break;
		default:
			ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
							errmsg("type is not supported"),
							errdetail("Arrays with element type %d are not supported.", (int)element_type)));
			break;
	}

	pgarray->ndim = ndims;
	PG_RETURN_ARRAYTYPE_P(pgarray);
}

Datum General_Array_to_Array(ArrayType *v1, Datum elt2, char*(*element_function)(Datum,Datum,Oid,char*)){
	ArrayType *pgarray = NULL;
	char *result = NULL, *result_point;

	int *dims,*lbs,ndims,nitems;
	int *dims1,*lbs1,ndims1;
	int i, type_size;
	bool typbyval;
	char *dat1;

	Oid element_type;
	TypeCacheEntry *typentry;

	Datum elt1;

	ndims1 = ARR_NDIM(v1);
	element_type = ARR_ELEMTYPE(v1);

	if (ndims1 == 0)
		PG_RETURN_ARRAYTYPE_P(v1);

	lbs1 = ARR_LBOUND(v1);
	dims1 = ARR_DIMS(v1);

	dat1 = ARR_DATA_PTR(v1);

	ndims = ndims1;
	dims = (int *) palloc(ndims * sizeof(int));
	lbs = (int *) palloc(ndims * sizeof(int));

	for (i = 0; i < ndims; i++)
	{
		dims[i] = dims1[i];
		lbs[i] = lbs1[i];
	}
	nitems = ArrayGetNItems(ndims, dims);
	if(ARR_HASNULL(v1)){
		ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
						errmsg("arrays cannot contain nulls"),
						errdetail("Arrays with element value NULL are not allowed.")));
	}
	typentry = lookup_type_cache(element_type,TYPECACHE_CMP_PROC_FINFO);

	type_size = typentry->typlen;
	typbyval = typentry->typbyval;

	switch(element_type){
		case INT2OID:
			result = (char *)palloc(type_size*nitems);break;
		case INT4OID:
			result = (char *)palloc(type_size*nitems);break;
		case INT8OID:
			result = (char *)palloc(type_size*nitems);break;
		case FLOAT4OID:
			result = (char *)palloc(type_size*nitems);break;
		case FLOAT8OID:
			result = (char *)palloc(type_size*nitems);break;
		default:
			ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
							errmsg("type is not supported"),
							errdetail("Arrays with element type %d are not supported.", (int)element_type)));
			break;
	}
	memset(result, 0, type_size*nitems);
    result_point = result;

	char* result_val = (char *)palloc(type_size);
	for (i = 0; i < nitems; ++i){
		elt1 = fetch_att(dat1, typbyval, type_size);
		dat1 = att_addlength_pointer(dat1, type_size, dat1);
		dat1 = (char *) att_align_nominal(dat1, typentry->typalign);

		switch(element_type){
			case INT2OID:
				*((int16*)(result_point)) = *(int16*)(element_function(elt1,elt2,element_type,result_val));break;
			case INT4OID:
				*((int32*)(result_point)) = *(int32*)(element_function(elt1,elt2,element_type,result_val));break;
			case INT8OID:
				*((int64*)(result_point)) = *(int64*)(element_function(elt1,elt2,element_type,result_val));break;
			case FLOAT4OID:
				*((float4*)(result_point)) = *(float4*)(element_function(elt1,elt2,element_type,result_val));break;
			case FLOAT8OID:
				*((float8*)(result_point)) = *(float8*)(element_function(elt1,elt2,element_type,result_val));break;
			default:
				ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
								errmsg("type is not supported"),
								errdetail("Arrays with element type %d are not supported.", (int)element_type)));
				break;
		}
		result_point+=type_size;
	}

	switch(element_type){
		case INT2OID:
			pgarray = construct_array((Datum *)result,nitems,INT2OID,sizeof(int64),true,'d');break;
		case INT4OID:
			pgarray = construct_array((Datum *)result,nitems,INT4OID,sizeof(int64),true,'d');break;
		case INT8OID:
			pgarray = construct_array((Datum *)result,nitems,INT8OID,sizeof(int64),true,'d');break;
		case FLOAT4OID:
			pgarray = construct_array((Datum *)result,nitems,FLOAT4OID,sizeof(float8),true,'d');break;
		case FLOAT8OID:
			pgarray = construct_array((Datum *)result,nitems,FLOAT8OID,sizeof(float8),true,'d');break;
		default:
			ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
							errmsg("type is not supported"),
							errdetail("Arrays with element type %d are not supported.", (int)element_type)));
			break;
	}

	pgarray->ndim = ndims;
	PG_RETURN_ARRAYTYPE_P(pgarray);
}



/*
 * This function normalizes an array.
 */
PG_FUNCTION_INFO_V1(array_normalize);
Datum array_normalize(PG_FUNCTION_ARGS)
{
	ArrayType * arg = PG_GETARG_ARRAYTYPE_P(0);

	if (ARR_NDIM(arg) != 1 || ARR_ELEMTYPE(arg) != FLOAT8OID)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("function \"%s\" called with invalid parameters",
						format_procedure(fcinfo->flinfo->fn_oid))));

	float8 * x = (float8 *)ARR_DATA_PTR(arg);
	int dim = ARR_DIMS(arg)[0];

	float8 d = 0;
	for (int i=0; i!=dim; i++)
		d += x[i] * x[i];

	if (d > 0 && d != 1.0) {
		/* Allocate memory for return value */
		Datum * array = palloc0(dim*sizeof(Datum));
		ArrayType * ret = construct_array(array,dim,FLOAT8OID,sizeof(float8),true,'i');
		float8 * ret_v = (float8 *)ARR_DATA_PTR(ret);

		float8 sqrtd = sqrt(d);

		for (int i=0; i!=dim; i++)
			ret_v[i] = x[i] / sqrtd;

		PG_RETURN_ARRAYTYPE_P(ret);
	} else {
		PG_RETURN_ARRAYTYPE_P(arg);
	}

}

/*
 * This function checks if an array contains NULL values.
 */
PG_FUNCTION_INFO_V1(array_contains_null);
bool array_contains_null(PG_FUNCTION_ARGS){
	ArrayType * arg = PG_GETARG_ARRAYTYPE_P(0);
	if(ARR_HASNULL(arg)){
		return(true);
	} else {
		return(false);
	}
}
