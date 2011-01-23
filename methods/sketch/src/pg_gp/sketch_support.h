/*! 
 * \file sketch_support.h
 *
 * \brief header file for sketch support routines
 */
#ifndef SKETCH_SUPPORT_H
#define SKETCH_SUPPORT_H

/*! @addtogroup desc-stats 
 *
 * @par About:
 * SQL aggregation functions (COUNT, SUM, MAX, MIN, AVG, STDDEV) are only
 * only the tip of the iceberg for describing data.  MADlib extends this
 * suite of functionality.  Where possible we try to provide high-peformance
 * algorithms that run in a single (parallel) pass of the data without 
 * overflowing main memory.  In some cases this is achieved by approximation
 * algorithms (e.g. sketches) -- for those algorithms it's important to
 * understand that answers are guaranteed mathematically to be within
 * plus-or-minus a small epsilon of the right answer with high probability.
 * It's always good to go back the research papers cited in the documents to
 * understand the caveats involved.
 *
 * In this module you will find methods for:
 *  - order statistics (quantiles, median)
 *  - distinct counting
 *  - histogramming
 *  - frequent-value counting
 */

/*! @addtogroup sketches 
 *
 * \par About:
 * There is a large body of research on small-space "sketch" techniques (sometimes called "synopsis data structures") for approximating properties of large data sets in a single pass.  Some of that work was targeted at stream or network processing, but it's equally applicable to large stored datasets.  Sketches are particularly useful for profiling multiple columns of a large table in a single pass.  This module currently implements user-defined aggregates for three main sketch methods:
 *  - <i>Flajolet-Martin (FM)</i> sketches (http://algo.inria.fr/flajolet/Publications/FlMa85.pdf) for approximately counting the number of distinct values in a set.
 *  - <i>Count-Min (CM)</i> sketches (http://dimacs.rutgers.edu/~graham/pubs/papers/cmencyc.pdf), which can be layered with scalar functions to approximate a number of descriptive statistics including
 *    - number of occurrences of a given value in a set
 *    - number of occurrences in a set that fall in a range of values (*)
 *    - order statistics including median and centiles (*)
 *    - histograms: both equi-width and equi-depth (*)
 *  - <i>Most Frequent Value (MFV)</i> sketches are basically a variant of Count-Min sketches that can generate a histogram for the most frequent values in a set. (**)
 *
 * \par
 *  <i>Note:</i> Features marked with a single star (*) only work for discrete types that can be cast to int8.  Features marked with a double star (**) do not perform aggregation in parallel.
 *
 *
 * \par Prerequisites:
 * None.  Because sketches are essentially a high-performance compression technique, they were custom-coded for efficiency in C for PostgreSQL/Greenplum.
 *
 * \par Usage/API
 * The sketch method consists of a number of SQL user-defined functions (UDFs) and user-defined aggregates (UDAs), documented within each individual method.
 *
 * \par Todo:
 * - Provide a relatively portable SQL-only implementation of CountMin.  (FM bit manipulation won't port well regardless).
 * - Provide a python wrapper to the CountMin sketch output, and write scalar functions in python.
 *
 *
 * \bug
 * - <i>Equality-Testing Issue:</i>
 * We do a lot of hashing in the sketch methods.  To provide canonical input to the
 * outfunc.  In some corner cases the ascii output may not respect the
 * equality/inequality that SQL intends.
 * The proper way to do this is not to use the outfunc, but rather to look up the
 * type-specific hash function as is done internally for hashjoin, hash indexes,
 * etc.  The basic pattern for looking up the hash function in Postgres
 * internals is something like the following:
 * <c>
 * get_sort_group_operators(dtype, false, true, false, &ltOpr, &eqOpr, &gtOpr);
 * success = get_op_hash_functions(eqOpr, result, NULL));
 * </c>
 */


#define MD5_HASHLEN_BITS 16*8 /*! md5 hash length in bits */

#ifndef MAXINT8LEN
#define MAXINT8LEN              25 /*! number of chars to hold an int8 */
#endif

Datum  array_set_bit_in_place(bytea *, int4, int4, int4, int4);
uint32 rightmost_one(uint8 *, size_t, size_t, size_t);
uint32 leftmost_zero(uint8 *, size_t, size_t, size_t);
uint32 ui_rightmost_one(uint32 v);
void   hex_to_bytes(char *hex, uint8 *bytes, size_t);
void bit_print(uint8 *c, int numbytes);
Datum md5_datum(char *);
int4 safe_log2(int64);
#endif /* SKETCH_SUPPORT_H */
