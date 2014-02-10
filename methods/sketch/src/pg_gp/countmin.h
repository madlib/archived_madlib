/*!
 * \file countmin.h
 *
 * \brief header file for CM and MFV sketches
 */

#ifndef _COUNTMIN_H_
#define _COUNTMIN_H_

#include <limits.h>  /* CHAR_BIT */


#define INT64BITS (sizeof(int64)*CHAR_BIT)
#define RANGES INT64BITS
#define DEPTH 8 /* magic tuning value: number of hash functions */
/* #define NUMCOUNTERS 65535 */
#define NUMCOUNTERS 1024  /* another magic tuning value: modulus of hash functions */

#ifdef INT64_IS_BUSTED
#define MAX_INT64 (INT64CONST(0x7FFFFFFF))
#define MAX_UINT64 (UINT64CONST(0xFFFFFFFF))
#else
#define MAX_INT64 (INT64CONST(0x7FFFFFFFFFFFFFFF))
#define MAX_UINT64 (UINT64CONST(0xFFFFFFFFFFFFFFFF))
#endif /* INT64_IS_BUSTED */

#define MID_INT64 (0)
#define MIN_INT64 (~MAX_INT64)
#define MID_UINT64 (MAX_UINT64 >> 1)
#define MIN_UINT64 (0)


/*!
 * \brief the CountMin sketch array
 *
 * a CountMin sketch is a set of DEPTH arrays of NUMCOUNTERS each.
 * It's like a "counting Bloom Filter" where instead of just hashing to
 * DEPTH bitmaps, we count up hash-collisions in DEPTH counter arrays
 */
typedef uint64 countmin[DEPTH][NUMCOUNTERS];

#define MAXARGS 3

/*!
 * \internal
 * \brief the transition value struct for CM sketches
 *
 * Holds the sketch counters
 * and a cache of handy metadata that we'll reuse across calls
 * \endinternal
 */
typedef struct {
    int64 args[MAXARGS];  /*! carry along additional args for finalizer */
    int   nargs;          /*! number of args being carried for finalizer */
    countmin sketches[RANGES];
} cmtransval;

/*! base size of a cmtransval */
#define CM_TRANSVAL_SZ (VARHDRSZ + sizeof(cmtransval))

#define CM_TRANSVAL_INITIALIZED(t) (VARSIZE(t) >= CM_TRANSVAL_SZ)


/*!
 * \internal
 * \brief array of ranges
 *
 * a data structure to hold the constituent dyadic (power-of-two) ranges
 * corresponding to an arbitrary range.
 * E.g. 14-48 becomes [[14-15], [16-31], [32-47], [48-48]]
 * \endinternal
 */
typedef struct {
    int64 spans[2*INT64BITS][2]; /*! the ranges */
    uint32 emptyoffset;        /*! offset of next empty span */
} rangelist;

#define ADVANCE_OFFSET(r)  \
    if ((r).emptyoffset == 2*INT64BITS) { \
        uint32 i; \
        for (i = 0; i < 2*INT64BITS; i++) \
            elog(NOTICE, \
                 "[" INT64_FORMAT ", " INT64_FORMAT "]", \
                 (r).spans[i][0], \
                 (r).spans[i][1]); \
        elog(ERROR, "countmin error: rangelist overflow"); \
    } \
    else ((r).emptyoffset++);

/*!
 * \internal
 * \brief offset/count pairs for MFV sketches
 * \endinternal
 */
typedef struct {
    unsigned offset;  /*! memory offset to the value */
    uint64 cnt;   /*! counter */
} offsetcnt;


/*!
 * \internal
 * \brief the transition value struct for MFV sketches.
 *
 * Holds a single
 * countmin sketch (no dyadic ranges) and an array of Most Frequent Values.
 * We are flexible with the number of mfvs, as well as the type.
 * Hence at the end of this struct is an array mfv[max_mfvs] of offsetcnt entries,
 * followed by an array of Postgres text objects with the output formats of
 * the mfvs.
 * Each mfv entry contains an offset from the top of the structure where
 * we can find a Postgres text object holding the output format of a
 * frequent value.
 * \endinternal
 */
typedef struct {
    unsigned max_mfvs;    /*! number of frequent values */
    unsigned next_mfv;    /*! index of next mfv to insert into */
    unsigned next_offset; /*! next memory offset to insert into */
    Oid typOid;           /*! Oid of the type being counted */
    int typLen;           /*! Length of the data type */
    bool typByVal;        /*! Whether type is by value or by reference */
    Oid outFuncOid;       /*! Oid of the outfunc for this type */
    countmin sketch;      /*! a single countmin sketch */
    /*!
     * type-independent collection of Most Frequent Values
     * Holds an array of (counter,offset) pairs, which by
     * convention is followed by the values themselves as text Datums,
     * accessible via the offsets
     */
    offsetcnt mfvs[];
} mfvtransval;

/*! base size of an MFV transval */
#define MFV_TRANSVAL_SZ(i) (VARHDRSZ + sizeof(mfvtransval) + i*sizeof(offsetcnt))

/*! free space remaining for text values */
#define MFV_TRANSVAL_CAPACITY(transblob) (VARSIZE(transblob) - VARHDRSZ - \
                                          ((mfvtransval *)VARDATA(transblob))-> \
                                          next_offset)

/* countmin aggregate protos */
Datum  countmin_trans_c(countmin, Datum, Oid, Oid);
bytea *cmsketch_check_transval(PG_FUNCTION_ARGS, bool);
bytea *cmsketch_init_transval();
void   countmin_dyadic_trans_c(cmtransval *, Datum);

/* countmin scalar function protos */
int64  cmsketch_count_c(countmin, Datum, Oid, Oid);
int64  cmsketch_count_md5_datum(countmin, bytea *, Oid);

/* hash_counters_iterate and its lambdas */
int64  hash_counters_iterate(bytea *, countmin, int64, int64 (*lambdaptr)(
                                 uint32,
                                 uint32,
                                 countmin,
                                 int64));

int64  increment_counter(uint32, uint32, countmin, int64);
int64  min_counter(uint32, uint32, countmin, int64);

/* MFV protos */
bytea *mfv_transval_append(bytea *, Datum);
int    mfv_find(bytea *, Datum);
bytea *mfv_transval_replace(bytea *, Datum, int);
bytea *mfv_transval_insert_at(bytea *, Datum, uint32);
void *mfv_transval_getval(bytea *, uint32);
bytea *mfv_init_transval(int, Oid);
bytea *mfvsketch_merge_c(bytea *, bytea *);
void   mfv_copy_datum(bytea *, int, Datum);
int cnt_cmp_desc(const void *i, const void *j);


/* UDF protos */
Datum __cmsketch_int8_trans(PG_FUNCTION_ARGS);
Datum cmsketch_width_histogram(PG_FUNCTION_ARGS);
Datum cmsketch_dhistogram(PG_FUNCTION_ARGS);
Datum __cmsketch_base64_final(PG_FUNCTION_ARGS);
Datum __cmsketch_merge(PG_FUNCTION_ARGS);
Datum cmsketch_dump(PG_FUNCTION_ARGS);
Datum __cmsketch_count_final(PG_FUNCTION_ARGS);
Datum __cmsketch_rangecount_final(PG_FUNCTION_ARGS);
Datum __cmsketch_centile_final(PG_FUNCTION_ARGS);
Datum __cmsketch_median_final(PG_FUNCTION_ARGS);
Datum __cmsketch_dhist_final(PG_FUNCTION_ARGS);
Datum __mfvsketch_trans(PG_FUNCTION_ARGS);
Datum __mfvsketch_final(PG_FUNCTION_ARGS);
Datum __mfvsketch_merge(PG_FUNCTION_ARGS);

#endif /* _COUNTMIN_H_ */

