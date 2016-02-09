/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
/*
 * The library provides a simple C API.  Essentially, a new stemmer can
 * be obtained by using "sb_stemmer_new".  "sb_stemmer_stem" is then
 * used to stem a word, "sb_stemmer_length" returns the stemmed
 * length of the last word processed, and "sb_stemmer_delete" is
 * used to delete a stemmer.
 *
 * Creating a stemmer is a relatively expensive operation - the expected
 * usage pattern is that a new stemmer is created when needed, used
 * to stem many words, and deleted after some time.
 *
 * Stemmers are re-entrant, but not threadsafe.  In other words, if
 * you wish to access the same stemmer object from multiple threads,
 * you must ensure that all access is protected by a mutex or similar
 * device.
 *
 * libstemmer does not currently incorporate any mechanism for caching the results
 * of stemming operations.  Such caching can greatly increase the performance of a
 * stemmer under certain situations, so suitable patches will be considered for
 * inclusion.
 *
 * The standard libstemmer sources contain an algorithm for each of the supported
 * languages.  The algorithm may be selected using the english name of the
 * language, or using the 2 or 3 letter ISO 639 language codes.  In addition,
 * the traditional "Porter" stemming algorithm for english is included for
 * backwards compatibility purposes, but we recommend use of the "English"
 * stemmer in preference for new projects.
 */

/*
 * Postgres headers
 */
#include <postgres.h>
#include <catalog/pg_type.h>
#include <fmgr.h>
#include <sys/time.h>
#include <funcapi.h>
#include <utils/builtins.h>
#include <utils/array.h>

/*
 * libstemmer header
 */
#include "libstemmer/include/libstemmer.h"

static const char * stem_token_text(struct sb_stemmer *stemmer, text *token) {
    char * stemmed_token = TextDatumGetCString(PointerGetDatum(token));
    if (strcmp(stemmed_token, "") == 0) {
        return "";
    }
    const sb_symbol *stemmed = sb_stemmer_stem(stemmer,
                                               (const sb_symbol *) stemmed_token,
                                               strlen(stemmed_token));
    return (const char*) stemmed;
}

/*
 * Plug in stemmer call to be invoked via SQL
 */
PG_FUNCTION_INFO_V1(stem_token);
Datum stem_token(PG_FUNCTION_ARGS)
{
    if (PG_ARGISNULL(0)){
        PG_RETURN_NULL();
    }
    text * org_token = PG_GETARG_TEXT_P(0);
    struct sb_stemmer *stemmer = sb_stemmer_new(
            "english" /* language */, NULL /* language encoding NULL for UTF-8 */);
    Assert(stemmer);
    text *stemmed = cstring_to_text(stem_token_text(stemmer, org_token));
    sb_stemmer_delete(stemmer);
    PG_RETURN_TEXT_P(stemmed);
}

/*
 *  Stemmer function processing text[] input
 */
PG_FUNCTION_INFO_V1(stem_token_arr);
Datum stem_token_arr(PG_FUNCTION_ARGS)
{
    if (PG_ARGISNULL(0)){
        PG_RETURN_NULL();
    }
    /* Prepare elements to receive input text[] */
    ArrayType *arr = PG_GETARG_ARRAYTYPE_P(0);
    Datum *dtum;
    bool *nulls;
    int ndim;
    /* Deconstruct input text[] */
    deconstruct_array(arr, TEXTOID, -1, false, 'i', &dtum, &nulls, &ndim);
    /* Prepare stemmer */
    struct sb_stemmer *stemmer = sb_stemmer_new(
            "english" /* language */, NULL /* language encoding NULL for UTF-8 */);
    Assert(stemmer);

    /* Call stemming code */
    text **result = (text **) palloc(ndim * sizeof(text * ));
    for(int i=0; i< ndim; i++) {
        text *token = dtum[i] == 0 ? NULL : DatumGetTextP(dtum[i]);
        char *empty;
        if(token == NULL) {
            empty =  (char *)palloc(sizeof(char));
            empty[0] = '\0';
        }
        result[i] = (token == NULL ?
                     cstring_to_text(empty) :
                     cstring_to_text(stem_token_text(stemmer, token)));
    }
    ArrayType *res = construct_array((Datum*)result, ndim, TEXTOID, -1, false, 'i');
    sb_stemmer_delete(stemmer);
    PG_RETURN_ARRAYTYPE_P(res);
}
