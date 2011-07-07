#!/usr/bin/env python
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Main Madpack installation executable.
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
import sys 
import getpass
import re
import os
import glob
import traceback
import subprocess
import datetime
from time import strftime

# Required Python version
py_min_ver = [2,6]

# Check python version
if sys.version_info[:2] < py_min_ver:
    print "ERROR: python version too old (%s). You need %s or greater." \
          % ( '.'.join(str(i) for i in sys.version_info[:3]), '.'.join(str(i) for i in py_min_ver))
    exit(1)

# Find MADlib root directory. This file is installed to
# $MADLIB_ROOT/madpack/madpack.py, so to get $MADLIB_ROOT we need to go
# two levels up in the directory hierarchy. We use (a) os.path.realpath and
# (b) __file__ (instead of sys.argv[0]) because madpack.py could be called
# (a) through a symbolic link and (b) not as the main module.
maddir = os.path.abspath( os.path.dirname( os.path.realpath(
    __file__)) + "/..")   # MADlib root dir
sys.path.append( maddir + "/madpack")

# Following lines are for development/testing only, so madpack.py can be run
# from the SRC directory tree
# maddir = os.path.abspath( os.path.dirname( os.path.realpath(__file__)) + "/../..")   # MADlib root dir
# sys.path.append( maddir + "/src/madpack")
# END - For development/testing only

# Import MADlib python modules
import argparse
import configyml

# Some read-only variables
this =  os.path.basename(sys.argv[0])   # name of this script

# Default directories
maddir_conf = maddir + "/config"           # Config dir
maddir_lib  = maddir + "/lib/libmadlib.so" # C/C++ libraries 

# Tempdir
tmpdir = '/tmp/madlib/log/%s' % strftime( "%Y%m%d-%H%M%S")

# Read the config files
ports = configyml.get_ports( maddir_conf )  # object made of Ports.yml
rev = configyml.get_version( maddir_conf )  # MADlib OS-level version
portid_list = []
for port in ports['ports']:
    portid_list.append(port['id'])

# Global variables
portid = None       # Target port ID ( eg: pg90, gp40)
dbconn = None       # DB Connection object
con_args = {}       # DB connection arguments
verbose = None      # Verbose flag
logfile = tmpdir + '/madpack.log'

## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Create a temp dir 
# @param dir temp directory path
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def __make_log_dir( dir):
    if not os.path.isdir( dir):
        try:
            os.makedirs( dir)
        except:
            print "ERROR: can not create directory: %s. Check permissions." % dir
            exit(1)    

## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Close the log file 
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def __log_close():
    try:
        log.close()     
        if os.path.getsize( logfile) == 0:
            os.remove( logfile)       
        else:
            __info( "Log saved in " + logfile, verbose)
    except:
        pass            

## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Error message wrapper 
# @param msg error message
# @param stop program exit flag
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def __error( msg, stop):
    # Print stack trace
    if stop == True:
        # traceback.print_exc()
        log.write( traceback.format_exc())
    # Print to stdout
    print this + ' : ERROR : ' + msg
    # Write to log file
    log.write( this + ' : ERROR : ' + msg + '\n')
    # Stop script
    if stop == True:
        # __log_close()
        exit(2)

#class MadpackError( Exception):
#    pass

## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Info message wrapper (verbose)
# @param msg info message
# @param verbose prints only if True
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def __info( msg, verbose):
    # Print to stdout
    if verbose:
        print this + ' : INFO : ' + msg
    # Write to log file
    log.write( this + ' : INFO : ' + msg + '\n')

## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Try importing specified dbapi module
# If success: return module handle
# @param api module name for database connection
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def __import_dbapi( api):
    try:       
        dbapi2 = __import__( api, globals(), locals(), ['']);
    except:
        if api.rfind('.') > 0:
            dbapi2 = __import__( api[api.rfind('.')+1:], globals(), locals(), [''])
    return dbapi2

## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Read MADlib version from database
# @param dbconn database conection object
# @param schema MADlib schema name
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def __get_madlib_dbver( schema):
    cur = dbconn.cursor()
    try:
        cur.execute( "SELECT version FROM %s.migrationhistory ORDER BY applied DESC LIMIT 1" % schema)
        row = cur.fetchone()
        if row == None:
            return None
        else:
            return row[0]
    except:
        dbconn.rollback()
        return None

## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Make sure we are connected to the expected db platform
# @param portid expected DB port id - to be validates
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def __check_db_port( portid):
    cur = dbconn.cursor()
    # PostgerSQL
    if portid == 'postgres':
        try:
            cur.execute( "SELECT version() AS version")        
            row = cur.fetchone()
            if row[0].lower().find( portid) >= 0 and row[0].lower().find( 'greenplum') < 0:
                return True
        except:
            dbconn.rollback()
    # Greenplum
    if portid == 'greenplum':
        try:
            cur.execute( "SELECT version() AS version")        
            row = cur.fetchone()
            if row[0].lower().find( portid) >= 0:
                return True
        except:
            dbconn.rollback()
    return False
                
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Convert version string into number for comparison
# @param rev version text
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def __get_rev_num( rev):
    try:
        num = re.findall( '[0-9]', rev)
        if num == []:
            return ['0']
        else:
            return num
    except:
        return ['0']

## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Print version information
# @param rev OS-level MADlib version
# @param dbrev DB-level MADlib version
# @param con_args database connection arguments
# @param schema MADlib schema name
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def __print_revs( rev, dbrev, con_args, schema):
    
    __info( "MADlib tools version    = %s (%s)" % (rev, sys.argv[0]), True)    
    if con_args:
        try:
            __info( "MADlib database version = %s (host=%s, db=%s, schema=%s)" 
                    % (dbrev, con_args['host'], con_args['database'], schema), True)
        except:          
            __info( "MADlib database version = [Unknown] (host=%s, db=%s, schema=%s)" 
                    % (dbrev, con_args['host'], con_args['database'], schema), True)      
    return

## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Check pl/python existence and version
# @param py_min_ver min Python version to run MADlib 
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def __plpy_check( py_min_ver):

    __info( "Testing PL/Python environment...", True)
    cur = dbconn.cursor()

    # Check PL/Python existence
    cur.execute( "SET client_min_messages=WARNING")
    cur.execute( "SELECT count(*) AS CNT FROM pg_language WHERE lanname = 'plpythonu'")
    rv = cur.fetchall()
    if rv[0][0] > 0:
        __info( "> PL/Python already installed", verbose)            
    else:
        __info( "> PL/Python not installed", verbose)            
        __info( "> Creating language PL/Python...", True)            
        try:
            cur.execute( "CREATE LANGUAGE plpythonu;")            
            dbconn.commit()
        except:
            __error( 'Cannot create language plpythonu. Stopping installation...', False)
            raise                

    # Check PL/Python version
    cur.execute( "SET client_min_messages=WARNING;")
    cur.execute( "DROP FUNCTION IF EXISTS plpy_version_for_madlib();")
    cur.execute( """CREATE OR REPLACE FUNCTION plpy_version_for_madlib() 
                    RETURNS TEXT AS 
                    $$
                        import sys
                        return '.'.join(str(item) for item in sys.version_info[:3])
                    $$
                    LANGUAGE plpythonu;""")
    cur.execute( "SELECT plpy_version_for_madlib() AS ver;")
    rv = cur.fetchall()
    py_cur_ver = [int(i) for i in rv[0][0].split('.')]
    if py_cur_ver >= py_min_ver:
        __info( "> PL/Python version: %s" % rv[0][0], verbose)            
    else:
        __error( "PL/Python version too old: %s. You need %s or greater" \
                % (rv[0][0], '.'.join(str(i) for i in py_min_ver)) 
                , False)            
        raise

    __info( "> PL/Python environment OK (version: %s)" % rv[0][0], True)            

## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Install MADlib
# @param schema MADlib schema name
# @param dbrev DB-level MADlib version
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def __db_install( schema, dbrev):

    __info( "Installing MADlib into %s schema..." % schema.upper(), True)
    
    temp_schema = schema + '_v' + ''.join(__get_rev_num( dbrev))
    schema_writable = None
    madlib_exists = None
    
    # Check the status of MADlib objects in database
    if dbrev != None:
        madlib_exists = True
    else:
        madlib_exists = False

    # Test if schema is writable
    try:
        cur = dbconn.cursor()
        cur.execute( "SET client_min_messages=WARNING;")
        cur.execute( "CREATE TABLE %s.__madlib_test_table (A INT);" % schema)
        cur.execute( "DROP TABLE %s.__madlib_test_table;" % schema)
        schema_writable = True
    except:
        dbconn.rollback()
        schema_writable = False
        
    # Some debugging
    # print 'madlib_exists = ' + str(madlib_exists)
    # print 'schema_writable = ' + str(schema_writable)
    
    ##
    # CASE #1: Target schema exists with MADlib objects:
    ##
    if schema_writable == True and madlib_exists == True:

        __info( "> Schema %s exists with MADlib objects" % schema.upper(), verbose)
    
        # Rename MADlib schema
        __db_rename_schema( schema, temp_schema)

        # Create MADlib schema
        try:
            __db_create_schema( schema)
        except:
            __db_rollback( schema, temp_schema)

        # Create MADlib objects
        try:
            __db_create_objects( schema, temp_schema)
        except:
            __db_rollback( schema, temp_schema)

    ##
    # CASE #2: Target schema exists w/o MADlib objects:
    ##
    elif schema_writable == True and madlib_exists == False:

        __info( "> Schema %s exists w/o MADlib objects" % schema.upper(), verbose)
        
        # Create MADlib objects
        try:
            __db_create_objects( schema, None)
        except:
            __error( "Building database objects failed. " +
                     "Before retrying: drop %s schema OR install MADlib into a different schema." % schema.upper(), True)

    ##
    # CASE #3: Target schema does not exist:
    ##
    elif schema_writable == False:

        __info( "> Schema %s does not exist" % schema.upper(), verbose)

        # Create MADlib schema
        try:
            __db_create_schema( schema)
        except:
            __db_rollback( schema, None)

        # Create MADlib objects
        try:
            __db_create_objects( schema, None)
        except:
            __db_rollback( schema, None)

    __info( "MADlib installed successfully in %s schema." % schema.upper(), True)
    
    
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Rename schema
# @param from_schema name of the schema to rename
# @param to_schema new name for the schema 
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def __db_rename_schema( from_schema, to_schema):

    __info( "> Renaming schema %s to %s" % (from_schema.upper(), to_schema.upper()), True)        
    try:
        cur = dbconn.cursor()
        cur.execute( "SET client_min_messages=WARNING;")
        cur.execute( "ALTER SCHEMA %s RENAME TO %s;" % (from_schema, to_schema))
        dbconn.commit();
    except:
        __error( 'Cannot rename schema. Stopping installation...', False)
        raise

## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Create schema
# @param from_schema name of the schema to rename
# @param to_schema new name for the schema 
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def __db_create_schema( schema):

    __info( "> Creating %s schema" % schema.upper(), True)        
    try:
        cur = dbconn.cursor()
        cur.execute( "SET client_min_messages=WARNING;")
        cur.execute( "CREATE SCHEMA " + schema + ";")
        cur.close()
        dbconn.commit();
    except:
        __info( 'Cannot create new schema. Rolling back installation...', True)
        pass

## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Create MADlib DB objects in the schema
# @param schema name of the target schema 
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def __db_create_objects( schema, old_schema):

    # Create MigrationHistory table
    try:
        __info( "> Creating %s.MigrationHistory table" % schema.upper(), True)
        cur = dbconn.cursor()
        cur.execute( "SET client_min_messages=WARNING;")
        cur.execute( "DROP TABLE IF EXISTS %s.migrationhistory" % schema)
        sql = """CREATE TABLE %s.migrationhistory 
               (id serial, version varchar(255), applied timestamp default current_timestamp);""" % schema
        cur.execute( sql)
        dbconn.commit();    
    except:
        __error( "Cannot crate MigrationHistory table", False)
        raise
    
    # Copy MigrationHistory table for record keeping purposes
    if old_schema:
        try:
            __info( "> Saving data from %s.MigrationHistory table" % old_schema.upper(), True)
            sql = """INSERT INTO %s.migrationhistory (version, applied) 
                   SELECT version, applied FROM %s.migrationhistory 
                   ORDER BY id;""" % (schema, old_schema)
            cur.execute( "SET client_min_messages=WARNING;")
            cur.execute( sql)
            dbconn.commit();    
        except:
            __error( "Cannot copy MigrationHistory table", False)
            raise

    # Stamp the DB installation
    try:
        __info( "> Writing version info in MigrationHistory table", True)
        cur = dbconn.cursor()
        cur.execute( "SET client_min_messages=WARNING;")
        cur.execute( "INSERT INTO %s.migrationhistory(version) VALUES('%s')" % (schema, rev))
        dbconn.commit();    
    except:
        __error( "Cannot insert data into %s.migrationhistory table" % schema, False)
        raise
    
    # Run migration SQLs    
    __info( "> Creating objects for modules:", True)  
    
    # Loop through all modules/modules  
    for moduleinfo in portspecs['modules']:   
     
        # Get the module name
        module = moduleinfo['name']
        __info("> - %s" % module, True)        
        
        # Make a temp dir for this module 
        cur_tmpdir = tmpdir + "/" + module
        __make_log_dir( cur_tmpdir)

        # Find the module dir (platform specific or generic)
        if os.path.isdir( maddir + "/ports/" + portid + "/modules/" + module):
            maddir_mod  = maddir + "/ports/" + portid + "/modules"
        else:        
            maddir_mod  = maddir + "/modules"

        # Loop through all SQL files for this module
        sql_files = maddir_mod + '/' + module + '/*.sql_in'
        for sqlfile in glob.glob( sql_files):
        
            # Set file names
            tmpfile = cur_tmpdir + '/' + os.path.basename(sqlfile) + '.tmp'
            logfile = cur_tmpdir + '/' + os.path.basename(sqlfile) + '.log'
            
            # Run the SQL
            try:
                retval = __db_run_sql( schema, maddir_mod, module, sqlfile, tmpfile, logfile)
            except:
                raise
                
            # If PSQL returned error
            if retval == 3:
                __error( "Failed executing: %s" % tmpfile, False)  
                __error( "For details check: %s" % logfile, False) 
                raise    

    
            # Check the output
            #log = open( logfile, 'r')
            #try:
            #    for line in log:
            #        if line.upper().find( 'ERROR') >= 0:
            #            print line,
            #            __error( "Problem running %s. Check %s for details." % (tmpfile, logfile), False)
            #            raise
            #finally:
            #    log.close() 

## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Run SQL file
# @param schema name of the target schema  
# @param maddir_mod name of the module dir with Python code
# @param module name of the module 
# @param sqlfile name of the file to parse  
# @param tmpfile name of the temp file to run
# @param logfile name of the log file (stdout)    
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def __db_run_sql( schema, maddir_mod, module, sqlfile, tmpfile, logfile):       

    # Check if the SQL file exists     
    if not os.path.isfile( sqlfile):
        __error("Missing module SQL file (%s)" % sqlfile, False)
        raise      
    
    # Prepare the file using M4
    try:
        f = open(tmpfile, 'w')
        
        # Find the madpack dir (platform specific or generic)
        if os.path.isdir( maddir + "/ports/" + portid + "/madpack"):
            maddir_madpack  = maddir + "/ports/" + portid + "/madpack"
        else:        
            maddir_madpack  = maddir + "/madpack"

        m4args = [ 'm4', 
                    '-P', 
                    '-DMADLIB_SCHEMA=' + schema, 
                    '-DPLPYTHON_LIBDIR=' + maddir_mod, 
                    '-DMODULE_PATHNAME=' + maddir_lib, 
                    '-DMODULE_NAME=' + module, 
                    '-I' + maddir_madpack,
                    '-D' + portid.upper(), 
                    sqlfile ]

        __info("> ... parsing: " + " ".join(m4args), verbose )
                    
        subprocess.call( m4args, stdout=f)  
        f.close()         
    except:
        __error("Failed executing m4 on %s" % sqlfile, False)
        raise

    # Run the SQL using DB command-line utility
    if portid == 'greenplum' or portid == 'postgres':
    
        if subprocess.call(['which', 'psql'], stdout=subprocess.PIPE, stderr=subprocess.PIPE) != 0:
            __error( "Can not find: psql", False)
            raise
            
        runcmd = [ 'psql', '-a',
                    '-v', 'ON_ERROR_STOP=1',
                    # '-v', 'CLIENT_MIN_MESSAGES=error',
                    '-h', con_args['host'].split(':')[0],
                    '-p', con_args['host'].split(':')[1],
                    '-d', con_args['database'],
                    '-U', con_args['user'],
                    '-f', tmpfile]
        runenv = os.environ
        runenv["PGPASSWORD"] = con_args['password']
    # Open log file
    try:
        log = open( logfile, 'w') 
    except:
        __error( "Cannot create log file %s" % logfile, False)
        raise    
    # Run the SQL
    try:
        __info("> ... executing " + tmpfile, verbose ) 
        retval = subprocess.call( runcmd , env=runenv, stdout=log, stderr=log) 
    except:            
        __error( "Failed executing %s." % tmpfile, False)  
        raise    
    finally:
        log.close()

    return retval

    # Run the SQL using DBAPI2 & SQLParse
    #import sqlparse
    #try:
    #    f = open(tmpfile)
    #    sqltext = "".join(f.readlines())
    #    stmts = sqlparse.split(sqltext)
    #    cur = dbconn.cursor()
    #    for s in stmts:
    #        if s.strip() != "":
    #            cur.execute( s.strip())
    #    dbconn.commit()            
    #except:
    #    __error( "Error while executing %s" % tmpfile, True)    
                       
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Rollback installation
# @param drop_schema name of the schema to drop
# @param keep_schema name of the schema to rename and keep
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def __db_rollback( drop_schema, keep_schema):

    __info( "Rolling back the installation...", True)

    if drop_schema == None:
        __error( 'No schema name to drop. Stopping rollback...', True)        

    # Drop the current schema
    __info( "> Dropping schema %s" % drop_schema.upper(), verbose)        
    try:
        cur = dbconn.cursor()
        cur.execute( "SET client_min_messages=WARNING;")
        cur.execute( "DROP SCHEMA %s CASCADE;" % (drop_schema))
        dbconn.commit();
    except:
        __error( "Cannot drop schema %s. Stopping rollback..." % drop_schema.upper(), True)        

    # Rename old to current schema
    if keep_schema != None:    
        __db_rename_schema( keep_schema, drop_schema)   

    __info( "Rollback finished OK.", True)
    raise
            

def unescape(string):
    """
    Unescape separation characters in connection strings, i.e., remove first
    backslash from "\/", "\@", "\:", and "\\".
    """
    if string is None:
        return None
    else:
        return re.sub(r'\\(?P<char>[/@:\\])', '\g<char>', string)

def parseConnectionStr(connectionStr):
    """
    @brief Parse connection strings of the form
           <tt>[username[/password]@][hostname][:port][/database]</tt>
    
    Separation characters (/@:) and the backslash (\) need to be escaped.
    @returns A tuple (username, password, hostname, port, database). Field not
             specified will be None.
    """
    match = re.search(
        r'((?P<user>([^/@:\\]|\\/|\\@|\\:|\\\\)+)' +
        r'(/(?P<password>([^/@:\\]|\\/|\\@|\\:|\\\\)*))?@)?' +
        r'(?P<host>([^/@:\\]|\\/|\\@|\\:|\\\\)+)?' +
        r'(:(?P<port>[0-9]+))?' + 
        r'(/(?P<database>([^/@:\\]|\\/|\\@|\\:|\\\\)+))?', connectionStr)
    return (
        unescape(match.group('user')),
        unescape(match.group('password')),
        unescape(match.group('host')),
        match.group('port'),
        unescape(match.group('database')))

## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Main       
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def main( argv):

    parser = argparse.ArgumentParser( description='MADlib package manager (' + rev + ')', 
                                      argument_default=False,
                                      formatter_class=argparse.RawTextHelpFormatter,
                                      epilog="""Example:
                                      
  $ madpack install -s madlib -p greenplum -c gpadmin@mdw:5432/testdb

  This will install MADlib objects into a Greenplum database called TESTDB 
  running on server MDW:5432. Installer will try to login as GPADMIN 
  and will prompt for password. The target schema will be MADLIB.
  
""")
            
    parser.add_argument( 'command', metavar='COMMAND', nargs=1, 
                         choices=['install','update','uninstall','version','install-check'], 
                         help = "One of the following options:\n"
                              + "  install/update : run sql scripts to load into DB\n"
                              + "  uninstall      : run sql scripts to uninstall from DB\n"
                              + "  version        : compare and print MADlib version (binaries vs database objects)\n"
                              + "  install-check  : test all installed modules\n")
  
    parser.add_argument( '-a', '--api', nargs=1, dest='api', metavar='API', 
                         help="DBAPI2 compliant Python module name")

    parser.add_argument( '-c', '--conn', metavar='CONNSTR', nargs=1, dest='connstr', default=None,
                         help= "Connection string of the following syntax:\n"
                             + "  [user[/password]@][host][:port][/database]\n" 
                             + "If not provided default values will be derived for PostgerSQL and Greenplum:\n"
                             + "- user: PGUSER or USER env variable or OS username\n"
                             + "- pass: PGPASSWORD env variable or runtime prompt\n"
                             + "- host: PGHOST env variable or 'localhost'\n"
                             + "- port: PGPORT env variable or '5432'\n"
                             + "- db: PGDATABASE env variable or OS username\n"
                             )

    parser.add_argument( '-s', '--schema', nargs=1, dest='schema', 
                         metavar='SCHEMA', default='madlib',
                         help="Target schema for the database objects.")
                         
    parser.add_argument( '-p', '--platform', nargs=1, dest='platform', 
                         metavar='PLATFORM', choices=portid_list,
                         help="Target database platform, current choices: " + str(portid_list))

    parser.add_argument( '-v', '--verbose', dest='verbose', 
                         action="store_true", help="Verbose mode.")

    ##
    # Get the arguments
    ##
    args = parser.parse_args()
    global verbose
    verbose = args.verbose
    __info( "Arguments: " + str(args), verbose);    

    ##
    # Parse DBAPI2 overwrite
    ##
    try:
        # Get the module name
        api = args.api[0] 
    except:
        api = None
    # And try importing it
    if api != None:
        try:
            dbapi2 = __import_dbapi( api)
            __info( "Imported user defined dbapi2 module (%s)." % (api), verbose)
        except:
            __error( "cannot import python module: %s. Try using the deafult one by skipping option -a/--api." % (args.api[0]), True)
        
    ##
    # Parse SCHEMA
    ##
    if len(args.schema[0]) > 1:
        schema = args.schema[0]
    else:    
        schema = args.schema

    ##
    # Parse DB Platform (== PortID) and compare with Ports.yml
    ##
    global portid  
    if args.platform:
        try:
            # Get the DB platform name == DB port id
            portid = args.platform[0].lower()
            # Loop through available db ports 
            for port in ports['ports']:                
                if portid == port['id']:
                    # Get the Python DBAPI2 module
                    portapi = port['dbapi2'] 
                    # Adjust MADlib directories for this port (if they exist)
                    global maddir_conf 
                    if os.path.isdir( maddir + "/ports/" + portid + "/config"):
                        maddir_conf = maddir + "/ports/" + portid + "/config"
                    global maddir_lib
                    if os.path.isfile( maddir + "/ports/" + portid + \
                            "/lib/libmadlib_" + portid + ".so"):
                        maddir_lib  = maddir + "/ports/" + portid + \
                            "/lib/libmadlib_" + portid + ".so"
                    # Get the list of modules for this port
                    global portspecs
                    portspecs = configyml.get_modules( maddir_conf) 
                    # print portspecs
        except:
            portid = None
            __error( "Can not find specs for port %s" % (args.platform[0]), True)
    else:
        portid = None
        portapi = None

    ##
    # If no API defined yet (try the default API - from Ports.yml)
    ##
    if api is None and portapi:
        try:
            dbapi2 = __import_dbapi( portapi)
            __info( "Imported dbapi2 module (%s) defined in Ports.yml." % (portapi), verbose)
        except:
            __error( "cannot import python module: %s. You can try importing another one using -a/--api option (see --help for details)." % (portapi), True)    

    ##
    # Parse CONNSTR (only if PLATFORM and DBAPI2 are defined)
    ##
    if portid and dbapi2:
        connStr = "" if args.connstr is None else args.connstr[0]
        (c_user, c_pass, c_host, c_port, c_db) = parseConnectionStr(connStr)
        
        # Find the default values for PG and GP
        if portid == 'postgres' or portid == 'greenplum':
            if c_user is None:
                c_user = os.environ.get('PGUSER', getpass.getuser())
            if c_pass is None:
                c_pass = os.environ.get('PGPASSWORD', None)
            if c_host is None:
                c_host = os.environ.get('PGHOST', 'localhost')
            if c_port is None:
                c_port = os.environ.get('PGPORT', '5432')
            if c_db is None:
                c_db = os.environ.get('PGDATABASE', c_user)
        
        ##
        # Try connecting to the database
        ##
        __info( "Testing database connection...", verbose)
        
        # Get password
        if c_pass is None:
            c_pass = getpass.getpass( "Password for user %s: " % c_user)

        # Set connection variables
        global con_args
        con_args['host'] = c_host + ':' + c_port
        con_args['database'] = c_db
        con_args['user'] = c_user
        con_args['password'] = c_pass    

        # Open connection
        global dbconn
        dbconn = dbapi2.connect( **con_args)
        __info( 'Database connection successful: %s@%s/%s' % (c_user, c_host + ':' + c_port, c_db), verbose)

        # Get MADlib version in DB
        dbrev = __get_madlib_dbver( schema)
        
        # Validate that db platform is correct 
        if __check_db_port( portid) == False:
            __error( "invalid database platform selected", True)
                   
        # Close connection
        dbconn.close()
        
    else:
        con_args = None
        dbrev = None
        
    ##
    # Parse COMMAND argument and compare with Ports.yml
    ## 

    # Debugging...
    # print "OS rev: " + str(rev) + " > " + str(__get_rev_num( rev))
    # print "DB rev: " + str(dbrev) + " > " + str(__get_rev_num( dbrev))        

    ###
    # COMMAND: version
    ###
    if args.command[0] == 'version':

        __print_revs( rev, dbrev, con_args, schema)
        
    ###
    # COMMAND: install/update
    ###
    elif args.command[0] == 'install' or args.command[0] == 'update':

        if not portid: 
            __error( "Missing -p/--platform parameter.", True)

        if not con_args:
            __error( "Unknown problem with database connection string: %s" % con_args, True)
        
        # 1) Compare OS and DB versions. Continue if OS > DB.
        __print_revs( rev, dbrev, con_args, schema)
        if __get_rev_num( dbrev) >= __get_rev_num( rev):
            __info( "Current MADlib version already up to date.", True)
            return

        # 2) Run installation 
        dbconn = dbapi2.connect( **con_args)
        try:
            __plpy_check( py_min_ver)
            __db_install( schema, dbrev)
        except:
            __error( "MADlib installation failed.", True)
        dbconn.close()     

    ###
    # COMMAND: uninstall (drops the schema)
    ###
    if args.command[0] == 'uninstall':

        # 1) Check versions and confirm deletion
        # __print_revs( rev, dbrev, con_args, schema)

        if __get_rev_num( dbrev) == ['0']:
            __info( "Nothing to uninstall. No version found in schema %s." % schema.upper(), True)
            return

        __info( "***************************************************************************", True)
        __info( "* Schema %s and all database objects depending on it will be dropped!" % schema.upper(), True)
        __info( "* This is potentially very dangerous operation!                      ", True)
        __info( "***************************************************************************", True)
        __info( "Would you like to continue? [Y/N]", True)
        go = raw_input( '>>> ').upper()
        while go != 'Y' and go != 'N':
            go = raw_input( 'Yes or No >>> ').upper()
        
        # 2) Do the uninstall/drop     
        if go == 'N':            
            __info( 'No problem. Nothing dropped.', True)
            return
            
        elif go == 'Y':
            dbconn = dbapi2.connect( **con_args)
            __info( "> dropping schema %s" % schema.upper(), verbose)        
            try:
                cur = dbconn.cursor()
                cur.execute( "SET client_min_messages=WARNING;")
                cur.execute( "DROP SCHEMA %s CASCADE;" % (schema))
                dbconn.commit();
            except:
                __error( "Cannot drop schema %s." % schema.upper(), True)         
            dbconn.close() 
            __info( 'Schema %s (and all dependent objects) has been dropped.' % schema.upper(), True)
            __info( 'MADlib is uninstalled.', True)
            
        else:
            return
           
    ###
    # COMMAND: install-check
    ###
    if args.command[0] == 'install-check':
    
        # 1) Compare OS and DB versions. Continue if OS = DB.
        if __get_rev_num( dbrev) != __get_rev_num( rev):
            __print_revs( rev, dbrev, con_args, schema)
            __info( "Versions do not match. Install-check stopped.", True)
            return
         
        # 2) Run test SQLs 
        __info( "> Running test scripts for:", verbose)   
        
        # Loop through all modules 
        for moduleinfo in portspecs['modules']:    
        
            # Get module name
            module = moduleinfo['name']
            __info("> - %s" % module, verbose)        

            # Make a temp dir for this module (if doesn't exist)
            cur_tmpdir = tmpdir + '/' + module + '/test'
            __make_log_dir( cur_tmpdir)
            
            # Find the module dir (platform specific or generic)
            if os.path.isdir( maddir + "/ports/" + portid + "/modules/" + module):
                maddir_mod  = maddir + "/ports/" + portid + "/modules"
            else:        
                maddir_mod  = maddir + "/modules"      
    
            # Loop through all test SQL files for this module
            sql_files = maddir_mod + '/' + module + '/test/*.sql_in'
            for sqlfile in glob.glob( sql_files):
            
                # Set file names
                tmpfile = cur_tmpdir + '/' + os.path.basename(sqlfile) + '.tmp'
                logfile = cur_tmpdir + '/' + os.path.basename(sqlfile) + '.log'
                            
                # Run the SQL
                run_start = datetime.datetime.now()
                retval = __db_run_sql( schema, maddir_mod, module, sqlfile, tmpfile, logfile)
                # Runtime evaluation
                run_end = datetime.datetime.now()
                milliseconds = round( (run_end - run_start).seconds * 1000 + (run_end - run_start).microseconds / 1000)
        
                # Analyze the output
                # If DB cmd line returned error
                if retval == 3:
                    __error( "Failed executing: %s" % tmpfile, False)  
                    __error( "For details check: %s" % logfile, False) 
                    result = 'FAIL'
                # If error log file exists
                elif os.path.getsize( logfile) > 0:
                    result = 'PASS'
                # Otherwise                   
                else:
                    result = 'ERROR'
                
                # Spit the line
                print "TEST CASE RESULT|Module: " + module + \
                    "|" + os.path.basename(sqlfile) + "|" + result + \
                    "|Time: %d milliseconds" % (milliseconds)           
    
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Start Here
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
if __name__ == "__main__":

    # Open the log
    __make_log_dir( tmpdir)
    try:
        log = open( logfile, 'w')
    except:
        print "ERROR: can not create log file: %s." % logfile
        exit(1)   

    # Run main
    try:
        main(sys.argv[1:])
    finally:
        # Close the log
        __log_close()
    