#!/usr/bin/python
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Main Madpack installation executable.
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
import sys 
import argparse
import getpass
import re
import os
import traceback
import subprocess
import datetime
from time import strftime

# Import madpack modlues. This file is installed to
# $MADLIB_ROOT/python/madpack/madpack.py, so to get $MADLIB_ROOT we need to go
# two levels up in the directory hierarchy. We use (a) os.path.realpath and
# (b) __file__ (instead of sys.argv[0]) because madpack.py could be called
# (a) through a symbolic link and (b) not as the main module.
maddir = os.path.abspath(os.path.dirname(os.path.realpath(
    __file__)) + "/../..")   # MADlib root dir
sys.path.append( maddir + "/python")
sys.path.append( maddir + "/python/ext")
from madpack import configyml

# Some read-only variables
this =  os.path.basename(sys.argv[0])   # name of this script

# Default directories
maddir_conf = maddir + "/config"           # Config dir
maddir_py   = maddir + "/python/methods"   # Python methods  
maddir_lib  = maddir + "/lib/libmadlib.so" # C/C++ libraries 
maddir_sql  = maddir + "/sql"              # SQL create files 
maddir_test = maddir + "/sql/test"         # Test SQL files 

# Tempdir
tmpdir = '/tmp/madlib'
if not os.path.isdir( tmpdir):
    try:
        os.mkdir( tmpdir)
    except:
        __error( 'Can not create temp dir: %s' % tmpdir, True)

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
logfile = tmpdir + '/' + "%s-%s.log" % ('madlib', strftime( "%Y%m%d-%H%M%S"))
log = open( logfile, 'w')

## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Close the log file 
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def __log_close():
    try:
        __info( "Log saved in " + logfile, verbose)
        log.close()            
    except:
        pass            

## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Error message wrapper 
# @param msg error message
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
        __log_close()
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
#def __get_madlib_dbver( dbconn, schema):
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
        cur.execute( "ALTER SCHEMA %s RENAME TO %s;" % (from_schema, to_schema))
        dbconn.commit();
    except:
        __error( 'Could not rename schema. Stopping execution...', False)
        raise Exception

## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Create schema
# @param from_schema name of the schema to rename
# @param to_schema new name for the schema 
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def __db_create_schema( schema):

    __info( "> Creating %s schema" % schema.upper(), True)        
    try:
        cur = dbconn.cursor()
        cur.execute( "CREATE SCHEMA " + schema + ";")
        cur.close()
        dbconn.commit();
    except:
        __info( 'Could not create new schema. Rolling back installation...', True)
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
        cur.execute( "DROP TABLE IF EXISTS %s.migrationhistory" % schema)
        sql = """CREATE TABLE %s.migrationhistory 
               (id serial, version varchar(255), applied timestamp default current_timestamp);""" % schema
        cur.execute( sql)
        dbconn.commit();    
    except:
        __error( "could not crate MigrationHistory table", False)
        raise Exception
    
    # Copy MigrationHistory table for record keeping purposes
    if old_schema:
        try:
            __info( "> Saving data from %s.MigrationHistory table" % old_schema.upper(), True)
            sql = """INSERT INTO %s.migrationhistory (version, applied) 
                   SELECT version, applied FROM %s.migrationhistory 
                   ORDER BY id;""" % (schema, old_schema)
            cur.execute( sql)
            dbconn.commit();    
        except:
            __error( "Could not copy MigrationHistory table", False)
            raise Exception

    # Stamp the DB installation
    try:
        __info( "> Writing version info in MigrationHistory table", True)
        cur = dbconn.cursor()
        cur.execute( "INSERT INTO %s.migrationhistory(version) VALUES('%s')" % (schema, rev))
        dbconn.commit();    
    except:
        __error( "Could not insert data into %s.migrationhistory table" % schema, False)
        raise Exception
    
    # Run migration SQLs    
    __info( "> Creating objects for:", True)  
    
    # Loop through all methods  
    for methodinfo in portspecs['methods']:   
     
        # Get the method name
        method = methodinfo['name']
        __info("> - %s" % method, True)        
        
        # Set file names
        sqlfile = maddir_sql + '/' + method + '.sql_in'
        tmpfile = tmpdir + '/' + method + '.tmp.sql'
        logfile = tmpdir + '/' + method + '.sql.log'
        
        # Run the SQL
        try:
            __db_run_sql( schema, sqlfile, tmpfile, logfile)
        except:
            raise Exception

        # Check the output
        log = open( logfile, 'r')
        try:
            for line in log:
                if line.upper().find( 'ERROR') >= 0:
                    print line,
                    __error( "Problem running %s. Check %s for details." % (tmpfile, logfile), False)
                    raise Exception
        finally:
            log.close() 

## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Run SQL file
# @param method name of the method  
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def __db_run_sql( schema, sqlfile, tmpfile, logfile):       

        # Check if the SQL file exists     
        if not os.path.isfile( sqlfile):
            __error("Missing method SQL file (%s)" % sqlfile, False)
            raise      
        
        # Prepare the file using M4
        try:
            __info("> executing " + os.path.basename(tmpfile), verbose )                
            f = open(tmpfile, 'w')
            m4args = [ 'm4', 
                       '-P', 
                       '-DMADLIB_SCHEMA=' + schema, 
                       '-DPLPYTHON_LIBDIR=' + maddir_py, 
                       '-DMODULE_PATHNAME=' + maddir_lib, 
                       '-D' + portid.upper(), 
                       sqlfile ]
            subprocess.call(m4args, stdout=f)   
            f.close()         
        except:
            __error("Failed executing m4 on %s" % sqlfile, False)
            raise Exception

        # Run the SQL using DB command-line utility
        if portid.upper() == 'GREENPLUM' or portid.upper() == 'POSTGRES':
        
            if subprocess.call(['which', 'psql'], stdout=subprocess.PIPE, stderr=subprocess.PIPE) != 0:
                __error( "Can not find: psql", False)
                raise Exception
                
            runcmd = [ 'psql', '-a',
                       '-h', con_args['host'].split(':')[0],
                       '-p', con_args['host'].split(':')[1],
                       '-d', con_args['database'],
                       '-U', con_args['user'],
                       '-f', tmpfile]
            runenv = os.environ
            runenv["PGPASSWORD"] = con_args['password']
        try:
            log = open( logfile, 'w') 
            subprocess.call( runcmd , env=runenv, stdout=log, stderr=log)          
        except:
            __error( "Could not execute %s" % tmpsql, False)
            raise Exception    
        finally:
            log.close()

        # Run the SQL using DBAPI2 & SQLParse
        #import sqlparse
        #try:
        #    f = open(tmpsql)
        #    sqltext = "".join(f.readlines())
        #    stmts = sqlparse.split(sqltext)
        #    cur = dbconn.cursor()
        #    for s in stmts:
        #        if s.strip() != "":
        #            cur.execute( s.strip())
        #    dbconn.commit()            
        #except:
        #    __error( "Error while executing %s" % tmpsql, True)    
                       
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
        cur.execute( "DROP SCHEMA %s CASCADE;" % (drop_schema))
        dbconn.commit();
    except:
        __error( "Could not drop schema %s. Stopping rollback..." % drop_schema.upper(), True)        

    # Rename old to current schema
    if keep_schema != None:    
        __db_rename_schema( keep_schema, drop_schema)   

    __info( "Rollback finished. Stopping execution.", True)
    __error( "Installation failed. ", True) 
            
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
                         help="""One of the following options: 
  install/update : run sql scripts to load into DB
  uninstall      : run sql scripts to uninstall from DB
  version        : compare and print MADlib version (binaries vs database objects)
  install-check  : test all installed methods""")
  
    parser.add_argument( '-a', '--api', nargs=1, dest='api', metavar='API', 
                         help="Python module for your database platform (DBAPI2 compliant)")

    parser.add_argument( '-c', '--conn', metavar='CONNSTR', nargs=1, dest='connstr', default=None,
                         help="connection string of the following syntax: user[/pass]@host:port/dbname")

    parser.add_argument( '-s', '--schema', nargs=1, dest='schema', 
                         metavar='SCHEMA', default='madlib',
                         help="target schema for the database objects")
                         
    parser.add_argument( '-p', '--platform', nargs=1, dest='platform', 
                         metavar='PLATFORM', choices=portid_list,
                         help="target database platform, current choices: " + str(portid_list))

    parser.add_argument( '-v', '--verbose', dest='verbose', 
                         action="store_true", help="more output")

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
            __info( "Using provided dbapi2 module (%s)." % (api), verbose)
        except:
            __error( "cannot import dbapi2 module: %s. Try using the deafult one from Ports.yml file." % (args.api[0]), True)
        
    ##
    # Parse SCHEMA
    ##
    if len(args.schema[0]) > 1:
        schema = args.schema[0]
    else:    
        schema = args.schema

    ##
    # Parse DB PLATFORM and compare with Ports.yml
    ##
    if args.platform:
        try:
            platform = args.platform[0]
            # Loop through available db ports 
            for port in ports['ports']:                
                if args.platform[0] == port['id']:
                    # Get the DB id
                    platform = args.platform
                    # Get the Python DBAPI2 module
                    portapi = port['dbapi2'] 
                    global portid  
                    portid = port['id']
                    # Adjust MADlib directories for this port (if exist)
                    global maddir_conf, maddir_py, maddir_lib, maddir_sql, maddir_test
                    if os.path.isdir( maddir + "/ports/" + port['id'] + "/config"):
                        maddir_conf = maddir + "/ports/" + port['id'] + "/config"
                    if os.path.isdir( maddir + "/ports/" + port['id'] + "/python"):
                        maddir_py   = maddir + "/ports/" + port['id'] + "/python"
                    if os.path.isdir( maddir + "/ports/" + port['id'] + 
                            "/lib/libmadlib_" + port['id'] + ".so"):
                        maddir_lib  = maddir + "/ports/" + port['id'] +
                            "/lib/libmadlib_" + port['id'] + ".so"
                    if os.path.isdir( maddir + "/ports/" + port['id'] + "/sql"):
                        maddir_sql  = maddir + "/ports/" + port['id'] + "/sql"
                    if os.path.isdir( maddir + "/ports/" + port['id'] + "/sql/test"):
                        maddir_test = maddir + "/ports/" + port['id'] + "/sql/test"
                    # Get the list of methods for this port
                    global portspecs
                    portspecs = configyml.get_methods( maddir_conf) 
                    # print portspecs
        except:
            platform = None
            __error( "Can not find specs for port %s" % (args.platform[0]), True)
    else:
        platform = None
        portapi = None

    ##
    # If no API defined yet (try the default API - from Ports.yml)
    ##
    if api == None and portapi != None:
        try:       
            dbapi2 = __import_dbapi( portapi)
            __info( "Using dbapi module (%s) defined in Ports.yml." % (portapi), verbose)
        except:
            __error( "cannot import dbapi2 module: %s. You can try specifying a different one (see --help)." % (portapi), True)

    ##
    # Parse CONNSTR (only if PLATFORM and DBAPI2 are defined)
    ##
    if platform != None and dbapi2 and args.connstr != None:

        try:
            (c_user, c_dsn) = args.connstr[0].split('@')
        except:
            __error( "invalid connection string: missing '@' separator (see '-h' for help)", True)
    
        try: 
            (c_user, c_pass) = c_user.split('/')
        except:
            pass_prompt = True
            c_pass = None
            
        if c_user == '':
            __error( "invalid connection string: missing 'user' parameter (see '-h' for help)", True) 
            
        try: 
            (c_dsn, c_db) = c_dsn.split('/')
        except:
            __error( "invalid connection string: missing '/' separator (see '-h' for help)", True)
    
        try: 
            (c_host, c_port) = c_dsn.split(':')
        except:
            __error( "invalid connection string: missing ':' separator (see '-h' for help)", True)
    
        ##
        # Try connecting to the database
        ##
        __info( "Testing database connection...", verbose)
        # get password
        if c_pass == None:
            c_pass = getpass.getpass( "Password for user %s: " % c_user)
        # set connection variables
        global con_args
        con_args['host'] = c_dsn
        con_args['database'] = c_db
        con_args['user'] = c_user
        con_args['password'] = c_pass    
        # Open connection
        global dbconn
        dbconn = dbapi2.connect( **con_args)
        __info( 'Database connection successful: %s@%s/%s' % (c_user, c_dsn, c_db), verbose)
        # Get MADlib version in DB
        dbrev = __get_madlib_dbver( schema)
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

        # 1) Compare OS and DB versions. Continue if OS > DB.
        __print_revs( rev, dbrev, con_args, schema)
        if __get_rev_num( dbrev) >= __get_rev_num( rev):
            __info( "Current MADlib version already up to date.", True)
            return

        # 2) Run installation 
        dbconn = dbapi2.connect( **con_args)
        __db_install( schema, dbrev)
        dbconn.close()     

    ###
    # COMMAND: uninstall (drops the schema)
    ###
    if args.command[0] == 'uninstall':

        # 1) Check versions and confirm deletion
        __print_revs( rev, dbrev, con_args, schema)

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
            __info( 'No problem. Nothing droped.', True)
            return
            
        elif go == 'Y':
            dbconn = dbapi2.connect( **con_args)
            __info( "> dropping schema %s" % schema.upper(), verbose)        
            try:
                cur = dbconn.cursor()
                cur.execute( "DROP SCHEMA %s CASCADE;" % (schema))
                dbconn.commit();
            except:
                __error( "Could not drop schema %s." % schema.upper(), True)         
            dbconn.close() 
            __info( 'Schema %s has been dropped. MADlib uninstalled.' % schema.upper(), True)
            
        else:
            return
           
    ###
    # COMMAND: install-check
    ###
    if args.command[0] == 'install-check':
    
        # 1) Compare OS and DB versions. Continue if OS = DB.
        if __get_rev_num( dbrev) != __get_rev_num( rev):
            __info( "Versions do not match. Install-check stopped.", True)
            return
         
        # 2) Run test SQLs 
        __info( "> Running test scripts for:", verbose)   
        
        # Loop through all methods 
        for methodinfo in portspecs['methods']:    
        
            # Get method name
            method = methodinfo['name']
            __info("> - %s" % method, verbose)        
            
            # Loop through all test files for each method
            for file in os.listdir( maddir_test):
                if file.find( method) == 0:
                
                    # Set file names
                    sqlfile = maddir_test + '/' + file
                    tmpfile = tmpdir + '/' + file + '.tmp'
                    logfile = tmpdir + '/' + file + '.log'
                    
                    # Run the SQL
                    run_start = datetime.datetime.now()
                    __db_run_sql( schema, sqlfile, tmpfile, logfile)
                    # Runtime evaluation
                    run_end = datetime.datetime.now()
                    seconds = (run_end - run_start).seconds
                    microsec = (run_end - run_start).microseconds
            
                    # Check the output
                    log = open( logfile, 'r')
                    result = 'PASS'
                    try:
                        for line in log:
                            if line.upper().find( 'ERROR') >= 0:
                                result = 'FAIL'
                    except:
                        result = 'ERROR'
                    finally:
                        log.close()                     
                    
                    # Spit the line
                    print "TEST CASE RESULT|MADlib method: " + method + "\
|" + file + "|" + result + "|Time: %s.%s sec" % (seconds, microsec)           
    
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Start Here
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
if __name__ == "__main__":

    # Run main
    main(sys.argv[1:])

    # Close the log
    __log_close()
    