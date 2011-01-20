/*! 
 * \file sortasort.h
 *
 * \brief header file for the sortasort directory data structure
 */
#define SORTA_SLOP 100

/*! 
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
 */
typedef struct {
    short num_vals;        /*! number of values so far */
    size_t storage_sz;     /*! the number of bytes available for strings at the end */
    size_t capacity;       /*! size of the sortasort directory */
    unsigned storage_cur;  /*! offset after the directory to do the next insertion */
    unsigned dir[0];       /*! storage of the strings */
} sortasort;

#define SORTASORT_DATA(s)  ((char *)(s->dir)) + \
    (s->capacity * (sizeof (s->dir[0])))
#define SORTASORT_GETVAL(s, i) (SORTASORT_DATA(s) + s->dir[i])

int sortasort_try_insert(sortasort *, char *);
sortasort *sortasort_init(sortasort *, size_t, size_t);
int sortasort_find(sortasort *, char *);
int sorta_cmp(const void *i, const void *j, void *thunk);
