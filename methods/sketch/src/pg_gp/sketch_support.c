/*!
 * \file sketch_support.c
 *
 * \brief Support routines for managing bitmaps used in sketches
 */


/*! @addtogroup grp_desc_stats
 *
 * @par About:
 * MADlib provides a number of descriptive statistics to complement
 * SQL's builtin aggregation functions (COUNT, SUM, MAX, MIN, AVG, STDDEV).
 * When possible we try
 * to provide high-peformance algorithms that run in a single (parallel)
 * pass of the data without overflowing main memory.  In some cases this
 * is achieved by approximation
 * algorithms (e.g. sketches) -- for those algorithms it's important to
 * understand that answers are guaranteed mathematically to be within
 * plus-or-minus a small epsilon of the right answer with high probability.
 * It's always good to go back the research papers cited in the documents to
 * understand the caveats involved.
 *
 * \par
 * In this module you will find methods for:
 *  - order statistics (quantiles, median)
 *  - distinct counting
 *  - histogramming
 *  - frequent-value counting
 */

/*
@addtogroup grp_sketches
*/

#include <math.h>

/* THIS CODE MAY NEED TO BE REVISITED TO ENSURE ALIGNMENT! */

#include <postgres.h>
#include <utils/array.h>
#include <utils/elog.h>
#include <nodes/execnodes.h>
#include <fmgr.h>
#include <utils/builtins.h>
#if PG_VERSION_NUM >= 100000
#include <common/md5.h>
#else
#include <libpq/md5.h>
#endif
#include <utils/lsyscache.h>
#include "sketch_support.h"



/*!
 * Simple linear function to find the rightmost bit that's set to one
 * (i.e. the # of trailing zeros to the right).
 * \param bits a bitmap containing many fm sketches
 * \param numsketches the number of sketches in the bits variable
 * \param sketchsz_bits the size of each sketch in bits
 * \param sketchnum the sketch number in which we want to find the rightmost one
 */
uint32 rightmost_one(uint8 *bits,
                     size_t numsketches,
                     size_t sketchsz_bits,
                     size_t sketchnum)
{
    (void) numsketches; /* avoid warning about unused parameter */
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
    uint8 *  s = &(((uint8 *)bits)[sketchnum*sketchsz_bits/8]);

    unsigned i;
    uint32   c = 0;     /* output: c will count trailing zero bits, */
    uint32   maxbyte = pow(2,8) - 1;

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
                             int32 numsketches,
                             int32 sketchsz_bits,
                             int32 sketchnum,
                             int32 bitnum)
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
    uint32 i;

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
 * Run the datum through an md5 hash.  No need to special-case variable-length types,
 * we'll just hash their length header too.
 * The POSTGRES code for md5 returns a bytea with a textual representation of the
 * md5 result.  We then convert it back into binary.
 * \param dat a Postgres Datum
 * \param typOid Postgres type Oid
 * \returns a bytea containing the hashed bytes
 */
bytea *sketch_md5_bytea(Datum dat, Oid typOid)
{
    // according to postgres' libpq/md5.c, need 33 bytes to hold
    // null-terminated md5 string
    char outbuf[MD5_HASHLEN*2+1];
    bytea *out = palloc0(MD5_HASHLEN+VARHDRSZ);
    bool byval = get_typbyval(typOid);
    int len = ExtractDatumLen(dat, get_typlen(typOid), byval, -1);
    void *datp = DatumExtractPointer(dat, byval);
    /*
     * it's very common to be hashing 0 for countmin sketches.  Rather than
     * hard-code it here, we cache on first lookup.  In future a bigger cache here
     * would be nice.
     */
    static bool zero_cached = false;
    static char md5_of_0_mem[MD5_HASHLEN+VARHDRSZ];
    static bytea *md5_of_0 = (bytea *) &md5_of_0_mem;

    if (byval && len == sizeof(int64) && *(int64 *)datp == 0 && zero_cached) {
        return md5_of_0;
    }
    else
        pg_md5_hash(datp, len, outbuf);

    hex_to_bytes(outbuf, (uint8 *)VARDATA(out), MD5_HASHLEN*2);
    SET_VARSIZE(out, MD5_HASHLEN+VARHDRSZ);
    if (byval && len == sizeof(int64) && *(int64 *)datp == 0 && !zero_cached) {
        zero_cached = true;
        memcpy(md5_of_0, out, MD5_HASHLEN+VARHDRSZ);
    }
    return out;
}


/*  TEST ROUTINES */
PG_FUNCTION_INFO_V1(sketch_array_set_bit_in_place);
Datum sketch_array_set_bit_in_place(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(sketch_rightmost_one);
Datum sketch_rightmost_one(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(sketch_leftmost_zero);
Datum sketch_leftmost_zero(PG_FUNCTION_ARGS);

#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6))
/*
 * Unfortunately, the VARSIZE_ANY_EXHDR macro produces a warning because of the
 * way type promotion and artihmetic conversions works in C99. See §6.1.3.8 of
 * the C99 standard.
 */
#pragma GCC diagnostic push
#endif
#pragma GCC diagnostic ignored "-Wsign-compare"
Datum sketch_rightmost_one(PG_FUNCTION_ARGS)
{
    bytea *bitmap = (bytea *)PG_GETARG_BYTEA_P(0);
    size_t sketchsz = PG_GETARG_INT32(1);          /* size in bits */
    size_t sketchnum = PG_GETARG_INT32(2);          /* from the left! */
    char * bits = VARDATA(bitmap);
    size_t len = (size_t)(VARSIZE_ANY_EXHDR(bitmap));

    return rightmost_one((uint8 *)bits, len, sketchsz, sketchnum);
}

Datum sketch_leftmost_zero(PG_FUNCTION_ARGS)
{
    bytea *bitmap = (bytea *)PG_GETARG_BYTEA_P(0);
    size_t sketchsz = PG_GETARG_INT32(1);          /* size in bits */
    size_t sketchnum = PG_GETARG_INT32(2);          /* from the left! */
    char * bits = VARDATA(bitmap);
    size_t len = (size_t)(VARSIZE_ANY_EXHDR(bitmap));

    return leftmost_zero((uint8 *)bits, len, sketchsz, sketchnum);
}
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6))
#pragma GCC diagnostic pop
#endif

Datum sketch_array_set_bit_in_place(PG_FUNCTION_ARGS)
{

    bytea *bitmap = (bytea *)PG_GETARG_BYTEA_P(0);
    int32   numsketches = PG_GETARG_INT32(1);
    int32   sketchsz = PG_GETARG_INT32(2);        /* size in bits */
    int32   sketchnum = PG_GETARG_INT32(3);        /* from the left! */
    int32   bitnum = PG_GETARG_INT32(4);        /* from the right! */

    return array_set_bit_in_place(bitmap,
                                  numsketches,
                                  sketchsz,
                                  sketchnum,
                                  bitnum);
}

/* In some cases with large numbers, log2 seems to round up incorrectly. */
int32 safe_log2(int64 x)
{
    int32 out = trunc(log2(x));

    while ((((int64)1) << out) > x)
        out--;
    return out;
}

/*
 * We need process c string and var especially here, it is really ugly,
 * but we have to.
 * Because here the user can change the binary representations directly.
 */
size_t ExtractDatumLen(Datum x, int len, bool byVal, size_t capacity)
{
    (void) byVal; /* avoid warning about unused parameter */
    size_t size = 0;
    size_t idx = 0;
    char   *data = NULL;
    if (len > 0)
    {
        size = len;
        if (capacity != (size_t)-1 && size > capacity) {
            elog(ERROR, "invalid transition state");
        }
    }
    else if (len == -1)
    {
        if (capacity == (size_t)-1) {
            size = VARSIZE_ANY(DatumGetPointer(x));
        }
        else {
            data = (char*)DatumGetPointer(x);
            if ((capacity >= VARHDRSZ)
                || (capacity >= 1 && VARATT_IS_1B(data))) {
                size = VARSIZE_ANY(data);
            }
            else {
                elog(ERROR, "invalid transition state");
            }
        }
    }
    else if (len == -2)
    {
        if (capacity == (size_t)-1) {
            return strlen((char *)DatumGetPointer(x)) + 1;
        }
        else {
            data = (char*)DatumGetPointer(x);
            size = 0;
            for (idx = 0; idx < capacity && data[idx] != 0; idx ++, size ++) {
            }
            if (idx >= capacity) {
                elog(ERROR, "invalid transition state");
            }
            size ++;
        }
    }
    else {
        elog(ERROR, "Datum typelength error in ExtractDatumLen: len is %u", (unsigned)len);
        return 0;
    }

    return size;
}

/*
 * walk an array of int64s and convert word order of int64s to big-endian
 * if force == true, convert even if this arch is big-endian
 */
void int64_big_endianize(uint64 *bytes64,
                         uint32 numbytes,
                         bool force)
{
    uint32 i;
    uint32 *bytes32 = (uint32 *)bytes64;
    uint32 x;
    uint32 total = 0;
#ifdef WORDS_BIGENDIAN
    bool small_endian = false;
#else
    bool small_endian = true;
#endif

    if (numbytes % 8)
        elog(ERROR, "illegal numbytes argument to big_endianize: not a multiple of 8");
    if (small_endian || force) {
        for (i = 0; i < numbytes/8; i+=2) {
            x = bytes32[i];
            bytes32[i] = bytes32[i+1];
            bytes32[i+1] = x;
            total++;
        }
    }
}

