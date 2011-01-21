/*! 
 * \file sketch_support.c
 *
 * \brief Support routines for managing bitmaps used in sketches
 */


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
 * @about
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
 * @prereq
 * None.  Because sketches are essentially a high-performance compression technique, they were custom-coded for efficiency in C for PostgreSQL/Greenplum.
 *
 * @usage
 * The sketch method consists of a number of SQL user-defined functions (UDFs) and user-defined aggregates (UDAs), documented within each individual method.
 *
 * @todo
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

/* THIS CODE MAY NEED TO BE REVISITED TO ENSURE ALIGNMENT! */

#include "postgres.h"
#include "utils/array.h"
#include "utils/elog.h"
#include "nodes/execnodes.h"
#include "fmgr.h"
#include "sketch_support.h"
#include "utils/builtins.h"

/*!
 * Simple linear function to find the rightmost bit that's set to one
 * (i.e. the # of trailing zeros to the right).
 * \param bits a bitmap containing many fm sketches
 * \param numsketches the number of sketches in the bits variable
 * \param the size of each sketch in bits
 * \param the sketch number in which we want to find the rightmost one
 */
uint32 rightmost_one(uint8 *bits,
                     size_t numsketches,
                     size_t sketchsz_bits,
                     size_t sketchnum)
{
    uint8 *s =
        &(((uint8 *)(bits))[sketchnum*sketchsz_bits/8]);
    int    i;
    uint32 c = 0;       /* output: c will count trailing zero bits, */

    if (sketchsz_bits % (sizeof(uint32)*CHAR_BIT))
        elog(
            ERROR,
            "number of bits per sketch is %u, must be a multiple of sizeof(uint32) = %u",
            (uint32)sketchsz_bits,
            (uint32)sizeof(uint32));

    /*
     * loop through the chunks of bits from right to left, counting zeros.
     * stop when we hit a 1.
     * XXX currently we look at CHAR_BIT (8) bits at a time
     * which would seem to avoid any 32- vs. 64-bit concerns.
     * might be worth tuning this up to do 32 bits at a time.
     */
    for (i = sketchsz_bits/(CHAR_BIT) -1; i >= 0; i--)
    {
        uint32 v = (uint32) (s[i]);

        if (!v)         /* all CHAR_BIT of these bits are 0 */
            c += CHAR_BIT /* * sizeof(uint32) */;
        else
        {
            c += ui_rightmost_one(v);
            break;             /* we found a 1 in this value of i, so we stop looping here. */
        }
    }
    return c;
}

/*!
 * Simple linear function to find the leftmost zero (# leading 1's)
 * Would be nice to unify with the previous -- e.g. a foomost_bar function
 * where foo would be left or right, and bar would be 0 or 1.
 * \param bits a bitmap containing many fm sketches
 * \param numsketches the number of sketches in the bits variable
 * \param the size of each sketch in bits
 * \param the sketch number in which we want to find the rightmost one
 */
uint32 leftmost_zero(uint8 *bits,
                     size_t numsketches,
                     size_t sketchsz_bits,
                     size_t sketchnum)
{
    uint8 *s = &(((uint8 *)bits)[sketchnum*sketchsz_bits/8]);

    int    i;
    uint32 c = 0;       /* output: c will count trailing zero bits, */
    int    maxbyte = pow(2,8) - 1;

    if (sketchsz_bits % (sizeof(uint32)*8))
        elog(
            ERROR,
            "number of bits per sketch is %u, must be a multiple of sizeof(uint32) = %u",
            (uint32)sketchsz_bits,
            (uint32)sizeof(uint32));

    if (sketchsz_bits > numsketches*8)
        elog(ERROR, "sketch sz declared at %u, but bitmap is only %u",
             (uint32)sketchsz_bits, (uint32)numsketches*8);


    /*
     * loop through the chunks of bits from left to right, counting ones.
     * stop when we hit a 0.
     */
    for (i = 0; i < sketchsz_bits/(CHAR_BIT); i++)
    {
        uint32 v = (uint32) s[i];

        if (v == maxbyte)
            c += CHAR_BIT /* * sizeof(uint32) */;
        else
        {
            /*
             * reverse and invert v, then do rightmost_one
             * magic reverse from http://graphics.stanford.edu/~seander/bithacks.html#ReverseByteWith32Bits
             */
            uint8 b =
                ((s[i] * 0x0802LU &
                  0x22110LU) |
                 (s[i] * 0x8020LU &
                  0x88440LU)) * 0x10101LU >> 16;
            v = (uint32) b ^ 0xff;

            c += ui_rightmost_one(v);
            break;             /* we found a 1 in this value of i, so we stop looping here. */
        }
    }
    return c;
}


/*
 * Given an array of n b-bit bitmaps, turn on the k'th most
 * significant bit of the j'th bitmap.
 * Both j and k are zero-indexed, BUT the bitmaps are indexed left-to-right,
 * whereas significant bits are (of course!) right-to-left within the bitmap.
 *
 * This function makes destructive updates; the caller should make sure to check
 * that we're being called in an agg context!
 * \param bitmap an array of FM sketches
 * \param numsketches # of sketches in the array
 * \param sketchsz_bits # of BITS per sketch
 * \param sketchnum index of the sketch to modify (from left, zero-indexed)
 * \param bitnum bit offset (from right, zero-indexed) in that sketch
 */
Datum array_set_bit_in_place(bytea *bitmap,
                             int4 numsketches,
                             int4 sketchsz_bits,
                             int4 sketchnum,
                             int4 bitnum)
{
    char mask;
    char bytes_per_sketch = sketchsz_bits/CHAR_BIT;

    if (sketchnum >= numsketches || sketchnum < 0)
        elog(ERROR,
             "sketch offset exceeds the number of sketches (0-based)");
    if (bitnum >= sketchsz_bits || bitnum < 0)
        elog(
            ERROR,
            "bit offset exceeds the number of bits per sketch (0-based)");
    if (sketchsz_bits % sizeof(uint32))
        elog(
            ERROR,
            "number of bits per sketch is %d, must be a multiple of sizeof(uint32) = %u",
            sketchsz_bits,
            (uint32)sizeof(uint32));

    mask = 0x1 << bitnum%8;     /* the bit to be modified within the proper byte (from the right) */
    ((char *)(VARDATA(bitmap)))[sketchnum*bytes_per_sketch     /* left boundary of the proper sketch */
                                + ( (bytes_per_sketch - 1)     /* right boundary of the proper sketch */
                                    - bitnum/CHAR_BIT      /* the byte to be modified (from the right) */
                                    )
    ]
        |= mask;

    PG_RETURN_BYTEA_P(bitmap);
}

/*!
 * Simple linear function to find the rightmost one (# trailing zeros) in an uint32.
 * Based on
 * http://graphics.stanford.edu/~seander/bithacks.html#ZerosOnRightLinear
 * Look there for fancier ways to do this.
 * \param v an integer
 */
uint32 ui_rightmost_one(uint32 v)
{
    uint32 c;

    v = (v ^ (v - 1)) >> 1;     /* Set v's trailing 0s to 1s and zero rest */

    for (c = 0; v; c++)
    {
        v >>= 1;
    }

    return c;
}

/*!
 * the postgres internal md5 routine only provides public access to text output
 * here we convert that text (in hex notation) back into bytes.
 * postgres hex output has two hex characters for each 8-bit byte.
 * so the output of this will be exactly half as many bytes as the input.
 * \param hex a string encoding bytes in hex
 * \param bytes out-value that will hold binary version of hex
 * \param hexlen the length of the hex string
 */
void hex_to_bytes(char *hex, uint8 *bytes, size_t hexlen)
{
    int i;

    for (i = 0; i < hexlen; i+=2)     /* +2 to consume 2 hex characters each time */
    {
        char c1 = hex[i];         /* high-order bits */
        char c2 = hex[i+1];         /* low-order bits */
        int  b1 = 0, b2 = 0;

        if (isdigit(c1)) b1 = c1 - '0';
        else if (c1 >= 'A' && c1 <= 'F') b1 = c1 - 'A' + 10;
        else if (c1 >= 'a' && c1 <= 'f') b1 = c1 - 'a' + 10;

        if (isdigit(c2)) b2 = c2 - '0';
        else if (c2 >= 'A' && c2 <= 'F') b2 = c2 - 'A' + 10;
        else if (c2 >= 'a' && c2 <= 'f') b2 = c2 - 'a' + 10;

        bytes[i/2] = b1*16 + b2;         /* i/2 because our for loop is incrementing by 2 */
    }

}


/*! debugging utility to output strings in binary */
void
bit_print(uint8 *c, int numbytes)
{
    int    j, i, l;
    char   p[MD5_HASHLEN_BITS+2];
    uint32 n;

    for (j = 0, l=0; j < numbytes; j++) {
        n = (uint32)c[j];
        i = 1<<(CHAR_BIT - 1);
        while (i > 0) {
            if (n & i)
                p[l++] = '1';
            else
                p[l++] = '0';
            i >>= 1;
        }
        p[l] = '\0';
    }
    elog(NOTICE, "bitmap: %s", p);
}

/*!
 * The POSTGRES code for md5 returns a bytea with a textual representation of the
 * md5 result.  We then convert it back into binary.
 * XXX The internal POSTGRES source code is actually converting from binary to the bytea
 *     so this is rather wasteful, but the internal code is marked static and unavailable here.
 * XXX In fact, the full cost right now is:
 * XXX -- outfunc converts internal type to CString
 * XXX -- here we convert CString to text, to call md5_bytea
 * XXX -- md5_bytea generates a byte array which it converts to hex text
 * XXX -- we then call binary_decode to convert back to the byte array.
 * XXX -- (alternatively we could call hex_to_bytes in sketch_support.c for the last step)
 */
Datum md5_datum(char *input)
{
    Datum hashtxt =
        DirectFunctionCall1(md5_bytea, PointerGetDatum(cstring_to_text(input)));

    return DirectFunctionCall2(binary_decode, hashtxt,
                               CStringGetTextDatum("hex"));
}


/*  TEST ROUTINES */
PG_FUNCTION_INFO_V1(sketch_array_set_bit_in_place);
Datum sketch_array_set_bit_in_place(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(sketch_rightmost_one);
Datum sketch_rightmost_one(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(sketch_leftmost_zero);
Datum sketch_leftmost_zero(PG_FUNCTION_ARGS);

Datum sketch_rightmost_one(PG_FUNCTION_ARGS)
{
    bytea *bitmap = (bytea *)PG_GETARG_BYTEA_P(0);
    size_t sketchsz = PG_GETARG_INT32(1);          /* size in bits */
    size_t sketchnum = PG_GETARG_INT32(2);          /* from the left! */
    char * bits = VARDATA(bitmap);
    size_t len = VARSIZE_ANY_EXHDR(bitmap);

    return rightmost_one((uint8 *)bits, len, sketchsz, sketchnum);
}

Datum sketch_leftmost_zero(PG_FUNCTION_ARGS)
{
    bytea *bitmap = (bytea *)PG_GETARG_BYTEA_P(0);
    size_t sketchsz = PG_GETARG_INT32(1);          /* size in bits */
    size_t sketchnum = PG_GETARG_INT32(2);          /* from the left! */
    char * bits = VARDATA(bitmap);
    size_t len = VARSIZE_ANY_EXHDR(bitmap);

    return leftmost_zero((uint8 *)bits, len, sketchsz, sketchnum);
}

Datum sketch_array_set_bit_in_place(PG_FUNCTION_ARGS)
{

    bytea *bitmap = (bytea *)PG_GETARG_BYTEA_P(0);
    int4   numsketches = PG_GETARG_INT32(1);
    int4   sketchsz = PG_GETARG_INT32(2);        /* size in bits */
    int4   sketchnum = PG_GETARG_INT32(3);        /* from the left! */
    int4   bitnum = PG_GETARG_INT32(4);        /* from the right! */

    return array_set_bit_in_place(bitmap,
                                  numsketches,
                                  sketchsz,
                                  sketchnum,
                                  bitnum);
}

/* In some cases with large numbers, log2 seems to round up incorrectly. */
int4 safe_log2(int64 x)
{
    int4 out = trunc(log2(x));

    while ((((int64)1) << out) > x)
        out--;
    return out;
}
