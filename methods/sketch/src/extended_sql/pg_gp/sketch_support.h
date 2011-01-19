#ifndef SKETCH_SUPPORT_H
#define SKETCH_SUPPORT_H

/*! \defgroup sketch

Implementation of a variety of small-space "sketch" techniques for approximating properties of large sets in a single pass.

\par About:

This module currently implements user-defined aggregates for two main sketch methods:
- Flajolet-Martin (FM) sketches (http://algo.inria.fr/flajolet/Publications/FlMa85.pdf) for approximately counting the number of distinct values in a set (i.e. SQL's COUNT(DISTINCT x)).  
- Count-Min sketches (http://dimacs.rutgers.edu/~graham/pubs/papers/cmencyc.pdf), which can be layered with scalar functions to approximating a number of descriptive statistics including
    - number of occurrences of a given value
    - number of occurrences in a range of values (*)
    - order statistics including median and centiles (*)
    - histograms: both equi-width and equi-depth (*)
    - most frequently occurring values ("MFV" sketches) (**)

Features marked with a single star (*) only work for discrete types that can be cast to int8.  Features marked with a double star (**) do not perform aggregation in parallel.


\par Prerequisites:

Implemented in C for PostgreSQL/Greenplum.

\par Todo:
- Provide a relatively portable SQL-only implementation of CountMin.  (FM bit manipulation won't port well regardless).
- Provide a python wrapper to the CountMin sketch output, and write scalar functions in python.

\par Example Execution (in-database):

See unit tests in the sql subdirectory.
        
\bug Equality-Testing Issue
We do a lot of hashing in the sketch methods.  To provide canonical input to the
outfunc.  In some corner cases the ascii output may not respect the 
equality/inequality that SQL intends.

The proper way to do this is not to use the outfunc, but rather to look up the 
type-specific hash function as is done internally for hashjoin, hash indexes, 
etc.  The basic pattern for looking up the hash function in Postgres 
internals is something like the following:
<c>
get_sort_group_operators(dtype, false, true, false, &ltOpr, &eqOpr, &gtOpr);
success = get_op_hash_functions(eqOpr, result, NULL));
</c>


*/


#define MD5_HASHLEN_BITS 16*8 /*! md5 hash length in bits */

#ifndef MAXINT8LEN
#define MAXINT8LEN		25 /*! number of chars to hold an int8 */
#endif


Datum        array_set_bit_in_place(bytea *, int4, int4, int4, int4);
uint32 rightmost_one(uint8 *, size_t, size_t, size_t);
uint32 leftmost_zero(uint8 *, size_t, size_t, size_t);
uint32 ui_rightmost_one(uint32 v);
void         hex_to_bytes(char *hex, uint8 *bytes, size_t);
void bit_print(uint8 *c, int numbytes);
Datum md5_datum(char *);
#endif /* SKETCH_SUPPORT_H */
