/*
 * Support routines for managing bitmaps used in "sketching"
 * algorithms for approximating aggregates.
 */

/* THIS CODE MAY NEED TO BE REVISITED TO ENSURE ALIGNMENT! */

#include "postgres.h"
#include "utils/array.h"
#include "utils/elog.h"
#include "nodes/execnodes.h"
#include "fmgr.h"
#include "sketch_support.h"

/*
 * Simple linear function to find the rightmost bit that's set to one
 * (i.e. the # of trailing zeros to the right).
 */
unsigned int rightmost_one(unsigned char *bits,
                           size_t numsketches,
                           size_t sketchsz_bits,
                           size_t sketchnum)
{
    unsigned char *s =
        &(((unsigned char *)(bits))[sketchnum*sketchsz_bits/8]);
    int            i;
    unsigned int   c = 0;     /* output: c will count trailing zero bits, */

    if (sketchsz_bits % (sizeof(unsigned int)*CHAR_BIT))
        elog(
            ERROR,
            "number of bits per sketch is %lu, must be a multiple of sizeof(unsigned int) = %lu",
            sketchsz_bits,
            (unsigned long)sizeof(unsigned int));

    /*
     * loop through the chunks of bits from right to left, counting zeros.
     * stop when we hit a 1.
     * XXX currently we look at CHAR_BIT (8) bits at a time
     * which would seem to avoid any 32- vs. 64-bit concerns.
     * might be worth tuning this up to do 32 bits at a time.
     */
    for (i = sketchsz_bits/(CHAR_BIT) -1; i >= 0; i--)
    {
        unsigned int v = (unsigned int) (s[i]);

        if (!v)         /* all CHAR_BIT of these bits are 0 */
            c += CHAR_BIT /* * sizeof(unsigned int) */;
        else
        {
            c += ui_rightmost_one(v);
            break;             /* we found a 1 in this value of i, so we stop looping here. */
        }
    }
    return c;
}

/*
 * Simple linear function to find the leftmost zero (# leading 1's)
 * Would be nice to unify with the previous -- e.g. a foomost_bar function
 * where foo would be left or right, and bar would be 0 or 1.
 */
unsigned int leftmost_zero(unsigned char *bits,
                           size_t numsketches,
                           size_t sketchsz_bits,
                           size_t sketchnum)
{
    unsigned char *s = &(((unsigned char *)bits)[sketchnum*sketchsz_bits/8]);

    int            i;
    unsigned int   c = 0;     /* output: c will count trailing zero bits, */
    int            maxbyte = pow(2,8) - 1;

    if (sketchsz_bits % (sizeof(unsigned int)*8))
        elog(
            ERROR,
            "number of bits per sketch is %lu, must be a multiple of sizeof(unsigned int) = %lu",
            sketchsz_bits,
            (unsigned long)sizeof(unsigned int));

    if (sketchsz_bits > numsketches*8)
        elog(ERROR, "sketch sz declared at %lu, but bitmap is only %lu",
             (unsigned long)sketchsz_bits, (unsigned long)numsketches*8);


    /*
     * loop through the chunks of bits from left to right, counting ones.
     * stop when we hit a 0.
     */
    for (i = 0; i < sketchsz_bits/(CHAR_BIT); i++)
    {
        unsigned int v = (unsigned int) s[i];

        if (v == maxbyte)
            c += CHAR_BIT /* * sizeof(unsigned int) */;
        else
        {
            /*
             * reverse and invert v, then do rightmost_one
             * magic reverse from http://graphics.stanford.edu/~seander/bithacks.html#ReverseByteWith32Bits
             */
            unsigned char b =
                ((s[i] * 0x0802LU &
                  0x22110LU) |
                 (s[i] * 0x8020LU &
                                  0x88440LU)) * 0x10101LU >> 16;
            v = (unsigned int) b ^ 0xff;

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
 */
Datum array_set_bit_in_place(bytea *bitmap,      /* an array of FM sketches */
                             int4 numsketches,   /* # of sketches in the array */
                             int4 sketchsz_bits, /* # of BITS per sketch */
                             int4 sketchnum,     /* index of the sketch to modify (from left, zero-indexed) */
                             int4 bitnum)        /* bit offset (from right, zero-indexed) in that sketch */
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
    if (sketchsz_bits % sizeof(unsigned int))
        elog(
            ERROR,
            "number of bits per sketch is %d, must be a multiple of sizeof(unsigned int) = %lu",
            sketchsz_bits,
            (unsigned long)sizeof(unsigned int));

    mask = 0x1 << bitnum%8;     /* the bit to be modified within the proper byte (from the right) */
    ((char *)(VARDATA(bitmap)))[sketchnum*bytes_per_sketch     /* left boundary of the proper sketch */
                                + ( (bytes_per_sketch - 1)     /* right boundary of the proper sketch */
                                    - bitnum/CHAR_BIT      /* the byte to be modified (from the right) */
                                    )
    ]
        |= mask;

    PG_RETURN_BYTEA_P(bitmap);
}

/*
 * Simple linear function to find the rightmost one (# trailing zeros) in an unsigned int.
 * Based on
 * http://graphics.stanford.edu/~seander/bithacks.html#ZerosOnRightLinear
 * Look there for fancier ways to do this.
 */
unsigned int ui_rightmost_one(unsigned int v)
{
    unsigned int c;

    v = (v ^ (v - 1)) >> 1;     /* Set v's trailing 0s to 1s and zero rest */

    for (c = 0; v; c++)
    {
        v >>= 1;
    }

    return c;
}

/*
 * postgres internal md5 routine only provides public access to text output
 * here we convert that text (in hex notation) back into bytes.
 * postgres hex output has two hex characters for each 8-bit byte.
 * so the output of this will be exactly half as many bytes as the input.
 */
void hex_to_bytes(char *hex, unsigned char *bytes, size_t hexlen)
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


/* debugging utility to output strings in binary */
void
bit_print(unsigned char *c, int numbytes)
{
    int          j, i, l;
    char         p[MD5_HASHLEN_BITS+2];
    unsigned int n;

    for (j = 0, l=0; j < numbytes; j++) {
        n = (unsigned int)c[j];
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
