/*!
 * A "sortasort" is a pre-marshalled *set* (no dups) of values intended for
 * append and query operations only (no deletion).  It's not a
 * particularly smart data structure.  Cuckoo hashing would be a
 * fancier solution.
 */

/*
 * It is structured as a header, followed by a fixed-length "directory"
 * (an array of offsets) that point to the actual null-terminated strings
 * concatenated in a variable-length array at the end of the directory.
 * The initial directory entries are sorted in ascending order of the strings
 * they point to, but the last < SORTA_SLOP entries are left unsorted to facilitate
 * efficient insertion.  Binary Search is used on all but those last entries,
 * which must be scanned. At every k*SORTA_SLOP'th insert, the full directory is
 * sorted.
 */
#include <ctype.h>
#include "postgres.h"
#include "fmgr.h"
#include "sortasort.h"

/*!
 * given a pre-allocated sortasort, set up its metadata
 * first argument s is the
 * \param s a pre-allocated sortasort
 * \param capacity size of the sortasort directory
 * \param s_sz size of s
 */
sortasort *
sortasort_init(sortasort *s,    
               size_t capacity, 
               size_t s_sz)     
{
    /* capacity is the size of the directory: i.e. max number of strings it can hold */
    s->capacity = capacity;
    if (s_sz - sizeof(sortasort) <= capacity*sizeof(s->dir[0]))
        elog(
            ERROR,
            "sortasort initialized too small to hold its own directory");

    /* storage_sz is the number of bytes available for strings at the end. */
    s->storage_sz = s_sz - sizeof(sortasort) - capacity*sizeof(s->dir[0]);

    /* number of values so far */
    s->num_vals = 0;

    /* offset after the directory to do the next insertion */
    s->storage_cur = 0;
    return(s);
}

/*! comparison function for qsort_arg */
int sorta_cmp(const void *i, const void *j, void *thunk)
{
    /* the "thunk" in this case is the sortasort being sorted */
    sortasort *s = (sortasort *)thunk;
    int        first = *(int *)i;
    int        second = *(int *)j;
    return strcmp((SORTASORT_DATA(s) + first), (SORTASORT_DATA(s) + second));
}


/*!
 * insert a new element into s_in if there's room and return TRUE
 * if not enough room, return FALSE
 * \param s_in a sortasort
 * \param v an element to insert
 */
int sortasort_try_insert(sortasort *s_in, char *v)
{
    /* first check to see if the element is already there */
    int found = sortasort_find(s_in, v);
    if (found >= 0 && found < s_in->num_vals) {
        /* found!  just return TRUE */
        return TRUE;
    }

    /* sanity check */
    if (found < -1 || found >= s_in->num_vals)
        elog(ERROR,
             "invalid offset %d returned by sortasort_find",
             found);

    /* we need to insert v.  return FALSE if not enough space. */
    if (s_in->storage_cur + strlen(v) + 1 >= s_in->storage_sz) {
        /* caller will have to allocate a bigger one and try again */
        return FALSE;
    }

    /* return -1 if no more capacity */
    if (s_in->num_vals >= s_in->capacity)
        return -1;

    /*
     * copy v to the current storage offset, put a pointer into dir,
     * update num_vals and storage_cur
     */
    memcpy((SORTASORT_DATA(s_in) + s_in->storage_cur), v, strlen(v) + 1);     /* +1 to pick up the '\0' */
    s_in->dir[s_in->num_vals++] = s_in->storage_cur;
    s_in->storage_cur += strlen(v) + 1;
    if (s_in->storage_cur > s_in->storage_sz)
        elog(ERROR, "went off the end of sortasort storage");

    /* re-sort every SORTA_SLOP vals */
    if (s_in->num_vals % SORTA_SLOP == 0)
        qsort_arg(s_in->dir,
                  s_in->num_vals,
                  sizeof(s_in->dir[0]),
                  sorta_cmp,
                  (void *)s_in);

    return TRUE;
}

/*!
 * find items in a sortasort.  this involves binary search in the sorted prefix,
 * and linear search in the <SORTA_SLOP-sized suffix.
 * We assume that the sorted prefix is the
 * highest multiple of SORTA_SLOP less than s->num_vals
 *
 * \param s a sortasort
 * \param v a value to find
 * \return position in directory where item found, or -1 if not found.
 * NOTE: return value 0 does not mean false!!  It means it *found* the item
 *       at position 0
 */
int sortasort_find(sortasort *s, char *v)
{
    int theguess, diff;
    int hi = (s->num_vals/SORTA_SLOP)*SORTA_SLOP;
    int themin = 0, themax = hi - 1;
    int i;
    int addend, subtrahend;

    /* binary search on the front of the sortasort */
    if (themax >= s->num_vals) {
        elog(ERROR, "max = %d, num_vals = %d", themax, s->num_vals);
    }
    theguess = hi / 2;
    while (themin < themax ) {
        if (!(diff = strcmp((SORTASORT_GETVAL(s,theguess)), v)))
            return theguess;
        if (themin == themax - 1) break;
        else if (diff < 0) {
            /* undershot */
            addend = (themax - theguess) / 2;
            if (!addend) addend = 1;
            themin = theguess;
            theguess += addend;
        }
        else {
            /* overshot */
            subtrahend = (theguess - themin) / 2;
            if (!subtrahend) subtrahend = 1;
            themax = theguess;
            theguess -= subtrahend;
        }
    }

    /* if we got here, continue with a naive linear search on the tail */
    for (i = hi; i < s->num_vals; i++)
        if (!strcmp((SORTASORT_GETVAL(s, i)), v))
            return i;
    return -1;
}
