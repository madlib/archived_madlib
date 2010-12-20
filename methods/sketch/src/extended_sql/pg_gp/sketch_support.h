#ifndef SKETCH_SUPPORT_H
#define SKETCH_SUPPORT_H

#define MD5_HASHLEN_BITS 16*8 /*! md5 hash length in bits */

Datum        array_set_bit_in_place(bytea *, int4, int4, int4, int4);
unsigned int rightmost_one(unsigned char *, size_t, size_t, size_t);
unsigned int leftmost_zero(unsigned char *, size_t, size_t, size_t);
unsigned int ui_rightmost_one(unsigned int v);
void         hex_to_bytes(char *hex, unsigned char *bytes, size_t);
void bit_print(unsigned char *c, int numbytes);
Datum md5_datum(char *);
#endif /* SKETCH_SUPPORT_H */
