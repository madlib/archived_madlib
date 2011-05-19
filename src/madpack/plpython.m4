    #
    # Import the current module PY files
    #
    import plpy
    import sys
    try:
        from MODULE_NAME import m4_ifdef( `IMPORT_SUBMODULE', `IMPORT_SUBMODULE', `MODULE_NAME')
    except:
        sys.path.append("PLPYTHON_LIBDIR")
        from MODULE_NAME import m4_ifdef( `IMPORT_SUBMODULE', `IMPORT_SUBMODULE', `MODULE_NAME')
    
    #    
    # Retrieve the schema name of the current function
    # Make it available as variable: this_schema
    #
    from inspect import getframeinfo, currentframe;
    fname = getframeinfo( currentframe()).function
    foid  = fname.rsplit( '_',1)[1] 
    # plpython names its functions "__plpython_procedure_<function name>_<oid>", 
    # of which we want the oid
    rv = plpy.execute( 'select nspname, proname from pg_proc p ' \
         'join pg_namespace n on (p.pronamespace = n.oid) ' \
         'where p.oid = %s' % foid, 1)
    this_schema  = rv[0]['nspname']