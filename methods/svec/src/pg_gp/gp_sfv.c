#include <postgres.h>

#include <stdio.h>
#include <string.h>
#include <search.h>
#include <stdlib.h>
#include <math.h>

#include "access/tupmacs.h"
#include "catalog/pg_type.h"
#if PG_VERSION_NUM >= 90100
#include "catalog/pg_collation.h"
#endif
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"

#include "sparse_vector.h"

Datum gp_extract_feature_histogram(PG_FUNCTION_ARGS);

static void gp_extract_feature_histogram_errout(char *msg);

static SvecType * classify_document(Datum *features, int num_features,
				  Datum *document, int num_words, bool *null_words);

#if PG_VERSION_NUM >= 90100
#include "catalog/pg_collation.h"
#define TextDatumCmp(x, y) (DatumGetInt32( \
			DirectFunctionCall2Coll(bttextcmp, DEFAULT_COLLATION_OID, (x), (y))))
#else
#define TextDatumCmp(x, y) (DatumGetInt32( \
			DirectFunctionCall2(bttextcmp, (x), (y))))
#endif

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
 */

/**
 * Notes from Brian Dolan on how this feature vector is commonly used:
 *
 * The actual count is hardly ever used.  Instead, it's turned into a weight.  The most
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
	SvecType   *returnval;
	ArrayType  *arr0, *arr1;
	Datum	   *features, *document;
	int			num_features, num_words;
	bool	   *null_words;
	int16		elmlen;
	bool		elmbyval;
	char		elmalign;
	int			i;

	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();

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

	if (ARR_ELEMTYPE(arr0) != TEXTOID || ARR_ELEMTYPE(arr1) != TEXTOID)
		gp_extract_feature_histogram_errout("the input types must be text[]");

	get_typlenbyvalalign(TEXTOID, &elmlen, &elmbyval, &elmalign);
	deconstruct_array(arr0, TEXTOID, elmlen, elmbyval, elmalign,
					  &features, NULL, &num_features);
	deconstruct_array(arr1, TEXTOID, elmlen, elmbyval, elmalign,
					  &document, &null_words, &num_words);

	for (i = 0; i < num_features - 1; i++)
	{
		int		cmp;

		cmp = TextDatumCmp(features[i], features[i + 1]);

		if (cmp > 0)
			elog(ERROR, "Dictionary is unsorted: '%s' is out of order.\n",
					TextDatumGetCString(features[i + 1]));
		else if (cmp == 0)
			elog(ERROR, "Dictionary has duplicated word: '%s'\n",
					TextDatumGetCString(features[i + 1]));
	}

	returnval = classify_document(features, num_features,
								  document, num_words, null_words);
	pfree(features);
	pfree(document);

	PG_RETURN_POINTER(returnval);
}

static void
gp_extract_feature_histogram_errout(char *msg) {
	ereport(ERROR,
		(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
		 errmsg(
		"%s\ngp_extract_feature_histogram internal error.",msg)));
}

/*
 * The comparator for bsearch.
 */
static int
textdatum_cmp(const void *a, const void *b)
{
	return TextDatumCmp(*(Datum *) a, *(Datum *) b);
}

static SvecType *
classify_document(Datum *features, int num_features,
				  Datum *document, int num_words, bool *null_words)
{
	float8 * histogram = (float8 *)palloc0(sizeof(float8)*num_features);
	SvecType * output_sfv;
	int i;

	for (i = 0; i < num_words; i++)
	{
		void   *found;

		/* Skip if this word is NULL */
		if (null_words[i])
			continue;
		found = bsearch(&document[i], features, num_features,
						sizeof(Datum), textdatum_cmp);
		if (found)
		{
			int		idx;

			idx = ((uintptr_t) found - (uintptr_t) features) / sizeof(Datum);
			histogram[idx]++;
		}

	}
	output_sfv = svec_from_float8arr(histogram, num_features);
	pfree(histogram);

	return output_sfv;
}
