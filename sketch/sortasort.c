// A "sortasort" is serialized set (no dups) of values intended for 
// append and query operations only (no deletion).  It's not a
// particularly smart data structure.  Cuckoo hashing would be a 
// fancier solution.

// It is structured as a "directory" (an array of offsets) that point to the actual 
// null-terminated strings concatenated at the end of the directory.  
// The directory is mostly sorted in ascending order of the strings it points 
// to, but the last < SORTA_SLOP entries are left unsorted.  Binary Search is used
// on all but those last entries, which must be scanned. At every k*SORTA_SLOP'th 
// insert, the full directory is sorted.
#include <ctype.h>
#include "postgres.h"
#include "fmgr.h"
#include "sortasort.h"

// given a pre-allocated sortasort, set up its metadata
// first argument s is the
sortasort *
sortasort_init(sortasort *s,    // the space allocated for the sortasort
               size_t capacity, // size of the sortasort directory
               size_t s_sz)     // size of s
{
  // capacity is the size of the directory: i.e. max number of strings it can hold
  s->capacity = capacity;
  if (s_sz - sizeof(sortasort) <= capacity*sizeof(s->dir[0]))
    elog(ERROR, "sortasort initialized too small to hold its own directory");
    
  // storage_sz is the number of bytes available for strings at the end. 
  s->storage_sz = s_sz - sizeof(sortasort) - capacity*sizeof(s->dir[0]);
  
  // number of values so far
  s->num_vals = 0;
  
  // offset after the directory to do the next insertion
  s->storage_cur = 0;
  return(s);
}

/* comparison function for qsort_arg */
int sorta_cmp(const void *i, const void *j, void *thunk)
{
  // the "thunk" in this case is the sortasort being sorted
  sortasort *s = (sortasort *)thunk;
  int first = *(int *)i;
  int second = *(int *)j;
  return strcmp((SORTASORT_DATA(s) + first), (SORTASORT_DATA(s) + second));
}


// insert a new element into s_in if there's room and return TRUE
// if not enough room, return FALSE
int sortasort_try_insert(sortasort *s_in, char *v)
{
  // first check to see if the element is already there
  int found = sortasort_find(s_in, v);
  if (found >= 0 && found < s_in->num_vals) {
    /* found!  just return TRUE */
    return TRUE;
  }
  
  /* sanity check */
  if (found < -1 || found >= s_in->num_vals)
    elog(ERROR, "invalid offset %d returned by sortasort_find", found);
  
  // we need to insert v.  return FALSE if not enough space.
  if (s_in->storage_cur + strlen(v) + 1 >= s_in->storage_sz) {
    // caller will have to allocate a bigger one and try again
    return FALSE;
  }
  
  // copy v to the current storage offset, put a pointer into dir, 
  // update num_vals and storage_cur
  memcpy((SORTASORT_DATA(s_in) + s_in->storage_cur), v, strlen(v) + 1); // +1 to pick up the '\0'
  s_in->dir[s_in->num_vals++] = s_in->storage_cur;
  s_in->storage_cur += strlen(v) + 1;
  if (s_in->storage_cur > s_in->storage_sz)
    elog(ERROR, "went off the end of sortasort storage");

  // re-sort every SORTA_SLOP vals
  if (s_in->num_vals % SORTA_SLOP == 0)
    qsort_arg(s_in->dir, s_in->num_vals, sizeof(unsigned), sorta_cmp, (void *)s_in);
  
  return TRUE;
}

// finding items in a sortasort involves binary search in the sorted prefix,
// and linear search in the <SORTA_SLOP-sized suffix.
// We assume that the sorted prefix is the 
// highest multiple of SORTA_SLOP less than s->num_vals
//
// Returns position in directory where item found, or -1 if not found.
// NOTE: return value 0 does not mean false!!  It means it *found* the item
//       at position 0
int sortasort_find(sortasort *s, char *v)
{
  int guess, diff;
  int hi = (s->num_vals/SORTA_SLOP)*SORTA_SLOP;
  int min = 0, max = hi;
  int i;

  // binary search on the front of the sortasort
  if (max > s->num_vals)
    elog(ERROR, "max = %d, num_vals = %d", max, s->num_vals);
  guess = hi / 2;
  while (min < (max - 1)) {
    if (!(diff = strcmp((SORTASORT_GETVAL(s,guess)), v)))
      return guess;
    else if (diff < 0) {
      // undershot
      min = guess;
      guess += (max - guess) / 2;
    }
    else {
      // overshot
      max = guess;
      guess -= (guess - min) / 2;
    }
  }
  
  // if we got here, continue with a naive linear search on the tail
  for (i = hi; i < s->num_vals; i++)
    if (!strcmp((SORTASORT_GETVAL(s, i)), v))
      return i;
  return -1;      
}
