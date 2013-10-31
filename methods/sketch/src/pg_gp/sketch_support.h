/*!
 * \file sketch_support.h
 *
 * \brief header file for sketch support routines
 */
#ifndef SKETCH_SUPPORT_H
#define SKETCH_SUPPORT_H

#define MD5_HASHLEN 16
#define MD5_HASHLEN_BITS 8*MD5_HASHLEN /*! md5 hash length in bits */

#ifndef MAXINT8LEN
#define MAXINT8LEN              25 /*! number of chars to hold an int8 */
#endif

#ifndef CHAR_BIT
#include <limits.h>
#endif

Datum  array_set_bit_in_place(bytea *, int32, int32, int32, int32);
uint32 rightmost_one(uint8 *, size_t, size_t, size_t);
uint32 leftmost_zero(uint8 *, size_t, size_t, size_t);
uint32 ui_rightmost_one(uint32 v);
void   hex_to_bytes(char *hex, uint8 *bytes, size_t);
void bit_print(uint8 *c, int numbytes);
Datum md5_cstring(char *);
bytea *sketch_md5_bytea(Datum, Oid);
int32   safe_log2(int64);
void   int64_big_endianize(uint64 *, uint32, bool);

/*! macro to convert a pointer into a marshalled array of Datums into a Datum */
#define PointerExtractDatum(x, byVal)  (byVal ? (*(Datum *)x) : (PointerGetDatum(x)))
/*! macro to convert a Datum into a pointer suitable for memcpy */
#define DatumExtractPointer(x, byVal)  (byVal ? (void *)&x : DatumGetPointer(x))

size_t ExtractDatumLen(Datum x, int len, bool byVal, size_t capacity);
#endif /* SKETCH_SUPPORT_H */
