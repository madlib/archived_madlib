/*-------------------------------------------------------------------------
 *
 * bayes.h
 *
 * Naive Bayes Classification.
 *
 *------------------------------------------------------------------------- 
 */

#ifndef MADLIB_BAYES_H
#define MADLIB_BAYES_H

#include "postgres.h"
#include "fmgr.h"

extern Datum nb_classify_accum(PG_FUNCTION_ARGS);
extern Datum nb_classify_combine(PG_FUNCTION_ARGS);
extern Datum nb_classify_final(PG_FUNCTION_ARGS);
extern Datum nb_probabilities_final(PG_FUNCTION_ARGS);

#endif
