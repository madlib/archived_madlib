/*!
 * A "sortasort" is a smallish array of strings, intended for append-only
 * modification, and network transmission as a single byte-string.  It is
 * structured as a header followed by an array of offsets (directory) that
 * point to the actual null-terminated strings stored in the "vals" array
 * at the end of the structure.
 */

/*!
 * The directory is mostly sorted in ascending order of the vals it points
 * to, but the last < SORTA_SLOP entries are left unsorted.  Binary Search
 * is used on all but those last entries, which must be scanned. At every
 * k*SORTA_SLOP'th insert, the full directory is sorted.
 */
#define SORTA_SLOP 100

typedef struct {
    short num_vals;
    size_t storage_sz;
    size_t capacity;
    unsigned storage_cur;
    unsigned dir[0];
} sortasort;

#define SORTASORT_DATA(s)  ((char *)(s->dir)) + \
    (s->capacity * (sizeof (s->dir[0])))
#define SORTASORT_GETVAL(s, i) (SORTASORT_DATA(s) + s->dir[i])

int sortasort_try_insert(sortasort *, char *);
sortasort *sortasort_init(sortasort *, size_t, size_t);
int sortasort_find(sortasort *, char *);
int sorta_cmp(const void *i, const void *j, void *thunk);
