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
import tempfile
import shutil

# Required Python version
py_min_ver = [2,6]

# Check python version
if sys.version_info[:2] < py_min_ver:
    print "ERROR: python version too old (%s). You need %s or greater." \
          % ('.'.join(str(i) for i in sys.version_info[:3]), '.'.join(str(i) for i in py_min_ver))
    exit(1)

# Find MADlib root directory. This file is installed to
# $MADLIB_ROOT/madpack/madpack.py, so to get $MADLIB_ROOT we need to go
# two levels up in the directory hierarchy. We use (a) os.path.realpath and
# (b) __file__ (instead of sys.argv[0]) because madpack.py could be called
# (a) through a symbolic link and (b) not as the main module.
maddir = os.path.abspath(os.path.dirname(os.path.realpath(
    __file__)) + "/..")   # MADlib root dir
sys.path.append(maddir + "/madpack")

# Import MADlib python modules
import argparse
import configyml

# Some read-only variables
this = os.path.basename(sys.argv[0])    # name of this script

# Default directories
maddir_conf = maddir + "/config"           # Config dir
maddir_lib  = maddir + "/lib/libmadlib.so" # C/C++ libraries 

# Read the config files
ports = configyml.get_ports(maddir_conf )  # object made of Ports.yml
rev = configyml.get_version(maddir_conf )  # MADlib OS-level version
portid_list = []
for port in ports:
    portid_list.append(port)

# Global variables
portid = None       # Target port ID (eg: pg90, gp40)
dbconn = None       # DB Connection object
dbver = None        # DB version
con_args = {}       # DB connection arguments
verbose = None      # Verbose flag

## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Create a temp dir 
# @param dir temp directory path
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def __make_dir(dir):
    if not os.path.isdir(dir):
        try:
            os.makedirs(dir)
        except:
            print "ERROR: can not create directory: %s. Check permissions." % dir
            exit(1)    

## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Error message wrapper 
# @param msg error message
# @param stop program exit flag
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def __error(msg, stop):
    # Print to stdout
    print this + ' : ERROR : ' + msg
    # Print stack trace
    if stop == True:
        exit(2)

#class MadpackError(Exception):
#    pass

## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Info message wrapper (verbose)
# @param msg info message
# @param verbose prints only if True
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def __info(msg, verbose):
    # Print to stdout
    if verbose:
        print this + ' : INFO : ' + msg

## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Runs a SQL query on the target platform DB
# using the default command-line utility.
# Very limited: 
#   - no text output with "new line" characters allowed
# @param sql query text to execute
# @param show_error displays the SQL error msg
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def __run_sql_query(sql, show_error):

    # Postgres & Greenplum
    if portid == 'greenplum' or portid == 'postgres':
    
        # Define sqlcmd
        sqlcmd = 'psql'
        delimiter = '|'
        
        # Test the DB cmd line utility
        std, err = subprocess.Popen(['which', sqlcmd], stdout=subprocess.PIPE, stderr=subprocess.PIPE).communicate()
        if std == '':
            __error("Command not found: %s" % sqlcmd, True)
        
        # Run the query
        runcmd = [ sqlcmd,
                    '-h', con_args['host'].split(':')[0],
                    '-p', con_args['host'].split(':')[1],
                    '-d', con_args['database'],
                    '-U', con_args['user'],
                    '-F', delimiter,
                    '-Ac', "set CLIENT_MIN_MESSAGES=error; " + sql]
        runenv = os.environ
        runenv["PGPASSWORD"] = con_args['password']
        std, err = subprocess.Popen(runcmd, env=runenv, stdout=subprocess.PIPE, stderr=subprocess.PIPE).communicate()
        if err:
            if show_error:
                __error("SQL command failed: \nSQL: %s \n%s" % (sql, err), False)
            raise Exception
        
        # Convert the delimited output into a dictionary
        results = [] # list of rows
        i = 0
        for line in std.splitlines():
            if i == 0:
                cols = [name for name in line.split(delimiter)]
            else:
                row = {} # dict of col_name:col_value pairs
                c = 0
                for val in line.split(delimiter):
                    row[cols[c]] = val
                    c += 1
                results.insert(i,row)
            i += 1
        # Drop the last line: "(X rows)"
        try:
            results.pop()
        except:
            pass
    
    return results

## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Run SQL file
# @param schema name of the target schema  
# @param maddir_mod_py name of the module dir with Python code
# @param module name of the module 
# @param sqlfile name of the file to parse  
# @param tmpfile name of the temp file to run
# @param logfile name of the log file (stdout)    
# @param pre_sql optional SQL to run before executing the file
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def __run_sql_file(schema, maddir_mod_py, module, sqlfile, tmpfile, logfile, pre_sql):       

    # Check if the SQL file exists     
    if not os.path.isfile(sqlfile):
        __error("Missing module SQL file (%s)" % sqlfile, False)
        raise Exception      
    
    # Prepare the file using M4
    try:
        f = open(tmpfile, 'w')
        
        # Add the before SQL
        if pre_sql:
            f.writelines([pre_sql, '\n\n'])
            f.flush()
        
        # Find the madpack dir (platform specific or generic)
        if os.path.isdir(maddir + "/ports/" + portid + "/" + dbver + "/madpack"):
            maddir_madpack  = maddir + "/ports/" + portid + "/" + dbver + "/madpack"
        else:        
            maddir_madpack  = maddir + "/madpack"

        m4args = [ 'm4', 
                    '-P', 
                    '-DMADLIB_SCHEMA=' + schema, 
                    '-DPLPYTHON_LIBDIR=' + maddir_mod_py, 
                    '-DMODULE_PATHNAME=' + maddir_lib, 
                    '-DMODULE_NAME=' + module, 
                    '-I' + maddir_madpack,
                    '-D' + portid.upper(), 
                    sqlfile ]

        __info("> ... parsing: " + " ".join(m4args), verbose )
                    
        subprocess.call(m4args, stdout=f)  
        f.close()         
    except:
        __error("Failed executing m4 on %s" % sqlfile, False)
        raise Exception

    # Run the SQL using DB command-line utility
    if portid == 'greenplum' or portid == 'postgres':

        # Define sqlcmd
        sqlcmd = 'psql'
        
        # Test the DB cmd line utility
        std, err = subprocess.Popen(['which', sqlcmd], stdout=subprocess.PIPE, stderr=subprocess.PIPE).communicate()
        if std == '':
            __error("Command not found: %s" % sqlcmd, True)
                            
        runcmd = [ sqlcmd, '-a',
                    '-v', 'ON_ERROR_STOP=1',
                    '-h', con_args['host'].split(':')[0],
                    '-p', con_args['host'].split(':')[1],
                    '-d', con_args['database'],
                    '-U', con_args['user'],
                    '-f', tmpfile]
        runenv = os.environ
        runenv["PGPASSWORD"] = con_args['password']
        
    # Open log file
    try:
        log = open(logfile, 'w') 
    except:
        __error("Cannot create log file: %s" % logfile, False)
        raise Exception    
        
    # Run the SQL
    try:
        __info("> ... executing " + tmpfile, verbose ) 
        retval = subprocess.call(runcmd , env=runenv, stdout=log, stderr=log) 
    except:            
        __error("Failed executing %s" % tmpfile, False)  
        raise Exception    
    finally:
        log.close()

    return retval

## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Read MADlib version from database
# @param dbconn database conection object
# @param schema MADlib schema name
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def __get_madlib_dbver(schema):
    try:
        row = __run_sql_query("""SELECT count(*) AS cnt FROM pg_tables 
            WHERE schemaname='%s' AND tablename='migrationhistory'""" % (schema), True)
        if int(row[0]['cnt']) > 0:
            row = __run_sql_query("""SELECT version FROM %s.migrationhistory 
                ORDER BY applied DESC LIMIT 1""" % schema, True)
            if row:
                return row[0]['version']
    except:
        __error("Failed reading MADlib db version", True)
        
    return None

## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Read version number from database (of form X.Y)
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def __get_dbver():
    try:
        versionStr = __run_sql_query("""SELECT version()""",
            True)[0]['version']
        if portid == 'postgres':
            match = re.search("PostgreSQL[a-zA-Z\s]*(\d+\.\d+)",
                versionStr)
        elif portid == 'greenplum':
            match = re.search("Greenplum[a-zA-Z\s]*(\d+\.\d+)",
                versionStr)
        return None if match is None else match.group(1)
    except:
        __error("Failed reading database version", True)

## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Make sure we are connected to the expected DB platform
# @param portid expected DB port id - to be validates
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def __check_db_port(portid):
    # Postgres
    if portid == 'postgres':
        try:
            row = __run_sql_query("SELECT version() AS version", True)  
        except:
            __error("Cannot validate DB platform type", True)
        if (row[0]['version'].lower().find(portid) >= 0 and 
            row[0]['version'].lower().find('greenplum') < 0):
            return True
    # Greenplum
    if portid == 'greenplum':
        try:
            row = __run_sql_query("SELECT version() AS version", True)  
        except:
            __error("Cannot validate DB platform type", True)
        if row[0]['version'].lower().find(portid) >= 0:
            return True            
    return False
                
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Convert version string into number for comparison
# @param rev version text
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def __get_rev_num(rev):
    try:
        num = re.findall('[0-9]', rev)
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
def __print_revs(rev, dbrev, con_args, schema):
    
    __info("MADlib tools version    = %s (%s)" % (rev, sys.argv[0]), True)    
    if con_args:
        try:
            __info("MADlib database version = %s (host=%s, db=%s, schema=%s)" 
                    % (dbrev, con_args['host'], con_args['database'], schema), True)
        except:          
            __info("MADlib database version = [Unknown] (host=%s, db=%s, schema=%s)" 
                    % (dbrev, con_args['host'], con_args['database'], schema), True)      
    return

## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Check pl/python existence and version
# @param py_min_ver min Python version to run MADlib 
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def __plpy_check(py_min_ver):

    __info("Testing PL/Python environment...", True)

    # Check PL/Python existence
    rv = __run_sql_query("SELECT count(*) AS CNT FROM pg_language WHERE lanname = 'plpythonu'", True)
    if int(rv[0]['cnt']) > 0:
        __info("> PL/Python already installed", verbose)            
    else:
        __info("> PL/Python not installed", verbose)            
        __info("> Creating language PL/Python...", True)            
        try:
            __run_sql_query("CREATE LANGUAGE plpythonu;", True)
        except:
            __error('Cannot create language plpythonu. Stopping installation...', False)
            raise Exception                

    # Check PL/Python version
    __run_sql_query("DROP FUNCTION IF EXISTS plpy_version_for_madlib();", False)
    __run_sql_query("""
        CREATE OR REPLACE FUNCTION plpy_version_for_madlib() 
        RETURNS TEXT AS 
        $$
            import sys
            # return '.'.join(str(item) for item in sys.version_info[:3])
            return str(sys.version_info[:3]).replace(',','.').replace(' ','').replace(')','').replace('(','')
        $$
        LANGUAGE plpythonu;
    """, True)
    rv = __run_sql_query("SELECT plpy_version_for_madlib() AS ver;", True)
    python = rv[0]['ver']
    py_cur_ver = [int(i) for i in python.split('.')]
    if py_cur_ver >= py_min_ver:
        __info("> PL/Python version: %s" % python, verbose)            
    else:
        __error("PL/Python version too old: %s. You need %s or greater" \
                % (python, '.'.join(str(i) for i in py_min_ver)) 
                , False)            
        raise Exception

    __info("> PL/Python environment OK (version: %s)" % python, True)            

## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Install MADlib
# @param schema MADlib schema name
# @param dbrev DB-level MADlib version
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def __db_install(schema, dbrev):

    __info("Installing MADlib into %s schema..." % schema.upper(), True)
    
    temp_schema = schema + '_v' + ''.join(__get_rev_num(dbrev))
    schema_writable = None
    madlib_exists = None
    
    # Check the status of MADlib objects in database
    if dbrev != None:
        madlib_exists = True
    else:
        madlib_exists = False

    # Test if schema is writable
    try:
        __run_sql_query("CREATE TABLE %s.__madlib_test_table (A INT);" % schema, False)
        __run_sql_query("DROP TABLE %s.__madlib_test_table;" % schema, False)
        schema_writable = True
    except:
        schema_writable = False
        
    # Some debugging
    # print 'madlib_exists = ' + str(madlib_exists)
    # print 'schema_writable = ' + str(schema_writable)
    
    ##
    # CASE #1: Target schema exists with MADlib objects:
    ##
    if schema_writable == True and madlib_exists == True:

        __info("***************************************************************************", True)
        __info("* Schema %s already exists" % schema.upper(), True)
        __info("* Installer will rename it to %s" % temp_schema.upper(), True)
        __info("***************************************************************************", True)
        __info("Would you like to continue? [Y/N]", True)
        go = raw_input('>>> ').upper()
        while go != 'Y' and go != 'N':
            go = raw_input('Yes or No >>> ').upper()
        if go == 'N':            
            __info('Installation stopped.', True)
            return
    
        # Rename MADlib schema
        __db_rename_schema(schema, temp_schema)

        # Create MADlib schema
        try:
            __db_create_schema(schema)
        except:
            __db_rollback(schema, temp_schema)

        # Create MADlib objects
        try:
            __db_create_objects(schema, temp_schema)
        except:
            __db_rollback(schema, temp_schema)

    ##
    # CASE #2: Target schema exists w/o MADlib objects:
    ##
    elif schema_writable == True and madlib_exists == False:

        __info("> Schema %s exists w/o MADlib objects" % schema.upper(), verbose)
        
        # Create MADlib objects
        try:
            __db_create_objects(schema, None)
        except:
            __error("Building database objects failed. " +
                    "Before retrying: drop %s schema OR install MADlib into a different schema." % schema.upper(), True)

    ##
    # CASE #3: Target schema does not exist:
    ##
    elif schema_writable == False:

        __info("> Schema %s does not exist" % schema.upper(), verbose)

        # Create MADlib schema
        try:
            __db_create_schema(schema)
        except:
            __db_rollback(schema, None)

        # Create MADlib objects
        try:
            __db_create_objects(schema, None)
        except:
            __db_rollback(schema, None)

    __info("MADlib %s installed successfully in %s schema." % (rev, schema.upper()), True)
        
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Rename schema
# @param from_schema name of the schema to rename
# @param to_schema new name for the schema 
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def __db_rename_schema(from_schema, to_schema):

    __info("> Renaming schema %s to %s" % (from_schema.upper(), to_schema.upper()), True)        
    try:
        __run_sql_query("ALTER SCHEMA %s RENAME TO %s;" % (from_schema, to_schema), True)
    except:
        __error('Cannot rename schema. Stopping installation...', False)
        raise Exception

## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Create schema
# @param from_schema name of the schema to rename
# @param to_schema new name for the schema 
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def __db_create_schema(schema):

    __info("> Creating %s schema" % schema.upper(), True)        
    try:
        __run_sql_query("CREATE SCHEMA %s;" % schema, True)
    except:
        __info('Cannot create new schema. Rolling back installation...', True)
        pass

## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Create MADlib DB objects in the schema
# @param schema name of the target schema 
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def __db_create_objects(schema, old_schema):

    # Create MigrationHistory table
    try:
        __info("> Creating %s.MigrationHistory table" % schema.upper(), True)
        __run_sql_query("DROP TABLE IF EXISTS %s.migrationhistory;" % schema, True)
        sql = """CREATE TABLE %s.migrationhistory 
               (id serial, version varchar(255), applied timestamp default current_timestamp);""" % schema
        __run_sql_query(sql, True);
    except:
        __error("Cannot crate MigrationHistory table", False)
        raise Exception
    
    # Copy MigrationHistory table for record keeping purposes
    if old_schema:
        try:
            __info("> Saving data from %s.MigrationHistory table" % old_schema.upper(), True)
            sql = """INSERT INTO %s.migrationhistory (version, applied) 
                   SELECT version, applied FROM %s.migrationhistory 
                   ORDER BY id;""" % (schema, old_schema)
            __run_sql_query(sql, True);
        except:
            __error("Cannot copy MigrationHistory table", False)
            raise Exception

    # Stamp the DB installation
    try:
        __info("> Writing version info in MigrationHistory table", True)
        __run_sql_query("INSERT INTO %s.migrationhistory(version) VALUES('%s')" % (schema, rev), True);
    except:
        __error("Cannot insert data into %s.migrationhistory table" % schema, False)
        raise Exception
    
    # Run migration SQLs    
    __info("> Creating objects for modules:", True)  
    
    # Loop through all modules/modules  
    for moduleinfo in portspecs['modules']:   
     
        # Get the module name
        module = moduleinfo['name']
        __info("> - %s" % module, True)        
        
        # Find the Python module dir (platform specific or generic)
        if os.path.isdir(maddir + "/ports/" + portid + "/" + dbver + "/modules/" + module):
            maddir_mod_py  = maddir + "/ports/" + portid + "/" + dbver + "/modules"
        else:
            maddir_mod_py  = maddir + "/modules"
        
        # Find the SQL module dir (platform specific or generic)
        if os.path.isdir(maddir + "/ports/" + portid + "/modules/" + module):
            maddir_mod_sql  = maddir + "/ports/" + portid + "/modules"
        elif os.path.isdir(maddir + "/modules/" + module):
            maddir_mod_sql  = maddir + "/modules"
        else:
            # This was a platform-specific module, for which no default exists.
            # We can just skip this module.
            continue

        # Make a temp dir for log files 
        cur_tmpdir = tmpdir + "/" + module
        __make_dir(cur_tmpdir)

        # Loop through all SQL files for this module
        mask = maddir_mod_sql + '/' + module + '/*.sql_in'
        sql_files = glob.glob(mask)

        if not sql_files:
            __error("No files found in: %s" % mask, True)

        # Execute all SQL files for the module
        for sqlfile in sql_files:

            # Set file names
            tmpfile = cur_tmpdir + '/' + os.path.basename(sqlfile) + '.tmp'
            logfile = cur_tmpdir + '/' + os.path.basename(sqlfile) + '.log'
            
            # Run the SQL
            try:
                retval = __run_sql_file(schema, maddir_mod_py, module, sqlfile, tmpfile, logfile, None)
            except:
                raise Exception
                
            # Check the exit status
            if retval != 0:
                __error("Failed executing %s" % tmpfile, False)  
                __error("Check the log at %s" % logfile, False) 
                raise Exception    
                       
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Rollback installation
# @param drop_schema name of the schema to drop
# @param keep_schema name of the schema to rename and keep
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def __db_rollback(drop_schema, keep_schema):

    __info("Rolling back the installation...", True)

    if drop_schema == None:
        __error('No schema name to drop. Stopping rollback...', True)        

    # Drop the current schema
    __info("> Dropping schema %s" % drop_schema.upper(), verbose)        
    try:
        __run_sql_query("DROP SCHEMA %s CASCADE;" % (drop_schema), True)
    except:
        __error("Cannot drop schema %s. Stopping rollback..." % drop_schema.upper(), True)        

    # Rename old to current schema
    if keep_schema != None:    
        __db_rename_schema(keep_schema, drop_schema)   

    __info("Rollback finished successfully.", True)
    raise Exception
            

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
def main(argv):

    parser = argparse.ArgumentParser(
                description='MADlib package manager (' + rev + ')', 
                argument_default=False,
                formatter_class=argparse.RawTextHelpFormatter,
                epilog="""Example:
                                      
  $ madpack install -s madlib -p greenplum -c gpadmin@mdw:5432/testdb

  This will install MADlib objects into a Greenplum database called TESTDB 
  running on server MDW:5432. Installer will try to login as GPADMIN 
  and will prompt for password. The target schema will be MADLIB.
  
""")
            
    parser.add_argument(
        'command', metavar='COMMAND', nargs=1, 
        choices=['install','update','uninstall','reinstall','version','install-check'], 
        help = "One of the following options:\n"
            + "  install/update : run sql scripts to load into DB\n"
            + "  uninstall      : run sql scripts to uninstall from DB\n"
            + "  reinstall      : performs uninstall and install\n"
            + "  version        : compare and print MADlib version (binaries vs database objects)\n"
            + "  install-check  : test all installed modules\n"
    )
  
    parser.add_argument(
        '-c', '--conn', metavar='CONNSTR', nargs=1, dest='connstr', default=None,
        help= "Connection string of the following syntax:\n"
            + "  [user[/password]@][host][:port][/database]\n" 
            + "If not provided default values will be derived for PostgerSQL and Greenplum:\n"
            + "- user: PGUSER or USER env variable or OS username\n"
            + "- pass: PGPASSWORD env variable or runtime prompt\n"
            + "- host: PGHOST env variable or 'localhost'\n"
            + "- port: PGPORT env variable or '5432'\n"
            + "- db: PGDATABASE env variable or OS username\n"
            )

    parser.add_argument('-s', '--schema', nargs=1, dest='schema', 
                         metavar='SCHEMA', default='madlib',
                         help="Target schema for the database objects.")
                         
    parser.add_argument('-p', '--platform', nargs=1, dest='platform', 
                         metavar='PLATFORM', choices=portid_list,
                         help="Target database platform, current choices: " + str(portid_list))

    parser.add_argument('-v', '--verbose', dest='verbose', 
                         action="store_true", help="Verbose mode.")

    parser.add_argument('-l', '--keeplogs', dest='keeplogs', default=False, 
                         action="store_true", help="Do not remove installation log files.")

    parser.add_argument('-d', '--tmpdir', dest='tmpdir', default = '/tmp/',
                         help="Temporary directory location for installation log files.")

    parser.add_argument('-t', '--testcase', dest='testcase', default="",
                         help="Module names to test, comma separated. Effective only for install-check.")

    ##
    # Get the arguments
    ##
    args = parser.parse_args()
    global verbose
    verbose = args.verbose
    __info("Arguments: " + str(args), verbose);    
    global keeplogs
    keeplogs = args.keeplogs

    global tmpdir
    try:
        tmpdir = tempfile.mkdtemp('', 'madlib.', args.tmpdir)
    except OSError, e:
        tmpdir = e.filename
        __error("cannot create temporary directory: '%s'." % tmpdir, True)

        
    ##
    # Parse SCHEMA
    ##
    if len(args.schema[0]) > 1:
        schema = args.schema[0].lower()
    else:    
        schema = args.schema.lower()

    ##
    # Parse DB Platform (== PortID) and compare with Ports.yml
    ##
    global portid
    global dbver
    if args.platform:
        try:
            # Get the DB platform name == DB port id
            portid = args.platform[0].lower()
            ports[portid]
        except:
            portid = None
            __error("Can not find specs for port %s" % (args.platform[0]), True)
    else:
        portid = None

    ##
    # Parse CONNSTR (only if PLATFORM and DBAPI2 are defined)
    ##
    if portid:
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
        __info("Testing database connection...", verbose)
        
        # Get password
        if c_pass is None:
            c_pass = getpass.getpass("Password for user %s: " % c_user)

        # Set connection variables
        global con_args
        con_args['host'] = c_host + ':' + c_port
        con_args['database'] = c_db
        con_args['user'] = c_user
        con_args['password'] = c_pass    

        # Get MADlib version in DB
        dbrev = __get_madlib_dbver(schema)
        
        # Get DB version
        dbver = __get_dbver()
        portdir = os.path.join(maddir, "ports", portid)
        supportedVersions = [dirItem for dirItem in os.listdir(portdir)
                                if os.path.isdir(os.path.join(portdir, dirItem))
                                and re.match("^\d+\.\d+", dirItem)]
        if dbver is None:
            dbver = ".".join(map(str, max([map(int, versionStr.split('.'))
                    for versionStr in supportedVersions])))
            __info("Could not parse version string reported by {DBMS}. Will "
                "default to newest supported version of {DBMS} "
                "({version}).".format(
                    DBMS = ports[portid]['name'],
                    version = dbver
                ), True)
        else:
            __info("Detected %s version %s." % (ports[portid]['name'], dbver),
                True)
            if not os.path.isdir(os.path.join(portdir, dbver)):
                __error("This version is not among the %s versions for which "
                    "MADlib support files have been installed (%s)." % 
                    (ports[portid]['name'], ", ".join(supportedVersions)), True)
        
        # Validate that db platform is correct 
        if __check_db_port(portid) == False:
            __error("Invalid database platform specified.", True)
            
        # Adjust MADlib directories for this port (if they exist)
        global maddir_conf 
        if os.path.isdir(maddir + "/ports/" + portid + "/" + dbver + "/config"):
            maddir_conf = maddir + "/ports/" + portid + "/" + dbver + "/config"
        global maddir_lib
        if os.path.isfile(maddir + "/ports/" + portid + "/" + dbver + \
                "/lib/libmadlib.so"):
            maddir_lib  = maddir + "/ports/" + portid + "/" + dbver + \
                "/lib/libmadlib.so"
        # Get the list of modules for this port
        global portspecs
        portspecs = configyml.get_modules(maddir_conf)                   
    else:
        con_args = None
        dbrev = None
            
    ##
    # Parse COMMAND argument and compare with Ports.yml
    ## 

    # Debugging...
    # print "OS rev: " + str(rev) + " > " + str(__get_rev_num(rev))
    # print "DB rev: " + str(dbrev) + " > " + str(__get_rev_num(dbrev))        

    # Make sure we have the necessary paramaters to continue
    if args.command[0] != 'version':
        if not portid: 
            __error("Missing -p/--platform parameter.", True)
        if not con_args:
            __error("Unknown problem with database connection string: %s" % con_args, True)
   
    ###
    # COMMAND: version
    ###
    if args.command[0] == 'version':

        __print_revs(rev, dbrev, con_args, schema)
                
    ###
    # COMMAND: uninstall/reinstall 
    ###
    if args.command[0] == 'uninstall' or args.command[0] == 'reinstall':

        if __get_rev_num(dbrev) == ['0']:
            __info("Nothing to uninstall. No version found in schema %s." % schema.upper(), True)
            return

        # Find any potential data to lose
        affected_objects = __run_sql_query("""
            SELECT 
                n1.nspname AS schema,
                relname AS relation, 
                attname AS column, 
                typname AS type
            FROM 
                pg_attribute a, 
                pg_class c, 
                pg_type t, 
                pg_namespace n, 
                pg_namespace n1     
            WHERE 
                n.nspname = '%s'
                AND t.typnamespace = n.oid 
                AND a.atttypid = t.oid 
                AND c.oid = a.attrelid 
                AND c.relnamespace = n1.oid
                AND c.relkind = 'r'
            ORDER BY 
                n1.nspname, relname, attname, typname""" % schema.lower()
                , True
        );
                
        __info("*** Uninstalling MADlib ***", True)        
        __info("***********************************************************************************", True)
        __info("* Schema %s and all database objects depending on it will be dropped!" % schema.upper(), True)
        if affected_objects:
            __info("* If you continue the following data will be lost (schema : table.column : type):", True)
            for ao in affected_objects:
                __info ( '* - ' + ao['schema'] + ' : ' + ao['relation'] + '.' + ao['column'] + ' : ' + ao['type'], True);
        __info("***********************************************************************************", True)
        __info("Would you like to continue? [Y/N]", True)
        go = raw_input('>>> ').upper()
        while go != 'Y' and go != 'N':
            go = raw_input('Yes or No >>> ').upper()
        
        # 2) Do the uninstall/drop     
        if go == 'N':            
            __info('No problem. Nothing dropped.', True)
            return
            
        elif go == 'Y':
            __info("> dropping schema %s" % schema.upper(), verbose)        
            try:
                __run_sql_query("DROP SCHEMA %s CASCADE;" % (schema), True)
            except:
                __error("Cannot drop schema %s." % schema.upper(), True)         

            __info('Schema %s (and all dependent objects) has been dropped.' % schema.upper(), True)
            __info('MADlib uninstalled successfully.', True)
            
        else:
            return

    ###
    # COMMAND: install/update/reinstall
    ###
    if ( args.command[0] == 'install' 
         or args.command[0] == 'update'
         or args.command[0] == 'reinstall'):
        
        # Refresh MADlib version in DB
        if args.command[0] == 'reinstall': 
            dbrev = __get_madlib_dbver(schema)
            print ""

        __info("*** Installing MADlib ***", True)
        
        # 1) Compare OS and DB versions. Continue if OS > DB.
        __print_revs(rev, dbrev, con_args, schema)
        if __get_rev_num(dbrev) >= __get_rev_num(rev):
            __info("Current MADlib version already up to date.", True)
            return

        # 2) Run installation 
        try:
            __plpy_check(py_min_ver)
            __db_install(schema, dbrev)
        except:
            __error("MADlib installation failed.", True)
           
    ###
    # COMMAND: install-check
    ###
    if args.command[0] == 'install-check':
    
        # 1) Compare OS and DB versions. Continue if OS = DB.
        if __get_rev_num(dbrev) != __get_rev_num(rev):
            __print_revs(rev, dbrev, con_args, schema)
            __info("Versions do not match. Install-check stopped.", True)
            return

        # Create install-check user
        test_user = 'madlib_' + rev.replace('.','') + '_installcheck'
        try:
            __run_sql_query("DROP USER IF EXISTS %s;" % (test_user), False)
        except:
            __run_sql_query("DROP OWNED BY %s CASCADE;" % (test_user), True)
            __run_sql_query("DROP USER IF EXISTS %s;" % (test_user), True)            
        __run_sql_query("CREATE USER %s;" % (test_user), True)
        # TO DO:
        # Change ALL to USAGE in the below GRANT command
        # and fix the failing modules which still write to MADLIB schema.
        __run_sql_query("GRANT ALL ON SCHEMA %s TO %s;" 
                        % (schema, test_user), True)
         
        # 2) Run test SQLs 
        __info("> Running test scripts for:", verbose)   
        
        caseset = set([test.strip()
            for test in args.testcase.split(',')]) if args.testcase != "" else set()

        # Loop through all modules 
        for moduleinfo in portspecs['modules']:
        
            # Get module name
            module = moduleinfo['name']

            # Skip if doesn't meet specified modules
            if len(caseset) > 0 and module not in caseset:
                continue

            __info("> - %s" % module, verbose)        

            # Make a temp dir for this module (if doesn't exist)
            cur_tmpdir = tmpdir + '/' + module + '/test'
            __make_dir(cur_tmpdir)
            
            # Find the Python module dir (platform specific or generic)
            if os.path.isdir(maddir + "/ports/" + portid + "/" + dbver + "/modules/" + module):
                maddir_mod_py  = maddir + "/ports/" + portid + "/" + dbver + "/modules"
            else:        
                maddir_mod_py  = maddir + "/modules"
            
            # Find the SQL module dir (platform specific or generic)
            if os.path.isdir(maddir + "/ports/" + portid + "/modules/" + module):
                maddir_mod_sql  = maddir + "/ports/" + portid + "/modules"
            else:        
                maddir_mod_sql  = maddir + "/modules"

            # Prepare test schema
            test_schema = "madlib_installcheck_%s" % (module)
            __run_sql_query("DROP SCHEMA IF EXISTS %s CASCADE; CREATE SCHEMA %s;" 
                            % (test_schema, test_schema), True)
            __run_sql_query("GRANT ALL ON SCHEMA %s TO %s;" 
                            % (test_schema, test_user), True)

            # Switch to test user and prepare the search_path
            pre_sql = '-- Switch to test user:\n' \
                      'SET ROLE %s;\n' \
                      '-- Set SEARCH_PATH for install-check:\n' \
                      'SET search_path=%s,%s;\n' \
                      % (test_user, test_schema, schema)
    
            # Loop through all test SQL files for this module
            sql_files = maddir_mod_sql + '/' + module + '/test/*.sql_in'
            for sqlfile in sorted(glob.glob(sql_files)):
            
                result = 'PASS'

                # Set file names
                tmpfile = cur_tmpdir + '/' + os.path.basename(sqlfile) + '.tmp'
                logfile = cur_tmpdir + '/' + os.path.basename(sqlfile) + '.log'
                
                # If there is no problem with the SQL file
                milliseconds = 0

                # Run the SQL
                run_start = datetime.datetime.now()
                retval = __run_sql_file(schema, maddir_mod_py, module, sqlfile, tmpfile, logfile, pre_sql)
                # Runtime evaluation
                run_end = datetime.datetime.now()
                milliseconds = round((run_end - run_start).seconds * 1000 + (run_end - run_start).microseconds / 1000)

                # Check the exit status
                if retval != 0:
                    __error("Failed executing %s" % tmpfile, False)  
                    __error("Check the log at %s" % logfile, False) 
                    result = 'FAIL'
                    keeplogs = True
                # Since every single statement in the test file gets logged, 
                # an empty log file indicates an empty or a failed test
                elif os.path.isfile(logfile) and os.path.getsize(logfile) > 0:
                    result = 'PASS'
                # Otherwise                   
                else:
                    result = 'ERROR'
                
                # Spit the line
                print "TEST CASE RESULT|Module: " + module + \
                    "|" + os.path.basename(sqlfile) + "|" + result + \
                    "|Time: %d milliseconds" % (milliseconds)           

            # Cleanup test schema for the module
            __run_sql_query( "DROP SCHEMA IF EXISTS %s CASCADE;" % (test_schema), True)

        # Drop install-check user
        __run_sql_query( "DROP OWNED BY %s CASCADE;" % (test_user), True)
        __run_sql_query( "DROP USER %s;" % (test_user), True)
            
    
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Start Here
## # # # # # # # # # # # # # # # # # # # # # # # # # # # #
if __name__ == "__main__":

    # Run main
    main(sys.argv[1:])

    # Optional log files cleanup    
    if keeplogs is False:
        shutil.rmtree(tmpdir)
    else:
        print "INFO: Log files saved in " + tmpdir

