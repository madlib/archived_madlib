/**
 * @file
 * \brief This file defines the SparseData structure and some basic 
 * functions on SparseData.
 *
 * SparseData provides array storage for repetitive data as commonly found
 * in numerical analysis of sparse arrays and matrices. 
 * A general run-length encoding scheme is adopted.
 * Sequential duplicate values in the array are represented in an index
 * structure that stores the count of the number of times a given value
 * is duplicated.
 * All storage is allocated with palloc().
 *
 * NOTE:
 * The SparseData structure is an in-memory structure and so must be
 * serialized into a persisted structure like a VARLENA when leaving
 * a GP / Postgres function.  This implies a COPY from the SparseData
 * to the VARLENA.
 */

#ifndef SPARSEDATA_H
#define SPARSEDATA_H

#include <math.h>
#include <string.h>
#include "postgres.h"
#include "lib/stringinfo.h"
#include "utils/array.h"
#include "catalog/pg_type.h"

/*!
 * \internal 
 * SparseData holds information about a sparse array of values
 * \endinternal 
 */
typedef struct 
{
	Oid type_of_data; 	/**< The native type of the data entries */
	int unique_value_count; /**< The number of unique values in the data array */
	int total_value_count;  /**< The total number of values, including duplicates */
	StringInfo vals;        /**< The unique number values are stored here as a stream of bytes */
	StringInfo index; 	/**< A count of each value is stored in the index */
} SparseDataStruct;

/*
 * Sometimes we wish to store an uncompressed array inside a SparseDataStruct;
 * instead of storing an array of ones [1,1,..,1,1] in the index field, which
 * is wasteful, we choose to use index->data == NULL to represent this special 
 * case. 
 */

/** 
 * Pointer to a SparseDataStruct
 */
typedef SparseDataStruct *SparseData;

/*------------------------------------------------------------------------------
 * Serialized SparseData
 *------------------------------------------------------------------------------
 * SparseDataStruct Contents
 * StringInfoData Contents for "vals"
 * StringInfoData Contents for "index"
 * data contents for "vals" (size is vals->maxlen)
 * data contents for "index" (size is index->maxlen)
 *
 * 	The vals and index fields are serialized as StringInfoData, then the
 * 	data contents are serialized at the end.
 *
 * 	Since two StringInfoData structs together are 64-bit aligned, there's
 * 	no need for padding.
 *
 * 	For reference, here is the format of the StringInfoData:
 * 		char * dataptr;
 * 			-> a placeholder in the serialized version, is filled
 * 			   when the serialized version is used in-place
 * 		int len;
 * 		int maxlen;
 * 		int cursor;
 */ 
/** 
 * @return The size of a serialized SparseData based on the actual consumed
 * length of the StringInfo data and StringInfoData structures.
 */
#define SIZEOF_SPARSEDATAHDR	MAXALIGN(sizeof(SparseDataStruct))
/** 
 * @param x a SparseData
 * @return The size of x minus the dynamic variables, plus two 
 * integers describing the length of the data area and index
 */
#define SIZEOF_SPARSEDATASERIAL(x) (SIZEOF_SPARSEDATAHDR + \
		(2*sizeof(StringInfoData)) + \
		(x)->vals->maxlen + (x)->index->maxlen)

/*
 * The following take a serialized SparseData as an argument and return
 * pointers to locations inside.
 */
#define SDATA_DATA_SINFO(x)	((char *)(x)+SIZEOF_SPARSEDATAHDR)
#define SDATA_INDEX_SINFO(x)	(SDATA_DATA_SINFO(x)+sizeof(StringInfoData))
#define SDATA_DATA_SIZE(x)	(((StringInfo)SDATA_DATA_SINFO(x))->maxlen)
#define SDATA_INDEX_SIZE(x)	(((StringInfo)SDATA_INDEX_SINFO(x))->maxlen)
#define SDATA_VALS_PTR(x)       (SDATA_INDEX_SINFO(x)+sizeof(StringInfoData))
#define SDATA_INDEX_PTR(x) 	(SDATA_VALS_PTR(x)+SDATA_DATA_SIZE(x))

#define SDATA_UNIQUE_VALCNT(x)	(((SparseData)(x))->unique_value_count)
#define SDATA_TOTAL_VALCNT(x)	(((SparseData)(x))->total_value_count)

/** 
 * @param x a SparseData
 * @return True if x is a scalar */
#define SDATA_IS_SCALAR(x)	(((((x)->unique_value_count)==((x)->total_value_count))&&((x)->total_value_count==1)) ? 1 : 0)

/** 
 * @param ptr Pointer to the start of the count entry of a SparseData
 * @return The size of the integer count in an RLE index pointed to by ptr 
 */
#define	int8compstoragesize(ptr) \
 (((ptr) == NULL) ? 0 : (((*((char *)(ptr)) < 0) ? 1 : (1 + *((char *)(ptr))))))
/* The size of a compressed int8 is stored in the first element of the ptr 
 * array; see the explanation at the int8_to_compword() function below.
 *
 * Note that if the ptr is NULL, a zero size is returned
 */

static inline void int8_to_compword(int64 num, char entry[9]);

/** Serialization function */
void serializeSparseData(char *target, SparseData source);

/* Constructors and destructors */
SparseData makeEmptySparseData(void);
SparseData makeInplaceSparseData(char *vals, char *index,
		int datasize, int indexsize, Oid datatype,
		int unique_value_count, int total_value_count);

SparseData makeSparseDataCopy(SparseData source_sdata);
SparseData makeSparseDataFromDouble(double scalar,int64 dimension);

SparseData makeSparseData(void);

void freeSparseData(SparseData sdata);
void freeSparseDataAndData(SparseData sdata);

StringInfo copyStringInfo(StringInfo source_sinfo);
StringInfo makeStringInfoFromData(char *data,int len);

/* Conversion to and from arrays */
double *sdata_to_float8arr(SparseData sdata);
int64 *sdata_index_to_int64arr(SparseData sdata);
SparseData float8arr_to_sdata(double *array, int count);
SparseData position_to_sdata(double *array_val, int64 *array_pos, size_t width, Oid type_of_data, int count, int64 end, double default_val);
SparseData arr_to_sdata(char *array, size_t width, Oid type_of_data, int count);
SparseData posit_to_sdata(char *array, int64* array_pos, size_t width, Oid type_of_data, int count, int64 end, char *base_val);

/* Some functions for accessing and changing elements of a SparseData */
SparseData lapply(text * func, SparseData sdata);
double sd_proj(SparseData sdata, int idx);
SparseData subarr(SparseData sdata, int start, int end);
SparseData reverse(SparseData sdata);
SparseData concat(SparseData left, SparseData right);

/* Returns the size of each basic type 
 */
static inline size_t
size_of_type(Oid type)
{
	switch (type)
	{ 
		case FLOAT4OID: return(4);
		case FLOAT8OID: return(8);
		case CHAROID: return(1);
		case INT2OID: return(2);
		case INT4OID: return(4);
		case INT8OID: return(8);
	}
	return(1);
}

/* Appends a count to the count array 
 */
static inline void append_to_rle_index(StringInfo index, int64 run_len)
{
	char bytes[]={0,0,0,0,0,0,0,0,0}; /* 9 bytes into which the compressed
					     int8 value is written */
	int8_to_compword(run_len,bytes); /* create compressed version of
					    int8 value */
	appendBinaryStringInfo(index,bytes,int8compstoragesize(bytes));
}

/* Adds a new block to a SparseData
 */
static inline void add_run_to_sdata(char *run_val, int64 run_len, size_t width,
		SparseData sdata)
{
	StringInfo index = sdata->index;
	StringInfo vals  = sdata->vals;

	appendBinaryStringInfo(vals,run_val,width);
	append_to_rle_index(index, run_len);
	sdata->unique_value_count++;
	sdata->total_value_count+=run_len;
}
/*------------------------------------------------------------------------------
 * Each integer count in the RLE index is stored in a number of bytes determined
 * by its size.  The larger the integer count, the larger the size of storage.
 * Following is the map of count maximums to storage size:
 * 	Range			Storage
 * 	---------		-----------------------------------------
 * 	0     - 127		signed char stores the negative count
 *
 * 		All higher than 127 have two parts, the description byte
 * 		and the count word
 *
 * 	description byte	signed char stores the number of bytes in the
 * 				count word one of 1,2,4 or 8
 *
 * 	128   - 32767		count word is 2 bytes, signed int16_t
 * 	32768 - 2147483648	count word is 4 bytes, signed int32_t
 * 	2147483648 - max	count word is 8 bytes, signed int64
 *------------------------------------------------------------------------------
 */
/* Transforms an int64 value to an RLE entry.  The entry is placed in the
 * provided entry[] array and will have a variable size depending on the value.
 */
static inline void int8_to_compword(int64 num, char entry[9])
{
	if (num < 128) {
		/* The reason this is negative is because entry[0] is
	           used to record sizes in the other cases. */
		entry[0] = -(char)num;
		return;
	} 

	entry[1] = (char)(num & 0xFF);
	entry[2] = (char)((num & 0xFF00) >> 8);

	if (num < 32768) { entry[0] = 2; return; }

	entry[3] = (char)((num & 0xFF0000) >> 16);
	entry[4] = (char)((num & 0xFF000000) >> 24);

	if (num < 2147483648) { entry[0] = 4; return; }

	entry[5] = (char)((num & 0xFF00000000) >> 32);
	entry[6] = (char)((num & 0xFF0000000000) >> 40);
	entry[7] = (char)((num & 0xFF000000000000) >> 48);
	entry[8] = (char)((num & 0xFF00000000000000) >> 56);

	entry[0] = 8;
}

/* Transforms a count entry into an int64 value when provided with a pointer
 * to an entry.
 */
static inline int64 compword_to_int8(const char *entry)
{
	char size = int8compstoragesize(entry);
	int16_t num_2;
	char *numptr2 = (char *)(&num_2);
	int32_t num_4;
	char *numptr4 = (char *)(&num_4);
	int64 num;
	char *numptr8 = (char *)(&num);

	switch(size) {
	        case 0: /* entry == NULL represents an array of ones; see 
			 * comment after definition of SparseDataStruct above
			 */
			return 1; break;
		case 1:
			num = -(entry[0]);
			break;
		case 3:
			numptr2[0] = entry[1];
			numptr2[1] = entry[2];
			num = num_2;
			break;
		case 5:
			numptr4[0] = entry[1];
			numptr4[1] = entry[2];
			numptr4[2] = entry[3];
			numptr4[3] = entry[4];
			num = num_4;
			break;
		case 9:
			numptr8[0] = entry[1];
			numptr8[1] = entry[2];
			numptr8[2] = entry[3];
			numptr8[3] = entry[4];
			numptr8[4] = entry[5];
			numptr8[5] = entry[6];
			numptr8[6] = entry[7];
			numptr8[7] = entry[8];
			break;
	}
	return num;
}

static inline void printout_double(double *vals, int num_values, int stop);
static inline void printout_double(double *vals, int num_values, int stop)
{
	(void) stop; /* avoid warning about unused parameter */
	char *output_str = (char *)palloc(sizeof(char)*(num_values*(6+18+2))+1);
	char *str = output_str;
	int numout;
	for (int i=0; i<num_values; i++) {
		numout = snprintf(str,26,"%6.2f,%#llX,",vals[i],
				  *((long long unsigned int *)(&(vals[i]))));
		str += numout-1;
	}
	*str = '\0';
	elog(NOTICE,"doubles:%s",output_str);
}
static inline void printout_index(char *ix, int num_values, int stop);
static inline void printout_index(char *ix, int num_values, int stop)
{
	(void) stop; /* avoid warning about unused parameter */
	char *output_str = (char *)palloc(sizeof(char)*((num_values*7)+1));
	char *str = output_str;
	int numout;
	elog(NOTICE,"num_values=%d",num_values);
	for (int i=0; i<num_values; i++,ix+=int8compstoragesize(ix)) {
		numout=snprintf(str,7,"%lld,",(long long int)compword_to_int8(ix));
		str+=numout;
	}
	*str = '\0';
	elog(NOTICE,"index:%s",output_str);
}
static inline void printout_sdata(SparseData sdata, char *msg, int stop);
static inline void printout_sdata(SparseData sdata, char *msg, int stop)
{
	elog(NOTICE,"%s ==> unvct,tvct,ilen,dlen,datatype=%d,%d,%d,%d,%d",
			msg,
			sdata->unique_value_count,sdata->total_value_count,
			sdata->index->len,sdata->vals->len,
			sdata->type_of_data);
	{
		char *ix=sdata->index->data;
		double *ar=(double *)(sdata->vals->data);
		printout_double(ar,sdata->unique_value_count,0);
		printout_index(ix,sdata->unique_value_count,0);
	}

	if (stop)
	  ereport(ERROR, 
			  (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			   errmsg("LAL STOP")));
}

/*------------------------------------------------------------------------------
 * Multiplication, Addition, Division by scalars
 *------------------------------------------------------------------------------
 */
#define typref(type,ptr) (*((type *)(ptr)))
#define valref(type,val,i) (((type *)(val)->vals->data)[(i)])

#define valsquare(val)	(val*val)
#define valcube(val)	(val*valsquare(val))
#define valquad(val)	(valsquare(valsquare(val)))

#define apply_const_to_sdata(sdata,i,op,scalar) \
	switch ((sdata)->type_of_data) \
{ \
	case FLOAT4OID: \
		valref(float,sdata,i) op typref(float,scalar); \
		break; \
	case FLOAT8OID: \
		valref(float8,sdata,i) op typref(float8,scalar); \
		break; \
	case CHAROID: \
		valref(char,sdata,i) op typref(char,scalar); \
		break; \
	case INT2OID: \
		valref(int16,sdata,i) op typref(int16,scalar); \
		break; \
	case INT4OID: \
		valref(int32,sdata,i) op typref(int32,scalar); \
		break; \
	case INT8OID: \
		valref(int64,sdata,i) op typref(int64,scalar); \
		break; \
}

#define apply_scalar_left_to_sdata(sdata,i,op,scalar) \
	switch ((sdata)->type_of_data) \
{ \
	case FLOAT4OID: \
		valref(float,sdata,i) = \
		typref(float,scalar) op valref(float,sdata,i); \
		break; \
	case FLOAT8OID: \
		valref(float8,sdata,i) = \
		typref(float8,scalar) op valref(float8,sdata,i); \
		break; \
	case CHAROID: \
		valref(char,sdata,i) = \
		typref(char,scalar) op valref(char,sdata,i); \
		break; \
	case INT2OID: \
		valref(int16,sdata,i) = \
		typref(int16,scalar) op valref(int16_t,sdata,i); \
		break; \
	case INT4OID: \
		valref(int32,sdata,i) = \
		typref(int32,scalar) op valref(int32_t,sdata,i); \
		break; \
	case INT8OID: \
		valref(int64,sdata,i) = \
		typref(int64,scalar) op valref(int64,sdata,i); \
		break; \
}

#define accum_sdata_result(result,left,i,op,right,j) \
	switch ((left)->type_of_data) \
{ \
	case FLOAT4OID: \
		typref(float,result) = \
		valref(float,left,i) op \
		valref(float,right,j); \
		break; \
	case FLOAT8OID: \
		typref(float8,result) = \
		valref(float8,left,i) op \
		valref(float8,right,j); \
		break; \
	case CHAROID: \
		typref(char,result) = \
		valref(char,left,i) op \
		valref(char,right,j); \
		break; \
	case INT2OID: \
		typref(int16,result) = \
		valref(int16,left,i) op \
		valref(int16,right,j); \
		break; \
	case INT4OID: \
		typref(int32,result) = \
		valref(int32,left,i) op \
		valref(int32,right,j); \
		break; \
	case INT8OID: \
		typref(int64,result) = \
		valref(int64,left,i) op \
		valref(int64,right,j); \
		break; \
}

#define apply_function_sdata_scalar(result,func,left,i,scalar) \
	switch ((left)->type_of_data) \
{ \
	case FLOAT4OID: \
		valref(float,result,i) =\
		func(valref(float,left,i),typref(float,scalar)); \
		break; \
	case FLOAT8OID: \
		valref(float8,result,i) =\
		func(valref(float8,left,i),typref(float8,scalar)); \
		break; \
	case CHAROID: \
		valref(char,result,i) =\
		func(valref(char,left,i),typref(char,scalar)); \
		break; \
	case INT2OID: \
		valref(int16,result,i) =\
		func(valref(int16,left,i),typref(int16,scalar)); \
		break; \
	case INT4OID: \
		valref(int32,result,i) =\
		func(valref(int32,left,i),typref(int32,scalar)); \
		break; \
	case INT8OID: \
		valref(int64,result,i) =\
		func(valref(int64,left,i),typref(int64,scalar)); \
		break; \
}

#define apply_square_sdata(result,left,i) \
	switch ((left)->type_of_data) \
{ \
	case FLOAT4OID: \
		valref(float,result,i) = \
		valsquare(valref(float,left,i)); \
		break; \
	case FLOAT8OID: \
		valref(float8,result,i) = \
		valsquare(valref(float8,left,i));\
		break; \
	case CHAROID: \
		valref(char,result,i) = \
		valsquare(valref(char,left,i));\
		break; \
	case INT2OID: \
		valref(int16_t,result,i) = \
		valsquare(valref(int16_t,left,i));\
		break; \
	case INT4OID: \
		valref(int32_t,result,i) = \
		valsquare(valref(int32_t,left,i));\
		break; \
	case INT8OID: \
		valref(int64,result,i) = \
		valsquare(valref(int64,left,i));\
		break; \
}

#define apply_cube_sdata(result,left,i) \
	switch ((left)->type_of_data) \
{ \
	case FLOAT4OID: \
		valref(float,result,i) = \
		valcube(valref(float,left,i)); \
		break; \
	case FLOAT8OID: \
		valref(float8,result,i) = \
		valcube(valref(float8,left,i));\
		break; \
	case CHAROID: \
		valref(char,result,i) = \
		valcube(valref(char,left,i));\
		break; \
	case INT2OID: \
		valref(int16_t,result,i) = \
		valcube(valref(int16_t,left,i));\
		break; \
	case INT4OID: \
		valref(int32_t,result,i) = \
		valcube(valref(int32_t,left,i));\
		break; \
	case INT8OID: \
		valref(int64,result,i) = \
		valcube(valref(int64,left,i));\
		break; \
}
#define apply_quad_sdata(result,left,i) \
	switch ((left)->type_of_data) \
{ \
	case FLOAT4OID: \
		valref(float,result,i) = \
		valquad(valref(float,left,i)); \
		break; \
	case FLOAT8OID: \
		valref(float8,result,i) = \
		valquad(valref(float8,left,i));\
		break; \
	case CHAROID: \
		valref(char,result,i) = \
		valquad(valref(char,left,i));\
		break; \
	case INT2OID: \
		valref(int16_t,result,i) = \
		valquad(valref(int16_t,left,i));\
		break; \
	case INT4OID: \
		valref(int32_t,result,i) = \
		valquad(valref(int32_t,left,i));\
		break; \
	case INT8OID: \
		valref(int64,result,i) = \
		valquad(valref(int64,left,i));\
		break; \
}

/* Checks that two SparseData have the same dimension
 */
static inline void
check_sdata_dimensions(SparseData left, SparseData right)
{
	if (left->total_value_count != right->total_value_count)
	{
		ereport(ERROR, 
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 errmsg("dimensions of vectors must be the same")));
	}
}

enum operation_t { subtract, add, multiply, divide };

/* Do one of subtract, add, multiply, or divide depending on
 * the value of operation.
 */
static inline void op_sdata_by_scalar_inplace(enum operation_t operation,
		char *scalar, SparseData sdata, bool scalar_is_right)
{
	if (scalar_is_right) //scalar is on the right
	{
		for(int i=0; i<sdata->unique_value_count; i++)
		{
			switch(operation)
			{
			case subtract:
				apply_const_to_sdata(sdata,i,-=,scalar)
				break;
			case add:
				apply_const_to_sdata(sdata,i,+=,scalar)
				break;
			case multiply:
				apply_const_to_sdata(sdata,i,*=,scalar)
				break;
			case divide:
				apply_const_to_sdata(sdata,i,/=,scalar)
				break;
			}
		}
	} else { //scalar is on the left
		for(int i=0; i<sdata->unique_value_count; i++)
		{
			switch(operation)
			{
			case subtract:
				apply_scalar_left_to_sdata(sdata,i,-,scalar)
				break;
			case add:
				apply_scalar_left_to_sdata(sdata,i,+,scalar)
				break;
			case multiply:
				apply_scalar_left_to_sdata(sdata,i,*,scalar)
				break;
			case divide:
				apply_scalar_left_to_sdata(sdata,i,/,scalar)
				break;
			}
		}
	}

}
static inline SparseData op_sdata_by_scalar_copy(enum operation_t operation,
		char *scalar, SparseData source_sdata, bool scalar_is_right) 
{
	SparseData sdata = makeSparseDataCopy(source_sdata);
	op_sdata_by_scalar_inplace(operation,scalar,sdata,scalar_is_right);
	return sdata;
}

/* Exponentiates an sdata left argument with a right scalar
 */
static inline SparseData pow_sdata_by_scalar(SparseData sdata,
		char *scalar)
{
	SparseData result = makeSparseDataCopy(sdata);
	for(int i=0; i<sdata->unique_value_count; i++)
		apply_function_sdata_scalar(result,pow,sdata,i,scalar)

	return(result);
}
static inline SparseData square_sdata(SparseData sdata)
{
	SparseData result = makeSparseDataCopy(sdata);
	for(int i=0; i<sdata->unique_value_count; i++)
		apply_square_sdata(result,sdata,i)

	return(result);
}
static inline SparseData cube_sdata(SparseData sdata)
{
	SparseData result = makeSparseDataCopy(sdata);
	for(int i=0; i<sdata->unique_value_count; i++)
		apply_cube_sdata(result,sdata,i)

	return(result);
}
static inline SparseData quad_sdata(SparseData sdata)
{
	SparseData result = makeSparseDataCopy(sdata);
	for(int i=0; i<sdata->unique_value_count; i++)
		apply_quad_sdata(result,sdata,i)

	return(result);
}

/* Checks the equality of two SparseData. We can't assume that two 
 * SparseData are in canonical form.
 *
 * The algorithm is simple: we traverse the left SparseData element by 
 * element, and for each such element x, we traverse all the elements of 
 * the right SparseData that overlaps with x and check that they are equal.
 *
 * Note: This function only works on SparseData of float8s at present.
 */   
static inline bool sparsedata_eq(SparseData left, SparseData right)
{
	if (left->total_value_count != right->total_value_count)
		return false;

	char * ix = left->index->data;	
	double * vals = (double *)left->vals->data;

	char * rix = right->index->data;
	double * rvals = (double *)right->vals->data;

	int read = 0, rread = 0;
	int rvid = 0;
	int rrun_length, i;

	for (i=0; i<left->unique_value_count; i++,ix+=int8compstoragesize(ix)) {
		read += compword_to_int8(ix);

		while (true) {
			/* 
			 * We need to use memcmp to handle NULLs (represented
			 * as NaNs) properly
			 */
			if (memcmp(&(vals[i]),&(rvals[rvid]),sizeof(float8))!=0)
				return false;
	
			/* 
			 * We never move the right element pointer beyond
			 * the current left element 
			 */
			rrun_length = compword_to_int8(rix);
			if (rread + rrun_length > read) break;

			/* 
			 * Increase counters if there are more elements in 
			 * the right SparseData that overlaps with current
			 * left element 
			 */ 
			rread += rrun_length;
			if (rvid < right->unique_value_count) {
				rix += int8compstoragesize(rix);
				rvid++;
			}
			if (rread == read) break;
		}
	}
	Assert(rread == read);
	return true;
}

/* Checks the equality of two SparseData. We can't assume that two 
 * SparseData are in canonical form.
 *
 * The algorithm is simple: we traverse the left SparseData element by 
 * element, and for each such element x, we traverse all the elements of 
 * the right SparseData that overlaps with x and check that they are equal.
 *
 * Unlike sparsedata_eq, this function assumes that any zero represents a 
 * mission data and hence still implyies equality
 *
 * Note: This function only works on SparseData of float8s at present.
 */ 
static inline bool sparsedata_eq_zero_is_equal(SparseData left, SparseData right)
{
	char * ix = left->index->data;	
	double* vals = (double *)left->vals->data;
	
	char * rix = right->index->data;
	double* rvals = (double *)right->vals->data;
	
	int read = 0, rread = 0;
	int i=-1, j=-1, minimum = 0;
	minimum = (left->total_value_count > right->total_value_count)?right->total_value_count:left->total_value_count;
	
	for (;(read < minimum)||(rread < minimum);) {
		if(read < rread){
			read += (int)compword_to_int8(ix);
			ix +=int8compstoragesize(ix);
			i++;
			if ((memcmp(&(vals[i]),&(rvals[j]),sizeof(float8))!=0)&&(vals[i]!=0.0)&&(rvals[j]!=0.0)){
				return false;
			}
		}else if(read > rread){
			rread += (int)compword_to_int8(rix);
			rix+=int8compstoragesize(rix);
			j++;
			if ((memcmp(&(vals[i]),&(rvals[j]),sizeof(float8))!=0)&&(vals[i]!=0.0)&&(rvals[j]!=0.0)){
				return false;
			}
		}else{
			read += (int)compword_to_int8(ix);
			rread += (int)compword_to_int8(rix);
			ix +=int8compstoragesize(ix);
			rix+=int8compstoragesize(rix);
			i++;
			j++;
			if ((memcmp(&(vals[i]),&(rvals[j]),sizeof(float8))!=0)&&(vals[i]!=0.0)&&(rvals[j]!=0.0)){
				return false;
			}
		}		
	}
	/*sprintf(result, "result after %d %f", j, rvals[j]);
	 ereport(NOTICE, 
	 (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
	 errmsg(result)));*/
	return true;
}

/* Checks if one SparseData object contained in another 
 *
 * First vector is said to contain second if all non-zero elements 
 * of the second data object equal those of the first one
 *
 * Note: This function only works on SparseData of float8s at present.
 */ 
static inline bool sparsedata_contains(SparseData left, SparseData right)
{
	char * ix = left->index->data;	
	double* vals = (double *)left->vals->data;
	
	char * rix = right->index->data;
	double* rvals = (double *)right->vals->data;
	
	int read = 0, rread = 0;
	int i=-1, j=-1, minimum = 0;
	int lsize, rsize;
	lsize = left->total_value_count;
	rsize = right->total_value_count;
	if((rsize > lsize)&&(rvals[right->unique_value_count-1]!=0.0)){
		return false;
	}
	
	minimum = (lsize > rsize)?rsize:lsize;
	
	for (;(read < minimum)||(rread < minimum);) {
		if(read < rread){
			read += (int)compword_to_int8(ix);
			ix +=int8compstoragesize(ix);
			i++;
			if ((memcmp(&(vals[i]),&(rvals[j]),sizeof(float8))!=0)&&(rvals[j]!=0.0)){
				return false;
			}
		}else if(read > rread){
			rread += (int)compword_to_int8(rix);
			rix+=int8compstoragesize(rix);
			j++;
			if ((memcmp(&(vals[i]),&(rvals[j]),sizeof(float8))!=0)&&(rvals[j]!=0.0)){
				return false;
			}
		}else{
			read += (int)compword_to_int8(ix);
			rread += (int)compword_to_int8(rix);
			ix +=int8compstoragesize(ix);
			rix+=int8compstoragesize(rix);
			i++;
			j++;
			if ((memcmp(&(vals[i]),&(rvals[j]),sizeof(float8))!=0)&&(rvals[j]!=0.0)){
				return false;
			}
		}		
	}
	return true;
}


static inline double id(double x) { return x; }
static inline double square(double x) { return x*x; }
static inline double myabs(double x) { return (x < 0) ? -(x) : x ; }

/* This function is introduced to capture a common routine for 
 * traversing a SparseData, transforming each element as we go along and 
 * summing up the transformed elements. The method is non-destructive to 
 * the input SparseData.
 */
static inline double 
accum_sdata_values_double(SparseData sdata, double (*func)(double)) 
{
	double accum=0.;
	char *ix = sdata->index->data;
	double *vals = (double *)sdata->vals->data;
	int64 run_length;

	for (int i=0;i<sdata->unique_value_count;i++)
	{
		run_length = compword_to_int8(ix);
		accum += func(vals[i])*run_length;
		ix+=int8compstoragesize(ix);
	}
	return (accum);
}

/* Computes the running sum of the elements of a SparseData */
static inline double sum_sdata_values_double(SparseData sdata) {
	return accum_sdata_values_double(sdata, id);
}

/* Computes the l2 norm of a SparseData */
static inline double l2norm_sdata_values_double(SparseData sdata) {
	return sqrt(accum_sdata_values_double(sdata, square));
}

/* Computes the l1 norm of a SparseData */
static inline double l1norm_sdata_values_double(SparseData sdata) {
	return accum_sdata_values_double(sdata, myabs);
}

/* 
 * Addition, Scalar Product, Division between SparseData arrays
 *
 * There are a few factors to consider:
 * - The dimension of the left and right arguments must be the same
 * - We employ an algorithm that does the computation on the compressed contents
 *   which creates a new SparseData array
 *------------------------------------------------------------------------------
 */
static inline SparseData op_sdata_by_sdata(enum operation_t operation,
					   SparseData left, SparseData right)
{
	SparseData sdata = makeSparseData();

	/*
	 * Loop over the contents of the left array, operating on elements
	 * of the right array and append a new value to the sdata when a
	 * new unique value is generated.
	 *
	 * We will manage two cursors, one for each of left and right arrays
	 */
	char *liptr=left->index->data;
	char *riptr=right->index->data;
	int left_run_length, right_run_length;
	char *new_value,*last_new_value;
	int tot_run_length=-1;
	left_run_length = compword_to_int8(liptr);
	right_run_length = compword_to_int8(riptr);
	int left_lst=0,right_lst=0;
	int left_nxt=left_run_length,right_nxt=right_run_length;
	int nextpos = Min(left_nxt,right_nxt),lastpos=0;
	int i=0,j=0;
	size_t width = size_of_type(left->type_of_data);

	check_sdata_dimensions(left,right);

	new_value      = (char *)palloc(width);
	last_new_value = (char *)palloc(width);

	while (1)
	{
		switch (operation)
		{
			case subtract:
				accum_sdata_result(new_value,left,i,-,right,j)
				break;
			case add:
			default:
				accum_sdata_result(new_value,left,i,+,right,j)
				break;
			case multiply:
				accum_sdata_result(new_value,left,i,*,right,j)
				break;
			case divide:
				accum_sdata_result(new_value,left,i,/,right,j)
				break;
		}
		/*
		 * Potentially add a new run, depending on whether this is a 
		 * different value from the previous calculation. It may be 
		 * that this calculation has produced an identical value to 
		 * the previous, in which case we store it up, waiting for a 
		 * new value to happen.
		 */
		if (tot_run_length==-1)
		{
			memcpy(last_new_value,new_value,width);
			tot_run_length=0;
		}
		if (memcmp(new_value,last_new_value,width))
		{
			add_run_to_sdata(last_new_value,tot_run_length,sizeof(float8),sdata);
			tot_run_length = 0;
			memcpy(last_new_value,new_value,width);
		}
		tot_run_length += (nextpos-lastpos);

		if ((left_nxt==right_nxt)&&(left_nxt==(left->total_value_count))) {
			break;
		} else if (left_nxt==right_nxt) {
			i++;j++;
			left_lst=left_nxt;right_lst=right_nxt;
			liptr+=int8compstoragesize(liptr);
			riptr+=int8compstoragesize(riptr);
		} else if (nextpos==left_nxt) {
			i++;
			left_lst=left_nxt;
			liptr+=int8compstoragesize(liptr);
		} else if (nextpos==right_nxt) {
			j++;
			right_lst=right_nxt;
			riptr+=int8compstoragesize(riptr);
		}
		left_run_length = compword_to_int8(liptr);
		right_run_length = compword_to_int8(riptr);
		left_nxt=left_run_length+left_lst;
		right_nxt=right_run_length+right_lst;
		lastpos=nextpos;
		nextpos = Min(left_nxt,right_nxt);
	}

	/*
	 * Add the last run if we ended with a repeating value
	 */
	if (tot_run_length!=0)
		add_run_to_sdata(new_value,tot_run_length,sizeof(float8),sdata);

	/*
	 * Set the return data type
	 */
	sdata->type_of_data = left->type_of_data;

	pfree(new_value);
	pfree(last_new_value);

	return sdata;
}

/*------------------------------------------------------------------------------
 * macros that will test whether a given double value is in the normal 
 * range or is in the special range (denormals, exceptions).
 *------------------------------------------------------------------------------
 */
/* Anything between LOW and HIGH is a denormal or exception */
#define SPEC_MASK_HIGH 0xFFF0000000000000
#define SPEC_MASK_LOW  0x7FF0000000000000
#define MASKTEST(x,y)	(((x)&(y))==x) /* MASKTEST checks for the presence of
					  the bits in x in the input y */

/* The input to MASKTEST_double should be an int64 mask and a (double *) to be
 * tested
 */
#define MASKTEST_double(x,y)	MASKTEST((x),*((int64 *)(&(y))))

#define DBL_IS_A_SPECIAL(x) \
  (MASKTEST_double(SPEC_MASK_HIGH,(x)) || MASKTEST_double(SPEC_MASK_LOW,(x)) \
   || ((x) == 0.))

#endif	/* SPARSEDATA_H */
