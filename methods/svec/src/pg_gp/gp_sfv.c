#include <postgres.h>

#include <stdio.h>
#include <string.h>
#include <search.h>
#include <stdlib.h>
#include <math.h>

#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "catalog/pg_type.h"
#include "access/tupmacs.h"

#include "sparse_vector.h"

static char **get_text_array_contents(ArrayType *array, int *numitems);

SvecType * classify_document(char **features, int num_features, 
			     char **document, int num_words);

Datum gp_extract_feature_histogram(PG_FUNCTION_ARGS);

void gp_extract_feature_histogram_errout(char *msg);

/**
 * 	gp_extract_feature_histogram
 * 	Approach:
 * 	  Definitions:
 * 	   Feature Vector:
 * 	    A feature vector is a list of words, generally all of the possible 
 *          choices of words. In other words, a feature vector is a dictionary 
 *          and might have cardinality of 20,000 or so.
 *
 * 	   Document:
 * 	    A document, here identifed using a list of words. Generally a 
 *          document will consist of a set of words contained in the feature 
 *          vector, but sometimes a document will contain words that are not 
 *          in the feature vector.
 *
 * 	   Sparse Feature Vector (SFV):
 * 	    An SFV is an array of attributes defined for each feature found 
 *          in a document. For example, you might define an SFV where each 
 *          attribute is a count of the number of instances of a feature is 
 *          found in the document, with one entry per feature found in the 
 *          document.
 *
 * 	Example:
 * 	  Say we have a document defined as follows:
 * 	    document1 = {"this","is","an","example","sentence","with","some",
 *                       "some","repeat","repeat"}
 * 	  And say we have a feature vector as follows:
 * 	    features = {"foo","bar","this","is","an","baz","example","sentence",
 *                      "with","some","repeat","word1","word2","word3"}
 *
 * 	  Now we'd like to create the SFV for document1. We can number each 
 *        feature starting at 1, so that feature(1) = foo, feature(2) = bar 
 *        and so on. The SFV of document1 would then be:
 * 	    sfv(document1,features) = {0,0,1,1,1,0,1,1,1,2,2,0,0,0}
 * 	  Note that the position in the SFV array is the number of the feature 
 *        vector and the attribute is the count of the number of features 
 *        found in each position.
 *
 *     We would like to store the SFV in a terse representation that fits 
 *     in a small amount of memory. We also want to be able to compare the 
 *     number of instances where the SFV of one document intersects another. 
 *     This routine uses the Sparse Vector datatype to store the SFV.
 *
 * Function Signature is:
 *
 * Where:
 * 	features:		a text array of features (words)
 *	document:		the document, tokenized into words
 *
 * Returns:
 * 	SFV of the document with counts of each feature, stored in a Sparse Vector (svec) datatype
 *
 * TODO:
 * 	Use the built-in hash table structure instead of hsearch()
 * 		The problem with hsearch is that it's not safe to use more than
 * 		one per process.  That means we currently can't do more than one document
 * 		classification per query slice or we'll get the wrong results.
 *	[DONE] Implement a better scheme for detecting whether we're in a new query since
 *	we created the hash table.
 *		Right now we write a key into palloc'ed memory and check to see
 *		if it's the same value on reentry to the classification routine.
 *		This is a hack and may fail in certain circumstances.
 *		A better approach uses the gp_session_id and gp_command_count
 *		to determine if we're in the same query as the last time we were
 *		called.
 */

/**
 * Notes from Brian Dolan on how this feature vector is commonly used:
 *
 * The actual count is hardly ever used.  Insead, it's turned into a weight.  The most
 * common weight is called tf/idf for "Term Frequency / Inverse Document Frequency".
 * The calculation for a given term in a given document is:
 * 	{#Times in this document} * log {#Documents / #Documents  the term appears in}
 * For instance, the term "document" in document A would have weight 1 * log (4/3).  In
 * document D it would have weight 2 * log (4/3).
 * Terms that appear in every document would have tf/idf weight 0, since:
 * 	log (4/4) = log(1) = 0.  (Our example has no term like that.) 
 * That usually sends a lot of values to 0.
 *
 * In this function we're just calculating the term:
 * 	log {#Documents / #Documents  the term appears in}
 * as an Svec.
 *
 * We'll need to have the following arguments:
 * 	Svec *count_corpus           //count of documents in which each feature is found
 * 	float8 *document_count      //count of all documents in corpus
 */

PG_FUNCTION_INFO_V1( gp_extract_feature_histogram );
Datum gp_extract_feature_histogram(PG_FUNCTION_ARGS)
{
	SvecType *returnval;
	char **features, **document;
	int num_features, num_words, result;
	ArrayType * arr0, * arr1;

        if (PG_ARGISNULL(0)) PG_RETURN_NULL();

        /* Error checking */
        if (PG_NARGS() != 2) 
		gp_extract_feature_histogram_errout(
	          "gp_extract_feature_histogram called with wrong number of arguments");

	arr0 = PG_GETARG_ARRAYTYPE_P(0);
	arr1 = PG_GETARG_ARRAYTYPE_P(1);

	/* Error if dictionary is empty or contains a null */
	if (ARR_HASNULL(arr0))
		gp_extract_feature_histogram_errout(
		  "dictionary argument contains a null entry");

	if (ARR_NDIM(arr0) == 0)
		gp_extract_feature_histogram_errout(
		  "dictionary argument is empty");

	/* Retrieve the C text array equivalents from the PG text[][] inputs */
	features = get_text_array_contents(arr0, &num_features);
	document = get_text_array_contents(arr1, &num_words);
	
	// Check if dictionary is sorted
	for (int i=0; i<num_features-1; i++) {
		
		result = strcmp(*(features+i),*(features+i+1));
		
		if (result > 0) {
			elog(ERROR,"Dictionary is unsorted: '%s' is out of order.\n",*(features+i+1));
		}else if (result == 0) {
			elog(ERROR,"Dictionary has duplicated word: '%s'\n",*(features+i+1));
		}

	}

	//elog(NOTICE,"Number of items in the feature array is: %d\n",num_features);
	//elog(NOTICE,"Number of items in the document array is: %d\n",num_words);

       	returnval = classify_document(features,num_features,document,num_words);

	pfree(features);
	pfree(document);

	PG_RETURN_POINTER(returnval);
}

void gp_extract_feature_histogram_errout(char *msg) {
	ereport(ERROR,
		(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
		 errmsg(
		"%s\ngp_extract_feature_histogram internal error.",msg)));
}

int str_bsearch(char * word, char ** features, int num_features) 
{
	int low = 0;
	int high = num_features - 1;
	int mid;
	int cmp_result;

	while (true) {
		if (low > high) return -5;

		mid = low + (high - low)/2;
		Assert(mid >= 0 && mid < num_features);

		cmp_result = strcmp(word, features[mid]);

		if (cmp_result == 0) return mid;
		if (cmp_result > 0) low = mid + 1;
		else high = mid - 1;
	}

	/*
	int i;
	for (i=0; i!=num_features; i++) 
		if (strcmp(word, features[i]) == 0)
			return i;
	return -5;
	*/
}

SvecType *classify_document(char **features, int num_features, 
			    char **document, int num_words) 
{
	float8 * histogram = (float8 *)palloc0(sizeof(float8)*num_features);
	SvecType * output_sfv;
	int i, idx;

	for (i=0; i!=num_words; i++) {
		if (document[i] == NULL) continue;
		idx = str_bsearch(document[i], features, num_features);
		if (idx >= 0)
			histogram[idx]++;
	}
	output_sfv = svec_from_float8arr(histogram, num_features);
	return output_sfv;
}

/**
 * Deconstruct a text[] into C-strings (note any NULL elements will be
 * returned as NULL pointers)
 */
static char ** get_text_array_contents(ArrayType *array, int *numitems)
{
        int ndim = ARR_NDIM(array);
        int * dims = ARR_DIMS(array);
        int nitems;        
	int16 typlen;
        bool typbyval;
        char typalign;
        char ** values;
        char * ptr;
        bits8 * bitmap;
        int bitmask;
        int i;

        Assert(ARR_ELEMTYPE(array) == TEXTOID);

        if (ARR_ELEMTYPE(array) != TEXTOID) {
		*numitems = 0;
		elog(WARNING,"attempt to use a non-text[][] variable with a function that uses text[][] argumenst.\n");
		return NULL;
	}

	nitems = ArrayGetNItems(ndim, dims);
        *numitems = nitems;

        get_typlenbyvalalign(ARR_ELEMTYPE(array),&typlen, &typbyval, &typalign);

        values = (char **) palloc(nitems * sizeof(char *));

        ptr = ARR_DATA_PTR(array);
        bitmap = ARR_NULLBITMAP(array);
        bitmask = 1;

        for (i = 0; i < nitems; i++) {
                if (bitmap && (*bitmap & bitmask) == 0) {
                        values[i] = NULL;
                } else {
                        values[i] = DatumGetCString(DirectFunctionCall1(textout,
					       	PointerGetDatum(ptr)));
                        ptr = att_addlength_pointer(ptr, typlen, ptr);
                        ptr = (char *) att_align_nominal(ptr, typalign);
                }
                /* advance bitmap pointer if any */
                if (bitmap) {
                        bitmask <<= 1;
                        if (bitmask == 0x100) {
                                bitmap++;
                                bitmask = 1;
                        }
                }
        }
        return values;
}



