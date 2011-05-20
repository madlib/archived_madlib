/**
 * PostgreSQL include file for sql_in files.
 *
 * We currently use m4, and m4 is a pain. Unfortunately, there seems no easy way
 * to escape the quote string, so we change it at the beginning and restore at
 * the end.
 * m4_changequote(<!,!>)
 */

/*
 * PythonFunction
 *
 * @param $1 directory
 * @param $2 python file (without suffix)
 * @param $3 function
 *
 * Example:
 * CREATE FUNCTION MADLIB_SCHEMA.logregr_coef(
 *     "source" VARCHAR,
 *     "depColumn" VARCHAR,
 *     "indepColumn" VARCHAR)
 * RETURNS DOUBLE PRECISION[]
 * AS PythonFunction(`regress', `logistic', `compute_logregr_coef')
 * LANGUAGE plpythonu VOLATILE;
 */ 
m4_define(<!PythonFunction!>, <!
$$
    import sys
    from inspect import getframeinfo, currentframe
    try:
        from $1 import $2
    except:
        sys.path.append("PLPYTHON_LIBDIR")
        from $1 import $2
    
    # Retrieve the schema name of the current function
    # Make it available as variable: MADlibSchema
    fname = getframeinfo(currentframe()).function
    foid  = fname.rsplit('_',1)[1]

    # plpython names its functions "__plpython_procedure_<function name>_<oid>",
    # of which we want the oid
    rv = plpy.execute('SELECT nspname, proname FROM pg_proc p ' \
         'JOIN pg_namespace n ON (p.pronamespace = n.oid) ' \
         'WHERE p.oid = %s' % foid, 1)

    global MADlibSchema
    MADlibSchema = rv[0]['nspname']
    
    return $2.$3(**globals())
$$
!>)

/*
 * Repetition: m4 is a lousy preprocessor. We change the quote character back to
 * their defaults.
 * m4_changequote(<!`!>,<!'!>)
 */
