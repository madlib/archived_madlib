if (datumLen > 1000) {
    void * array[10];
    size_t size;
    char **symbols;
    int    i;

    /* get void*'s for all entries on the stack */
    size = backtrace(array, 10);
    symbols = backtrace_symbols(array, size);
    for (i= 0; i < size; i++)
        elog(NOTICE, "%s", symbols[i]);
}

