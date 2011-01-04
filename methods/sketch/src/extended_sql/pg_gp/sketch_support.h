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
        

*/


#define MD5_HASHLEN_BITS 16*8 /*! md5 hash length in bits */

Datum        array_set_bit_in_place(bytea *, int4, int4, int4, int4);
uint32 rightmost_one(uint8 *, size_t, size_t, size_t);
uint32 leftmost_zero(uint8 *, size_t, size_t, size_t);
uint32 ui_rightmost_one(uint32 v);
void         hex_to_bytes(char *hex, uint8 *bytes, size_t);
void bit_print(uint8 *c, int numbytes);
Datum md5_datum(char *);
#endif /* SKETCH_SUPPORT_H */
