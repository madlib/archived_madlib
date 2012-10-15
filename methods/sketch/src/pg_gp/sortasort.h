/*!
 * \file sortasort.h
 *
 * \brief header file for the sortasort directory data structure
 */
#define SORTA_SLOP 100

/*!
 * \internal
 * \brief a pre-marshalled directory data structure to hold strings
 *
 * A sortasort is a smallish array of strings, intended for append-only
 * modification, and network transmission as a single byte-string.  It is
 * structured as a header followed by an array of offsets (directory) that
 * point to the actual null-terminated strings stored in the "vals" array
 * at the end of the structure.
 *
 * The directory is mostly sorted in ascending order of the vals it points
 * to, but the last < SORTA_SLOP entries are left unsorted.  Binary Search
 * is used on all but those last entries, which must be scanned. At every
 * k*SORTA_SLOP'th insert, the full directory is sorted.
 * \endinternal
 */
typedef struct {
    size_t num_vals;       /*! number of values so far */
    size_t storage_sz;     /*! the number of bytes available for strings at the end */
    size_t capacity;       /*! size of the sortasort directory */
    int    typLen;         /*! length of this Postgres type (-1 for bytea, -2 for cstring) */
    size_t typByVal;       /*! Postgres typByVal flag */
    unsigned storage_cur;  /*! offset after the directory to do the next insertion */
    unsigned dir[];        /*! storage of the strings */
} sortasort;



/*
 * get the ith item stored in sortasort
 */
char *sortasort_getval(sortasort *s, unsigned i);

int sortasort_try_insert(sortasort *, Datum, int);
sortasort *sortasort_init(sortasort *, size_t, size_t, int, bool);
int sortasort_find(sortasort *, Datum);
int sorta_cmp(const void *i, const void *j, void *thunk);
