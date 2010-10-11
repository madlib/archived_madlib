#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "sortasort.h"
#include <libpq-fe.h>

#define SORTASORT_SIZE sizeof(sortasort) + 1024*sizeof(unsigned) + 1024

int
main(int argc, char **argv)
{
    FILE *     fp;
    char       s[1024];
    sortasort *ss = (sortasort *)malloc(SORTASORT_SIZE), *ss_new;
    int        i, item, retval;

    for (i = 0; i < 100; i++) {
        retval= 0;
        item = 0;
        fp = fopen(argv[1], "r");
        if (fp == NULL) {
            fprintf(stderr, "file not found");
            return(-1);
        }

        sortasort_init(ss, 1024, SORTASORT_SIZE);

        while (fgets(s, 1024, fp)) {
            item++;
            retval = sortasort_try_insert(ss, s);
            if (retval == -2) break;
            if (!retval) {
                printf("doubling...\n");
                ss_new = (sortasort *)malloc(
                    sizeof(sortasort) + 1024*
                    sizeof(unsigned) +
                    ss->storage_sz*2 + strlen(s));
                memcpy(
                    ss_new,
                    ss,
                    sizeof(sortasort) + 1024*
                    sizeof(unsigned) +
                    ss->storage_sz);
                ss_new->storage_sz = ss_new->storage_sz*2 +
                                     strlen(s);
                free(ss);
                ss = ss_new;
                retval = sortasort_try_insert(ss, s);
            }
        }

        fclose(fp);
    }

    for(i = 0; i < ss->num_vals; i++) {
        printf("%d: %s", i, SORTASORT_GETVAL(ss,i));
        if (!sortasort_find(ss, SORTASORT_GETVAL(ss,i))) {
            printf("XXX failed to find %s", SORTASORT_GETVAL(ss,i));
            return -1;
        }
    }
    return(0);
}
