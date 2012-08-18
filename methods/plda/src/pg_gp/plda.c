/* ----------------------------------------------------------------------- *//** 
 *
 * @file plda.c
 *
 * @brief Support functions for Parallel Latent Dirichlet Allocation
 * @author Kee Siong Ng
 *
 * This file is a subsidiary file to plda.sql, where we implement a few important
 * UDFs in PL/C for efficiency reasons.
 *
 * Word-topic counts, which are 2 dimensional matrices, are implemented as a 
 * one dimensional array using Postgres's internal ArrayType data structure.
 *
 *//* ----------------------------------------------------------------------- */

#include "postgres.h"
#include "funcapi.h"
#include "fmgr.h"
#include "catalog/pg_type.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "executor/executor.h"
#include <stdlib.h>
#include <assert.h>

#ifndef NO_PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif
/* Indicate "version 1" calling conventions for all exported functions. */
PG_FUNCTION_INFO_V1(sampleNewTopics);
PG_FUNCTION_INFO_V1(randomTopics);
PG_FUNCTION_INFO_V1(zero_array);
PG_FUNCTION_INFO_V1(sum_int4array);
PG_FUNCTION_INFO_V1(cword_count);

/**
 * Returns an array of a given length filled with zeros
 *
 * This function can be further optimised, since we're effectively allocating
 * the desired array twice, once in palloc0() and once in construct_array().
 */
Datum zero_array(PG_FUNCTION_ARGS);
Datum zero_array(PG_FUNCTION_ARGS)
{
	int32 len = PG_GETARG_INT32(0);
	Datum * array = palloc0(len * sizeof(Datum));
	ArrayType * pgarray = construct_array(array, len, INT4OID, 4, true, 'i');
	PG_RETURN_ARRAYTYPE_P(pgarray);
}

/**
 * Returns the element-wise sum of two arrays.
 *
 * First argument can be NULL; second argument can't.
 */
Datum sum_int4array(PG_FUNCTION_ARGS);
Datum sum_int4array(PG_FUNCTION_ARGS)
{
	Datum * array;
	ArrayType * arr0, * arr1, * ret;
	int32 * arr0_v, * arr1_v, * ret_v;
	int dim;

	if (PG_ARGISNULL(1)) 
		ereport
		 (ERROR,
		  (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
		   errmsg("function \"%s\" called with NULL second argument",
			  format_procedure(fcinfo->flinfo->fn_oid))));

	/* Get second argument */
	arr1 = PG_GETARG_ARRAYTYPE_P(1);

	if (ARR_NDIM(arr1) != 1 || ARR_ELEMTYPE(arr1) != INT4OID)
		ereport
		 (ERROR,
		  (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
		   errmsg("function \"%s\" called with invalid parameters",
			  format_procedure(fcinfo->flinfo->fn_oid))));

	/* We allow first argument to be null, in which case we return 
	   second (non-null) argument */
	if (PG_ARGISNULL(0)) 
		PG_RETURN_ARRAYTYPE_P(arr1);

	/* Get first argument */
	arr0 = PG_GETARG_ARRAYTYPE_P(0);
	if (ARR_NDIM(arr0) != 1 || ARR_ELEMTYPE(arr0) != INT4OID ||
	    ARR_DIMS(arr1)[0] != ARR_DIMS(arr0)[0])
		ereport
		 (ERROR,
		  (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
		   errmsg("function \"%s\" called with invalid parameters",
			  format_procedure(fcinfo->flinfo->fn_oid))));

	/* Do appropriate casts */
	dim = ARR_DIMS(arr1)[0];
	arr1_v = (int32 *)ARR_DATA_PTR(arr1);
	arr0_v = (int32 *)ARR_DATA_PTR(arr0);

	/* Allocate memory for return value */
	array = palloc0(dim*sizeof(Datum));
	ret = construct_array(array,dim,INT4OID,4,true,'i');
	ret_v = (int32 *)ARR_DATA_PTR(ret);

	for (int i=0; i!=dim; i++)
		ret_v[i] = arr0_v[i] + arr1_v[i];
	
	PG_RETURN_ARRAYTYPE_P(ret);
}

/**
 * This function updates the word-topic count array given the assignment of
 * topics to words in a document.
 *
 * Note: The function modifies the input word-topic count array, and can only 
 * be used as part of the cword_agg() function.
 */
Datum cword_count(PG_FUNCTION_ARGS);
Datum cword_count(PG_FUNCTION_ARGS)
{
	ArrayType * count_arr, * doc_arr, * topics_arr;
	int32 * count, * doc, * topics;
	int32 doclen, num_topics, dsize, i;
	Datum * array;
	int32 idx;

	if (!(fcinfo->context && IsA(fcinfo->context, AggState)))
		elog(ERROR, "cword_count not used as part of an aggregate");

	doclen = PG_GETARG_INT32(3);
	num_topics = PG_GETARG_INT32(4);
	dsize = PG_GETARG_INT32(5);

	/* Construct a zero'd array at the first call of this function */
	if (PG_ARGISNULL(0)) {
		array = palloc0(dsize*num_topics*sizeof(Datum));
		count_arr =
		    construct_array(array,dsize*num_topics,INT4OID,4,true,'i');
	} else {
		count_arr = PG_GETARG_ARRAYTYPE_P(0);
	}
	doc_arr = PG_GETARG_ARRAYTYPE_P(1);
	topics_arr = PG_GETARG_ARRAYTYPE_P(2);

	/* Check that the input arrays are of the right dimension and type */
	if (ARR_NDIM(count_arr) != 1 || ARR_ELEMTYPE(count_arr) != INT4OID ||
	    ARR_NDIM(doc_arr) != 1 || ARR_ELEMTYPE(doc_arr) != INT4OID ||
	    ARR_NDIM(topics_arr) != 1 || ARR_ELEMTYPE(topics_arr) != INT4OID)
		ereport
		 (ERROR,
		  (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
		   errmsg("transition function \"%s\" called with invalid parameters",
			  format_procedure(fcinfo->flinfo->fn_oid))));

	count = (int32 *)ARR_DATA_PTR(count_arr);
	doc = (int32 *)ARR_DATA_PTR(doc_arr);
	topics = (int32 *)ARR_DATA_PTR(topics_arr);

	/* Update the word-topic count */
	for (i=0; i!=doclen; i++) {
		idx = (doc[i]-1) * num_topics + (topics[i]-1);

		if (idx < 0 || idx >= dsize*num_topics)
			ereport
			 (ERROR,
			  (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
		           errmsg("function \"%s\" called with invalid parameters",
				  format_procedure(fcinfo->flinfo->fn_oid))));
		
		count[idx]++;
	}
	PG_RETURN_ARRAYTYPE_P(count_arr);
}

/**
 * This function samples a new topic for a given word based on count statistics
 * computed on the rest of the corpus. This is the core function in the Gibbs
 * sampling inference algorithm for LDA. 
 * 
 * Parameters
 *  @param numtopics number of topics
 *  @param widx the index of the current word whose topic is to be sampled
 *  @param wtopic the current assigned topic of the word
 *  @param global_count the word-topic count matrix
 *  @param local_d the distribution of topics in the current document
 *  @param topic_counts the distribution of number of words in the corpus assigned to each topic
 *  @param alpha the Dirichlet parameter for the topic multinomial
 *  @param eta the Dirichlet parameter for the per-topic word multinomial
 *
 * The function is non-destructive to all the input arguments.
 */
static int32 sampleTopic
   (int32 numtopics, int32 widx, int32 wtopic, int32 * global_count,
    int32 * local_d, int32 * topic_counts, float8 alpha, float8 eta) 
{
	int32 j, glcount_temp, locald_temp, ret;
	float8 r, cl_prob, total_unpr;

	// this array captures the cumulative prob. distribution of the topics
	float8 * topic_prs = (float8 *)palloc(sizeof(float8) * numtopics); 

	/* make adjustment for 0-indexing */
	widx--;
	wtopic--;

	/* calculate topic (unnormalised) probabilities */
	total_unpr = 0;
	for (j=0; j!=numtopics; j++) {
		// number of times widx is assigned topic j in the corpus
		glcount_temp = global_count[widx * numtopics + j];
		// number of times a word is assigned topic j in this document
		locald_temp = local_d[j];
		// adjust the counts to exclude current word's contribution
		if (j == wtopic) {
			glcount_temp--;
			locald_temp--;
		}
		// the topic probability for current word, proportional to 
		//   fraction of times word is assigned topic j
		// x fraction of times a word is assigned topic j in current doc
		cl_prob = (locald_temp + alpha) * (glcount_temp + eta) /
			  (topic_counts[j] + numtopics * eta);
		total_unpr += cl_prob;
		topic_prs[j] = total_unpr;
	}
	/* normalise probabilities */
	for (j=0; j!=numtopics; j++)
		topic_prs[j] = topic_prs[j] / total_unpr;

	/* Draw a topic at random */
	r = drand48();
	ret = 1;
	while (true) {
		if (ret == numtopics || r < topic_prs[ret-1]) break;
		ret++; 
	}
	if (ret < 1 || ret > numtopics)
		elog(ERROR, "sampleTopic: ret = %d", ret);

	pfree(topic_prs);
	return ret;
}


/**
 * This function checks the validity of array parameters for "sampleNewTopics".
 *
 * Parameters
 *  @param p_array The array to be checked
 *  @param fn_oid The oid of the function to be checked
 *  @param array_name The name of the array
 *
 */
static void check_array_sampleNewTopics
	(ArrayType * p_array, Oid fn_oid, const char * array_name)
{
	if (ARR_HASNULL(p_array))
	{
		ereport(ERROR,
		       (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			errmsg("In function \"%s\", the %s should not contain null "
			       "elements",
			       format_procedure(fn_oid),
			       array_name)));
	}

	if (ARR_NDIM(p_array) != 1)
	{
		ereport(ERROR,
		       (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			errmsg("In function \"%s\", the dimension of %s should be 1",
			       format_procedure(fn_oid),
			       array_name)));
	}

	if (ARR_ELEMTYPE(p_array) != INT4OID)
	{
		ereport(ERROR,
		       (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			errmsg("In function \"%s\", the element type in the %s should "
			       "be INT4",
			       format_procedure(fn_oid),
			       array_name)));
	}
}

/**
 * This function assigns a topic to each word in a document using the count
 * statistics obtained so far on the corpus. The function returns an array
 * of int4s, which pack two arrays together: the topic assignment to each
 * word in the document (the first len elements in the returned array), and
 * the number of words assigned to each topic (the last num_topics elements
 * of the returned array).
 */
Datum sampleNewTopics(PG_FUNCTION_ARGS);
Datum sampleNewTopics(PG_FUNCTION_ARGS)
{
	int32 i, widx, wtopic, rtopic;

	ArrayType * doc_arr = PG_GETARG_ARRAYTYPE_P(0);
	ArrayType * topics_arr = PG_GETARG_ARRAYTYPE_P(1);
	ArrayType * topic_d_arr = PG_GETARG_ARRAYTYPE_P(2);
	ArrayType * global_count_arr = PG_GETARG_ARRAYTYPE_P(3);
	ArrayType * topic_counts_arr = PG_GETARG_ARRAYTYPE_P(4);
	int32 num_topics = PG_GETARG_INT32(5);
	int32 dsize = PG_GETARG_INT32(6);
	float8 alpha = PG_GETARG_FLOAT8(7);
	float8 eta = PG_GETARG_FLOAT8(8);
	Oid fn_oid = fcinfo->flinfo->fn_oid;

	check_array_sampleNewTopics(doc_arr, fn_oid, "document array");
	check_array_sampleNewTopics(topics_arr, fn_oid, "topic array");
	check_array_sampleNewTopics(topic_d_arr, fn_oid, "topic distribution array");
	check_array_sampleNewTopics(global_count_arr, fn_oid, "global count array");
	check_array_sampleNewTopics(topic_counts_arr, fn_oid, "topic count array");

	// the document array
	int32 * doc = (int32 *)ARR_DATA_PTR(doc_arr);
	int32 len = ARR_DIMS(doc_arr)[0];

	// array giving topic assignment to each word in document
	int32 * topics = (int32 *)ARR_DATA_PTR(topics_arr);

	// distribution of topics in document
	int32 * topic_d = (int32 *)ARR_DATA_PTR(topic_d_arr);

	// the word-topic count matrix
	int32 * global_count = (int32 *)ARR_DATA_PTR(global_count_arr);

	// total number of words assigned to each topic in the whole corpus
	int32 * topic_counts = (int32 *)ARR_DATA_PTR(topic_counts_arr);

	ArrayType * ret_topics_arr, * ret_topic_d_arr;
	int32 * ret_topics, * ret_topic_d;

	Datum * arr1 = palloc0(len * sizeof(Datum));
	ret_topics_arr = construct_array(arr1,len,INT4OID,4,true,'i');
	ret_topics = (int32 *)ARR_DATA_PTR(ret_topics_arr);

	Datum * arr2 = palloc0(num_topics * sizeof(Datum));
	ret_topic_d_arr = construct_array(arr2,num_topics,INT4OID,4,true,'i');
	ret_topic_d = (int32 *)ARR_DATA_PTR(ret_topic_d_arr);

	for (i=0; i!=len; i++) {
		widx = doc[i];

		if (widx < 1 || widx > dsize)
		     ereport
		      (ERROR,
		       (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			errmsg("function \"%s\" called with invalid parameters. Word index is: %d. "
                    " Dictionary size is: %d. Word index should be in the range of [1, "
                    " dict_size]",
			       format_procedure(fcinfo->flinfo->fn_oid), widx, dsize)));

		wtopic = topics[i];
		rtopic = sampleTopic(num_topics,widx,wtopic,global_count,
				     topic_d,topic_counts,alpha,eta);

		// <sampleNewTopics error checking> 

		ret_topics[i] = rtopic;
		ret_topic_d[rtopic-1]++;
	}

	Datum values[2];
	values[0] = PointerGetDatum(ret_topics_arr);
	values[1] = PointerGetDatum(ret_topic_d_arr);

	TupleDesc tuple;
	if (get_call_result_type(fcinfo, NULL, &tuple) != TYPEFUNC_COMPOSITE)
		ereport(ERROR,
			(errcode( ERRCODE_FEATURE_NOT_SUPPORTED ),
			 errmsg( "function returning record called in context "
				 "that cannot accept type record" )));
	tuple = BlessTupleDesc(tuple);

	bool * isnulls = palloc0(2 * sizeof(bool));
	HeapTuple ret = heap_form_tuple(tuple, values, isnulls);

	if (isnulls[0] || isnulls[1])
		ereport(ERROR,
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 errmsg("function \"%s\" produced null results",
				format_procedure(fcinfo->flinfo->fn_oid))));

	PG_RETURN_DATUM(HeapTupleGetDatum(ret));
}
/*
 <sampleNewTopics error checking>
 if (rtopic < 1 || rtopic > num_topics || wtopic < 1 || wtopic > num_topics)
     elog(ERROR, "sampleNewTopics: rtopic = %d wtopic = %d", rtopic, wtopic);
 */

/**
 * This function returns an array of random topic assignments for a given
 * document length.
 */
Datum randomTopics(PG_FUNCTION_ARGS);
Datum randomTopics(PG_FUNCTION_ARGS)
{
	int32 doclen = PG_GETARG_INT32(0);
	int32 num_topics = PG_GETARG_INT32(1);

	ArrayType * ret_topics_arr, * ret_topic_d_arr;
	int32 * ret_topics, * ret_topic_d;

	Datum * arr1 = palloc0(doclen * sizeof(Datum));
	ret_topics_arr = construct_array(arr1,doclen,INT4OID,4,true,'i');
	ret_topics = (int32 *)ARR_DATA_PTR(ret_topics_arr);

	Datum * arr2 = palloc0(num_topics * sizeof(Datum));
	ret_topic_d_arr = construct_array(arr2,num_topics,INT4OID,4,true,'i');
	ret_topic_d = (int32 *)ARR_DATA_PTR(ret_topic_d_arr);

	/* Sample topics */
	int i, rtopic;
	for (i=0; i!=doclen; i++) {
		rtopic = random() % num_topics + 1;
		ret_topics[i] = rtopic;
		ret_topic_d[rtopic-1]++;
	}
	
	/* Package up the return arrays */
	Datum values[2];
	values[0] = PointerGetDatum(ret_topics_arr);
	values[1] = PointerGetDatum(ret_topic_d_arr);

	TupleDesc tuple;
	if (get_call_result_type(fcinfo, NULL, &tuple) != TYPEFUNC_COMPOSITE)
		ereport(ERROR,
			(errcode( ERRCODE_FEATURE_NOT_SUPPORTED ),
			 errmsg( "function returning record called in context "
				 "that cannot accept type record" )));
	tuple = BlessTupleDesc(tuple);

	bool * isnulls = palloc0(2 * sizeof(bool));
	HeapTuple ret = heap_form_tuple(tuple, values, isnulls);

	if (isnulls[0] || isnulls[1])
		ereport(ERROR,
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 errmsg("function \"%s\" produced null results",
				format_procedure(fcinfo->flinfo->fn_oid))));

	PG_RETURN_DATUM(HeapTupleGetDatum(ret));
}
