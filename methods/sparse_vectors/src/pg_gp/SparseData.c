/**
 * @file
 * \brief This file implements different operations on the SparseData structure.
 *
 * Most functions on sparse vectors are first defined on SparseData, and
 * then packaged up as functions on sparse vectors using wrappers. 
 *
 * This is the general procedure for adding functionalities to sparse vectors:
 *  1. Write the function for SparseData.
 *  2. Wrap it up for SvecType in operators.c or sparse_vector.c.
 *  3. Make the function available in gp_svec.sql.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include "SparseData.h"
#include "utils/builtins.h"
#include "utils/syscache.h"
#include "parser/parse_func.h"
#include "access/htup.h"
#include "catalog/pg_proc.h"

/** 
 * Constructor for a SparseData structure - Allocates empty dynamic 
 * StringInfo of unknown initial sizes.
 */
SparseData makeSparseData(void) {
	/* Allocate the struct */
	SparseData sdata = (SparseData)palloc(sizeof(SparseDataStruct));

	/* Allocate the included elements */
	sdata->vals  = makeStringInfo();
	sdata->index = makeStringInfo();
	sdata->vals->len = 0;
	sdata->index->len = 0;
	sdata->vals->cursor = 0;
	sdata->index->cursor = 0;
	sdata->unique_value_count=0;
	sdata->total_value_count=0;
	sdata->type_of_data = FLOAT8OID;
	return sdata;
}

/** 
 * Constructor for a SparseData structure - Creates a SparseData with 
 * zero storage in its StringInfos.
 */
SparseData makeEmptySparseData(void) {
	/* Allocate the struct */
	SparseData sdata = makeSparseData();

	/* Free the data area */
	pfree(sdata->vals->data);
	pfree(sdata->index->data);
	sdata->vals->data  = palloc(1);
	sdata->index->data = palloc(1);
	sdata->vals->maxlen=0;
	sdata->index->maxlen=0;
	return sdata;
}

/**
 * Constructor for a SparseData structure - Creates a SparseData in place 
 * using pointers to the vals and index data
 */
SparseData makeInplaceSparseData(char *vals, char *index,
		int datasize, int indexsize, Oid datatype,
		int unique_value_count, int total_value_count) {
	SparseData sdata = makeEmptySparseData();
	sdata->unique_value_count = unique_value_count;
	sdata->total_value_count  = total_value_count;
	sdata->vals->data = vals;
	sdata->vals->len = datasize;
	sdata->vals->maxlen = sdata->vals->len;
	sdata->index->data = index;
	sdata->index->len = indexsize;
	sdata->index->maxlen = sdata->index->len;
	sdata->type_of_data = datatype;
	return sdata;
}

/**
 * Constructor for a SparseData structure - Creates a copy of an existing 
 * SparseData. 
 */
SparseData makeSparseDataCopy(SparseData source_sdata) {
	/* Allocate the struct */
	SparseData sdata = (SparseData)palloc(sizeof(SparseDataStruct));
	/* Allocate the included elements */
	sdata->vals = copyStringInfo(source_sdata->vals);
	sdata->index = copyStringInfo(source_sdata->index);
	sdata->type_of_data       = source_sdata->type_of_data;
	sdata->unique_value_count = source_sdata->unique_value_count;
	sdata->total_value_count  = source_sdata->total_value_count;
	return sdata;
}
/**
 * Creates a SparseData of size dimension from a constant
 */
SparseData makeSparseDataFromDouble(double constant,int64 dimension) {
	char *bytestore=(char *)palloc(sizeof(char)*9);
	SparseData sdata = float8arr_to_sdata(&constant,1);
	int8_to_compword(dimension,bytestore); /* create compressed version of
					          int8 value */
	pfree(sdata->index->data);
	sdata->index->data = bytestore;
	sdata->index->len = int8compstoragesize(bytestore);
	sdata->total_value_count=dimension;
	if (sdata->index->maxlen < int8compstoragesize(bytestore)) {
		ereport(ERROR,(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			       errOmitLocation(true),
			       errmsg("Internal error")));
	}
	return sdata;
}

/**
 * Destructor for SparseData
 */
void freeSparseData(SparseData sdata) {
	pfree(sdata->vals);
	pfree(sdata->index);
	pfree(sdata);
}

/** 
 * Destructor for SparseData
 */
void freeSparseDataAndData(SparseData sdata) {
	pfree(sdata->vals->data);
	pfree(sdata->index->data);
	freeSparseData(sdata);
}

/**
 * Copies a StringInfo structure
 */
StringInfo copyStringInfo(StringInfo sinfo) {
	StringInfo result;
	char *data;
	if (sinfo->data == NULL) {
		data = NULL;
	} else {
		data   = (char *)palloc(sizeof(char)*(sinfo->len+1));
		memcpy(data,sinfo->data,sinfo->len);
		data[sinfo->len] = '\0';
	}
	result = makeStringInfoFromData(data,sinfo->len);
	return result;
}

/**
 * Makes a StringInfo from a data pointer and length
 */
StringInfo makeStringInfoFromData(char *data,int len) {
	StringInfo sinfo;
	sinfo = (StringInfo)palloc(sizeof(StringInfoData));
	sinfo->data   = data;
	sinfo->len    = len;
	sinfo->maxlen = len;
	sinfo->cursor = 0;
	return sinfo;
}

/**
 * Converts a double[] to a SparseData compressed format, representing duplicate
 * values in the double[] by a count and a single value.
 * Note that special double values like denormalized numbers and exceptions
 * like NaN are treated like any other value - if there are duplicates, the
 * value of the special number is preserved and they are counted.
 *
 * @param array The array of doubles to be converted to a SparseData
 * @param count The size of array
 */
SparseData float8arr_to_sdata(double *array, int count) {
	return arr_to_sdata((char *)array, sizeof(float8), FLOAT8OID, count);
}

/**
 * Converts an array (of arbitrary base type) to a compressed SparseData.
 */
SparseData arr_to_sdata(char *array, size_t width, Oid type_of_data, int count){
	char *run_val=array;
	int64 run_len=1;
	SparseData sdata = makeSparseData();

	sdata->type_of_data=type_of_data;

	for (int i=1; i<count; i++) {
		char *curr_val=array+ (i*size_of_type(type_of_data));
			if (memcmp(curr_val,run_val,width))
			{ /*run is interrupted, initiate new run */
				/* package up the finished run */
				add_run_to_sdata(run_val,run_len,width,sdata);
				/* mark beginning of new run */
				run_val=curr_val;
				run_len=1;
			} else 
			{ /* we're still in the same run */
				run_len++;
			}
	}
	add_run_to_sdata(run_val, run_len, width, sdata); /* package up the last run */

	/* Add the final tallies */
	sdata->unique_value_count = sdata->vals->len/width;
	sdata->total_value_count = count;

	return sdata;
}

/**
 * Converts a SparseData compressed structure to a float8[]
 */
double *sdata_to_float8arr(SparseData sdata) {
	double *array;
	int j, aptr;
	char *iptr;

	if (sdata->type_of_data != FLOAT8OID) {
		ereport(ERROR,(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			errOmitLocation(true),
			errmsg("Data type of SparseData is not FLOAT64\n")));
	}

	if ((array=(double *)palloc(sizeof(double)*(sdata->total_value_count)))
	      == NULL)
	{
		ereport(ERROR,(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			       errOmitLocation(true),
			       errmsg("Error allocating memory for array\n")));
	}

	iptr = sdata->index->data;
	aptr = 0;
	for (int i=0; i<sdata->unique_value_count; i++) {
		for (j=0;j<compword_to_int8(iptr);j++,aptr++) {
			array[aptr] = ((double *)(sdata->vals->data))[i];
		}
		iptr+=int8compstoragesize(iptr);
	}

	if ((aptr) != sdata->total_value_count) 
	{
		ereport(ERROR,(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
		  errOmitLocation(true),
		  errmsg("Array size is incorrect, is: %d and should be %d\n",
				aptr,sdata->total_value_count)));

		pfree(array);
		return NULL;
	}

	return array;
}

/**
 * Converts the count array of a SparseData into an array of integers
 */
int64 *sdata_index_to_int64arr(SparseData sdata) {
	char *iptr;
	int64 *array_ix = (int64 *)palloc(
			sizeof(int64)*(sdata->unique_value_count));

	iptr = sdata->index->data;
	for (int i=0; i<sdata->unique_value_count; i++,
			iptr+=int8compstoragesize(iptr)) {
		array_ix[i] = compword_to_int8(iptr);
	}
	return(array_ix);
}

/**
 * Serializes a SparseData compressed structure
 */
void serializeSparseData(char *target, SparseData source)
{
	/* SparseDataStruct header */
	memcpy(target,source,SIZEOF_SPARSEDATAHDR);
	/* Two StringInfo structures describing the data and index */
	memcpy(SDATA_DATA_SINFO(target), source->vals,sizeof(StringInfoData));
	memcpy(SDATA_INDEX_SINFO(target),source->index,sizeof(StringInfoData));
	/* The unique data values */
	memcpy(SDATA_VALS_PTR(target),source->vals->data,source->vals->maxlen);
	/* The index values */
	memcpy(SDATA_INDEX_PTR(target),source->index->data,source->index->maxlen);

	/*
	 * Set pointers to the data areas of the serialized structure
	 * First the two StringInfo structures contained in the SparseData,
	 * then the data areas inside each of the two StringInfos.
	 */
	((SparseData)target)->vals = (StringInfo)SDATA_DATA_SINFO(target);
	((SparseData)target)->index = (StringInfo)SDATA_INDEX_SINFO(target);
	((StringInfo)(SDATA_DATA_SINFO(target)))->data = SDATA_VALS_PTR(target);
	if (source->index->data != NULL)
	{
		((StringInfo)(SDATA_INDEX_SINFO(target)))->data = SDATA_INDEX_PTR(target);
	} else {
		((StringInfo)(SDATA_INDEX_SINFO(target)))->data = NULL;
	}
}

/**
 * Prints a SparseData
 */
void printSparseData(SparseData sdata) {
	int value_count = sdata->unique_value_count;
	{
		char *indexdata = sdata->index->data;
		double *values  = (double *)(sdata->vals->data);
		for (int i=0; i<value_count; i++) {
			printf("run_length[%d] = %lld, ",i,(long long int)compword_to_int8(indexdata));
			printf("value[%d] = %f\n",i,values[i]);
			indexdata+=int8compstoragesize(indexdata);
		}
	}
}

/**
 * Projects onto an element of a SparseData. As is normal in 
 * SQL, we start counting from one. 
 */
double sd_proj(SparseData sdata, int idx) {
	char * ix = sdata->index->data;
	double * vals = (double *)sdata->vals->data;
	float8 ret;
	int read, i;

	/* error checking */
	if (0 >= idx || idx > sdata->total_value_count)
		ereport(ERROR, 
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 errOmitLocation(true),
			 errmsg("Index out of bounds.")));

	/* find desired block */
	read = compword_to_int8(ix);
	i = 0;
	while (read < idx) {
		ix += int8compstoragesize(ix);
		read += compword_to_int8(ix);
		i++;
	}
	return vals[i];
}

/**
 * Extracts a sub-array, indexed by start and end, of a SparseData. 
 */
SparseData subarr(SparseData sdata, int start, int end) {
	char * ix = sdata->index->data;
	double * vals = (double *)sdata->vals->data;
	SparseData ret = makeSparseData();
	size_t wf8 = sizeof(float8);
	
	if (start > end) 
		return reverse(subarr(sdata,end,start));

	/* error checking */
	if (0 >= start || start > end || end > sdata->total_value_count)
		ereport(ERROR, 
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 errOmitLocation(true),
			 errmsg("Array index out of bounds.")));

	/* find start block */
	int read = compword_to_int8(ix);
	int i = 0;
	while (read < start) {
		ix += int8compstoragesize(ix);
		read += compword_to_int8(ix);
		i++;
	}
	if (end <= read) {
		/* the whole subarray is in the first block, we are done */
		add_run_to_sdata((char *)(&vals[i]), end-start+1, wf8, ret);
		return ret;
	}
	/* else start building subarray */
	add_run_to_sdata((char *)(&vals[i]), read-start+1, wf8, ret);

	for (int j=i+1; j<sdata->unique_value_count; j++) {
		ix += int8compstoragesize(ix);
		int esize = compword_to_int8(ix);
		if (read + esize > end) {
			add_run_to_sdata((char *)(&vals[j]), end-read, wf8,ret);
			break;
		} 
		add_run_to_sdata((char *)(&vals[j]), esize, wf8, ret);
		read += esize;
		if (read == end) break;
	}
	return ret;
}

/**
 * Returns a copy of a SparseData, with the order of the
 * elements reversed. The function is non-destructive to the input SparseData.
 */
SparseData reverse(SparseData sdata) {
	char * ix = sdata->index->data;
	double * vals = (double *)sdata->vals->data;
	SparseData ret = makeSparseData();
	size_t w = sizeof(float8);

	/* move to the last count array element */
	for (int j=0; j<sdata->unique_value_count-1; j++)
		ix += int8compstoragesize(ix);

	/* copy from right to left */
	for (int j=sdata->unique_value_count-1; j!=-1; j--) {
		add_run_to_sdata((char *)(&vals[j]),compword_to_int8(ix),w,ret);
		ix -= int8compstoragesize(ix);
	}
	return ret;
}

/** 
 * Returns the concatenation of two input SparseData.
 *  Copies of the input sparse data are made.
 */
SparseData concat(SparseData left, SparseData right) {
	if (left == NULL && right == NULL) {
		return NULL;
	} else if (left == NULL && right != NULL) {
		return makeSparseDataCopy(right);
	} else if (left != NULL && right == NULL) {
		return makeSparseDataCopy(left);
	}
	SparseData sdata = makeEmptySparseData();
	char *vals,*index;
	int l_val_len = left->vals->len;
	int r_val_len = right->vals->len;
	int l_ind_len = left->index->len;
	int r_ind_len = right->index->len;
	int val_len=l_val_len+r_val_len;
	int ind_len=l_ind_len+r_ind_len;
	
	vals = (char *)palloc(sizeof(char)*val_len);
	index = (char *)palloc(sizeof(char)*ind_len);
	
	memcpy(vals          ,left->vals->data,l_val_len);
	memcpy(vals+l_val_len,right->vals->data,r_val_len);
	memcpy(index,          left->index->data,l_ind_len);
	memcpy(index+l_ind_len,right->index->data,r_ind_len);
	
	sdata->vals  = makeStringInfoFromData(vals,val_len);
	sdata->index = makeStringInfoFromData(index,ind_len);
	sdata->type_of_data = left->type_of_data;
	sdata->unique_value_count = left->unique_value_count+
		right->unique_value_count;
	sdata->total_value_count  = left->total_value_count+
		right->total_value_count;
	return sdata;
}

static bool lapply_error_checking(Oid foid, List * funcname);

/**
 * This function applies an input function on all elements of a sparse data. 
 * The function is modelled after the corresponding function in R.
 *
 * @param func The name of the function to apply
 * @param sdata The input sparse data to be modified
 * @see lapply_error_checking()
 */
SparseData lapply(text * func, SparseData sdata) {
	Oid argtypes[1] = { FLOAT8OID };
	List * funcname = textToQualifiedNameList(func);
	SparseData result = makeSparseDataCopy(sdata);
	Oid foid = LookupFuncName(funcname, 1, argtypes, false);

	lapply_error_checking(foid, funcname);

	for (int i=0; i<sdata->unique_value_count; i++)
		valref(float8,result,i) = 
		    DatumGetFloat8(
		      OidFunctionCall1(foid,
				    Float8GetDatum(valref(float8,sdata,i))));
	return result;
}

/* This function checks for error conditions in lapply() function calls.
 */
static bool lapply_error_checking(Oid foid, List * func) {
	/* foid != InvalidOid; otherwise LookupFuncName would raise error.
	   Here we check that the return type of foid is float8. */
	HeapTuple ftup = SearchSysCache(PROCOID, 
					ObjectIdGetDatum(foid), 0, 0, 0);
	Form_pg_proc pform = (Form_pg_proc) GETSTRUCT(ftup);

	if (pform->prorettype != FLOAT8OID)
		ereport(ERROR, 
			(errcode(ERRCODE_DATATYPE_MISMATCH),
			 errmsg("return type of %s is not double", 
				NameListToString(func)))); 

	// check volatility

	ReleaseSysCache(ftup);
	return true;
}




