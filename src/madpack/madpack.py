#!/usr/bin/env python
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Main Madpack installation executable.
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
import sys
import getpass
import re
import os
import glob
import traceback
import subprocess
import datetime
import tempfile
import shutil
import unittest

from upgrade_util import ChangeHandler
from upgrade_util import ViewDependency
from upgrade_util import TableDependency
from upgrade_util import ScriptCleaner

from itertools import izip_longest

# Required Python version
py_min_ver = [2, 6]

# Check python version
if sys.version_info[:2] < py_min_ver:
    print("ERROR: python version too old (%s). You need %s or greater." %
          ('.'.join(str(i) for i in sys.version_info[:3]), '.'.join(str(i) for i in py_min_ver)))
    exit(1)

# Find MADlib root directory. This file is installed to
# $MADLIB_ROOT/madpack/madpack.py, so to get $MADLIB_ROOT we need to go
# two levels up in the directory hierarchy. We use (a) os.path.realpath and
# (b) __file__ (instead of sys.argv[0]) because madpack.py could be called
# (a) through a symbolic link and (b) not as the main module.
maddir = os.path.abspath(os.path.dirname(os.path.realpath(__file__)) + "/..")   # MADlib root dir
sys.path.append(maddir + "/madpack")

# Import MADlib python modules
import argparse
import configyml

# Some read-only variables
this = os.path.basename(sys.argv[0])    # name of this script

# Default directories
maddir_conf = maddir + "/config"           # Config dir
maddir_lib = maddir + "/lib/libmadlib.so"  # C/C++ libraries

# Read the config files
ports = configyml.get_ports(maddir_conf)  # object made of Ports.yml
rev = configyml.get_version(maddir_conf)  # MADlib OS-level version
portid_list = []
for port in ports:
    portid_list.append(port)

SUPPORTED_PORTS = ('postgres', 'greenplum', 'hawq')

# Global variables
portid = None       # Target port ID (eg: pg90, gp40)
dbconn = None       # DB Connection object
dbver = None        # DB version
con_args = {}       # DB connection arguments
verbose = None      # Verbose flag
keeplogs = None
tmpdir = None
is_hawq2 = False


def _make_dir(dir):
    """
    # Create a temp dir
    # @param dir temp directory path
    """
    if not os.path.isdir(dir):
        try:
            os.makedirs(dir)
        except:
            print "ERROR: can not create directory: %s. Check permissions." % dir
            exit(1)
# ------------------------------------------------------------------------------


def _error(msg, stop):
    """
    Error message wrapper
        @param msg error message
        @param stop program exit flag
    """
    # Print to stdout
    print this + ' : ERROR : ' + msg
    # stack trace is not printed
    if stop:
        exit(2)
# ------------------------------------------------------------------------------


def _info(msg, verbose=True):
    """
    Info message wrapper (verbose)
        @param msg info message
        @param verbose prints only if True
    """
    # Print to stdout
    if verbose:
        print this + ' : INFO : ' + msg
# ------------------------------------------------------------------------------


def run_query(sql, show_error, con_args=con_args):
    # Define sqlcmd
    sqlcmd = 'psql'
    delimiter = ' <$madlib_delimiter$> '

    # Test the DB cmd line utility
    std, err = subprocess.Popen(['which', sqlcmd], stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE).communicate()
    if std == '':
        _error("Command not found: %s" % sqlcmd, True)

    # Run the query
    runcmd = [sqlcmd,
              '-h', con_args['host'].split(':')[0],
              '-p', con_args['host'].split(':')[1],
              '-d', con_args['database'],
              '-U', con_args['user'],
              '-F', delimiter,
              '--no-password',
              '--no-psqlrc',
              '--no-align',
              '-c', sql]
    runenv = os.environ
    if 'password' in con_args:
        runenv["PGPASSWORD"] = con_args['password']
    runenv["PGOPTIONS"] = '-c search_path=public -c client_min_messages=error'
    std, err = subprocess.Popen(runcmd, env=runenv, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE).communicate()

    if err:
        if show_error:
            _error("SQL command failed: \nSQL: %s \n%s" % (sql, err), False)
        if 'password' in err:
            raise EnvironmentError
        else:
            raise Exception

    # Convert the delimited output into a dictionary
    results = []  # list of rows
    i = 0
    for line in std.splitlines():
        if i == 0:
            cols = [name for name in line.split(delimiter)]
        else:
            row = {}  # dict of col_name:col_value pairs
            c = 0
            for val in line.split(delimiter):
                row[cols[c]] = val
                c += 1
            results.insert(i, row)
        i += 1
    # Drop the last line: "(X rows)"
    try:
        results.pop()
    except:
        pass

    return results
# ------------------------------------------------------------------------------


def _internal_run_query(sql, show_error):
    """
    Runs a SQL query on the target platform DB
    using the default command-line utility.
    Very limited:
      - no text output with "new line" characters allowed
         @param sql query text to execute
         @param show_error displays the SQL error msg
    """
    return run_query(sql, show_error, con_args)
# ------------------------------------------------------------------------------


def _get_relative_maddir(maddir, port):
    """ Return a relative path version of maddir

    GPDB and HAWQ installations have a symlink outside of GPHOME that
    links to the current GPHOME. After a DB upgrade, this symlink is updated to
    the new GPHOME.

    'maddir_lib', which uses the absolute path of GPHOME, is hardcoded into each
    madlib function definition. Replacing the GPHOME path with the equivalent
    relative path makes it simpler to perform DB upgrades without breaking MADlib.
    """
    if port not in ('greenplum', 'hawq'):
        # do nothing for postgres
        return maddir

    # e.g. maddir_lib = $GPHOME/madlib/Versions/1.9/lib/libmadlib.so
    # 'madlib' is supposed to be in this path, which is the default folder
    # used by GPPKG to install madlib
    try:
        abs_gphome, tail = maddir.split('madlib/')
    except ValueError:
        return maddir

    link_name = 'greenplum-db' if port == 'greenplum' else 'hawq'

    # Check outside $GPHOME if there is a symlink to this absolute path
    # os.pardir is equivalent to ..
    # os.path.normpath removes the extraneous .. from that path
    rel_gphome = os.path.normpath(os.path.join(abs_gphome, os.pardir, link_name))
    if os.path.islink(rel_gphome) and os.path.realpath(rel_gphome) == os.path.realpath(abs_gphome):
        # if the relative link exists and is pointing to current location
        return os.path.join(rel_gphome, 'madlib', tail)
    else:
        return maddir
# ------------------------------------------------------------------------------


def _run_sql_file(schema, maddir_mod_py, module, sqlfile,
                  tmpfile, logfile, pre_sql, upgrade=False,
                  sc=None):
    """
        Run SQL file
            @param schema name of the target schema
            @param maddir_mod_py name of the module dir with Python code
            @param module  name of the module
            @param sqlfile name of the file to parse
            @param tmpfile name of the temp file to run
            @param logfile name of the log file (stdout)
            @param pre_sql optional SQL to run before executing the file
            @param upgrade are we upgrading as part of this sql run
            @param sc object of ScriptCleaner
    """

    # Check if the SQL file exists
    if not os.path.isfile(sqlfile):
        _error("Missing module SQL file (%s)" % sqlfile, False)
        raise ValueError("Missing module SQL file (%s)" % sqlfile)

    # Prepare the file using M4
    try:
        f = open(tmpfile, 'w')
        # Add the before SQL
        if pre_sql:
            f.writelines([pre_sql, '\n\n'])
            f.flush()
        # Find the madpack dir (platform specific or generic)
        if os.path.isdir(maddir + "/ports/" + portid + "/" + dbver + "/madpack"):
            maddir_madpack = maddir + "/ports/" + portid + "/" + dbver + "/madpack"
        else:
            maddir_madpack = maddir + "/madpack"
        maddir_ext_py = maddir + "/lib/python"

        m4args = ['m4',
                  '-P',
                  '-DMADLIB_SCHEMA=' + schema,
                  '-DPLPYTHON_LIBDIR=' + maddir_mod_py,
                  '-DEXT_PYTHON_LIBDIR=' + maddir_ext_py,
                  '-DMODULE_PATHNAME=' + maddir_lib,
                  '-DMODULE_NAME=' + module,
                  '-I' + maddir_madpack,
                  sqlfile]

        _info("> ... parsing: " + " ".join(m4args), verbose)

        subprocess.call(m4args, stdout=f)
        f.close()
    except:
        _error("Failed executing m4 on %s" % sqlfile, False)
        raise Exception

    # Only update function definition
    sub_module = ''
    if upgrade:
        # get filename from complete path without the extension
        sub_module = os.path.splitext(os.path.basename(sqlfile))[0]
        _info(sub_module, False)
        if sub_module not in sc.get_change_handler().newmodule:
            sql = open(tmpfile).read()
            sql = sc.cleanup(sql)
            open(tmpfile, 'w').write(sql)

    # Run the SQL using DB command-line utility
    if portid in ('greenplum', 'postgres', 'hawq'):
        sqlcmd = 'psql'
        # Test the DB cmd line utility
        std, err = subprocess.Popen(['which', sqlcmd], stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE).communicate()
        if not std:
            _error("Command not found: %s" % sqlcmd, True)

        runcmd = [sqlcmd, '-a',
                  '-v', 'ON_ERROR_STOP=1',
                  '-h', con_args['host'].split(':')[0],
                  '-p', con_args['host'].split(':')[1],
                  '-d', con_args['database'],
                  '-U', con_args['user'],
                  '--no-password',
                  '-f', tmpfile]
        runenv = os.environ
        if 'password' in con_args:
            runenv["PGPASSWORD"] = con_args['password']
        runenv["PGOPTIONS"] = '-c client_min_messages=notice'

    # Open log file
    try:
        log = open(logfile, 'w')
    except:
        _error("Cannot create log file: %s" % logfile, False)
        raise Exception

    # Run the SQL
    try:
        _info("> ... executing " + tmpfile, verbose)
        retval = subprocess.call(runcmd, env=runenv, stdout=log, stderr=log)
    except:
        _error("Failed executing %s" % tmpfile, False)
        raise Exception
    finally:
        log.close()

    return retval
# ------------------------------------------------------------------------------


def _get_madlib_dbrev(schema):
    """
    Read MADlib version from database
        @param dbconn database conection object
        @param schema MADlib schema name
    """
    try:
        row = _internal_run_query("SELECT count(*) AS cnt FROM pg_tables " +
                                  "WHERE schemaname='" + schema + "' AND " +
                                  "tablename='migrationhistory'", True)
        if int(row[0]['cnt']) > 0:
            row = _internal_run_query("""SELECT version FROM %s.migrationhistory
                ORDER BY applied DESC LIMIT 1""" % schema, True)
            if row:
                return row[0]['version']
    except:
        _error("Failed reading MADlib db version", True)

    return None
# ------------------------------------------------------------------------------


def _get_dbver():
    """ Read version number from database (of form X.Y) """
    try:
        versionStr = _internal_run_query("SELECT pg_catalog.version()", True)[0]['version']
        if portid == 'postgres':
            match = re.search("PostgreSQL[a-zA-Z\s]*(\d+\.\d+)", versionStr)
        elif portid == 'greenplum':
            # for Greenplum the 3rd digit is necessary to differentiate
            # 4.3.5+ from versions < 4.3.5
            match = re.search("Greenplum[a-zA-Z\s]*(\d+\.\d+\.\d+)", versionStr)
        elif portid == 'hawq':
            match = re.search("HAWQ[a-zA-Z\s]*(\d+\.\d+)", versionStr)
        return None if match is None else match.group(1)
    except:
        _error("Failed reading database version", True)
# ------------------------------------------------------------------------------


def _check_db_port(portid):
    """
    Make sure we are connected to the expected DB platform
        @param portid expected DB port id - to be validates
    """
    # Postgres
    try:
        row = _internal_run_query("SELECT version() AS version", True)
    except:
        _error("Cannot validate DB platform type", True)
    if row and row[0]['version'].lower().find(portid) >= 0:
        if portid == 'postgres':
            if row[0]['version'].lower().find('greenplum') < 0:
                return True
        elif portid == 'greenplum':
            if row[0]['version'].lower().find('hawq') < 0:
                return True
        elif portid == 'hawq':
            return True
    return False
# ------------------------------------------------------------------------------


def _is_rev_gte(left, right):
    """ Return if left >= right

    Args:
        @param left: list. Revision numbers in a list form (as returned by
                           _get_rev_num).
        @param right: list. Revision numbers in a list form (as returned by
                           _get_rev_num).

    Returns:
        Boolean

    If left and right are all numeric then regular list comparison occurs.
    If either one contains a string, then comparison occurs till both have int.
        First list to have a string is considered smaller
        (including if the other does not have an element in corresponding index)

    Examples:
        [1, 9, 0] >= [1, 9, 0]
        [1, 9, 1] >= [1, 9, 0]
        [1, 9, 1] >= [1, 9]
        [1, 10] >= [1, 9, 1]
        [1, 9, 0] >= [1, 9, 0, 'dev']
        [1, 9, 1] >= [1, 9, 0, 'dev']
        [1, 9, 0] >= [1, 9, 'dev']
        [1, 9, 'rc'] >= [1, 9, 'dev']
        [1, 9, 'rc', 0] >= [1, 9, 'dev', 1]
        [1, 9, 'rc', '1'] >= [1, 9, 'rc', '1']
    """
    def all_numeric(l):
        return not l or all(isinstance(i, int) for i in l)

    if all_numeric(left) and all_numeric(right):
        return left >= right
    else:
        for i, (l_e, r_e) in enumerate(izip_longest(left, right)):
            if isinstance(l_e, int) and isinstance(r_e, int):
                if l_e == r_e:
                    continue
                else:
                    return l_e > r_e
            elif isinstance(l_e, int) or isinstance(r_e, int):
                #  [1, 9, 0] > [1, 9, 'dev']
                #  [1, 9, 0] > [1, 9]
                return isinstance(l_e, int)
            else:
                # both are not int
                if r_e is None:
                    # [1, 9, 'dev'] < [1, 9]
                    return False
                else:
                    return l_e is None or left[i:] >= right[i:]
        return True
# ----------------------------------------------------------------------


def _get_rev_num(rev):
    """
    Convert version string into number for comparison
        @param rev version text
                It is expected to follow Semantic Versioning (semver.org)
                Valid inputs:
                    1.9.0, 1.10.0, 2.5.0
                    1.0.0-alpha, 1.0.0-alpha.1, 1.0.0-0.3.7, 1.0.0-x.7.z.92
                    1.0.0+20130313144700, 1.0.0-beta+exp.sha.5114f85
    """
    try:
        rev_parts = re.split('[-+_]', rev)
        # get numeric part of the version string
        num = [int(i) for i in rev_parts[0].split('.')]
        num += [0] * (3 - len(num))  # normalize num to be of length 3
        # get identifier part of the version string
        if len(rev_parts) > 1:
            num.extend(map(str, rev_parts[1:]))
        if not num:
            num = [0]
        return num
    except:
        # invalid revision
        return [0]
# ------------------------------------------------------------------------------


def _print_revs(rev, dbrev, con_args, schema):
    """
    Print version information
        @param rev OS-level MADlib version
        @param dbrev DB-level MADlib version
        @param con_args database connection arguments
        @param schema MADlib schema name
    """
    _info("MADlib tools version    = %s (%s)" % (str(rev), sys.argv[0]), True)
    if con_args:
        try:
            _info("MADlib database version = %s (host=%s, db=%s, schema=%s)"
                  % (dbrev, con_args['host'], con_args['database'], schema), True)
        except:
            _info("MADlib database version = [Unknown] (host=%s, db=%s, schema=%s)"
                  % (dbrev, con_args['host'], con_args['database'], schema), True)
    return
# ------------------------------------------------------------------------------


def _plpy_check(py_min_ver):
    """
    Check pl/python existence and version
        @param py_min_ver min Python version to run MADlib
    """

    _info("Testing PL/Python environment...", True)

    # Check PL/Python existence
    rv = _internal_run_query("SELECT count(*) AS CNT FROM pg_language "
                             "WHERE lanname = 'plpythonu'", True)
    if int(rv[0]['cnt']) > 0:
        _info("> PL/Python already installed", verbose)
    else:
        _info("> PL/Python not installed", verbose)
        _info("> Creating language PL/Python...", True)
        try:
            _internal_run_query("CREATE LANGUAGE plpythonu;", True)
        except:
            _error("""Cannot create language plpythonu. Please check if you
                have configured and installed portid (your platform) with
                `--with-python` option. Stopping installation...""", False)
            raise Exception

    # Check PL/Python version
    _internal_run_query("DROP FUNCTION IF EXISTS plpy_version_for_madlib();", False)
    _internal_run_query("""
        CREATE OR REPLACE FUNCTION plpy_version_for_madlib()
        RETURNS TEXT AS
        $$
            import sys
            # return '.'.join(str(item) for item in sys.version_info[:3])
            return str(sys.version_info[:3]).replace(',','.').replace(' ','').replace(')','').replace('(','')
        $$
        LANGUAGE plpythonu;
    """, True)
    rv = _internal_run_query("SELECT plpy_version_for_madlib() AS ver;", True)
    python = rv[0]['ver']
    py_cur_ver = [int(i) for i in python.split('.')]
    if py_cur_ver >= py_min_ver:
        _info("> PL/Python version: %s" % python, verbose)
    else:
        _error("PL/Python version too old: %s. You need %s or greater"
               % (python, '.'.join(str(i) for i in py_min_ver)), False)
        raise Exception
    _internal_run_query("DROP FUNCTION IF EXISTS plpy_version_for_madlib();", False)
    _info("> PL/Python environment OK (version: %s)" % python, True)
# ------------------------------------------------------------------------------


def _db_install(schema, dbrev, testcase):
    """
    Install MADlib
        @param schema MADlib schema name
        @param dbrev DB-level MADlib version
        @param testcase command-line args for a subset of modules
    """
    _info("Installing MADlib into %s schema..." % schema.upper(), True)

    temp_schema = schema + '_v' + ''.join(map(str, _get_rev_num(dbrev)))
    # Check the status of MADlib objects in database
    madlib_exists = False if dbrev is None else True

    # Test if schema is writable
    try:
        _internal_run_query("CREATE TABLE %s.__madlib_test_table (A INT);" % schema, False)
        _internal_run_query("DROP TABLE %s.__madlib_test_table;" % schema, False)
        schema_writable = True
    except:
        schema_writable = False
    # CASE #1: Target schema exists with MADlib objects:
    if schema_writable and madlib_exists:
        # work-around before UDT is available in HAWQ
        if portid == 'hawq':
            _info("***************************************************************************", True)
            _info("* Schema MADLIB already exists", True)
            _info("* For HAWQ, MADlib objects will be overwritten to the 'MADLIB' schema", True)
            _info("* It may drop any database objects (tables, views, etc.) that depend on 'MADLIB' SCHEMA!!!!!!!!!!!!!", True)
            _info("***************************************************************************", True)
            _info("Would you like to continue? [Y/N]", True)
            go = raw_input('>>> ').upper()
            while go not in ('Y', 'YES', 'N', 'NO'):
                go = raw_input('Yes or No >>> ').upper()
            if go in ('N', 'NO'):
                _info('Installation stopped.', True)
                return
            # Rolling back in HAWQ will drop catalog functions. For exception, we
            # simply push the exception to the caller to terminate the install
            _db_create_objects(schema, None, testcase=testcase, hawq_debug=True)
        else:
            _info("***************************************************************************", True)
            _info("* Schema %s already exists" % schema.upper(), True)
            _info("* Installer will rename it to %s" % temp_schema.upper(), True)
            _info("***************************************************************************", True)
            _info("Would you like to continue? [Y/N]", True)
            go = raw_input('>>> ').upper()
            while go not in ('Y', 'YES', 'N', 'NO'):
                go = raw_input('Yes or No >>> ').upper()
            if go in ('N', 'NO'):
                _info('Installation stopped.', True)
                return

            # Rename MADlib schema
            _db_rename_schema(schema, temp_schema)

            # Create MADlib schema
            try:
                _db_create_schema(schema)
            except:
                _db_rollback(schema, temp_schema)

            # Create MADlib objects
            try:
                _db_create_objects(schema, temp_schema, testcase=testcase)
            except:
                _db_rollback(schema, temp_schema)

    # CASE #2: Target schema exists w/o MADlib objects:
    # For HAWQ, after the DB initialization, there is no
    # madlib.migrationhistory table, thus madlib_exists is False
    elif schema_writable and not madlib_exists:
            # Create MADlib objects
            try:
                _db_create_objects(schema, None, testcase=testcase)
            except:
                _error("Building database objects failed. "
                       "Before retrying: drop %s schema OR install MADlib into "
                       "a different schema." % schema.upper(), True)

    #
    # CASE #3: Target schema does not exist:
    #
    elif not schema_writable:
        if portid == 'hawq' and not is_hawq2:
            # Rolling back in HAWQ will drop catalog functions. For exception, we
            # simply push the exception to the caller to terminate the install
            raise Exception("MADLIB schema is required for HAWQ")

        _info("> Schema %s does not exist" % schema.upper(), verbose)

        # Create MADlib schema
        try:
            _db_create_schema(schema)
        except:
            _db_rollback(schema, None)

        # Create MADlib objects
        try:
            _db_create_objects(schema, None, testcase=testcase)
        except:
            _db_rollback(schema, None)

    _info("MADlib %s installed successfully in %s schema." % (str(rev), schema.upper()), True)
# ------------------------------------------------------------------------------


def _db_upgrade(schema, dbrev):
    """
    Upgrade MADlib
        @param schema MADlib schema name
        @param dbrev DB-level MADlib version
    """
    if _is_rev_gte(_get_rev_num(dbrev), _get_rev_num(rev)):
        _info("Current MADlib version already up to date.", True)
        return

    if _is_rev_gte([1,8],_get_rev_num(dbrev)):
        _error("""
            MADlib versions prior to v1.9 are not supported for upgrade.
            Please try upgrading to v1.9.1 and then upgrade to this version.
            """, True)
        return

    _info("Upgrading MADlib into %s schema..." % schema.upper(), True)
    _info("\tDetecting dependencies...", True)

    _info("\tLoading change list...", True)
    ch = ChangeHandler(schema, portid, con_args, maddir, dbrev, is_hawq2)

    _info("\tDetecting table dependencies...", True)
    td = TableDependency(schema, portid, con_args)

    _info("\tDetecting view dependencies...", True)
    vd = ViewDependency(schema, portid, con_args)

    abort = False
    if td.has_dependency():
        _info("*" * 50, True)
        _info("\tFollowing user tables/indexes are dependent on MADlib objects:", True)
        _info(td.get_dependency_str(), True)
        _info("*" * 50, True)
        cd_udt = [udt for udt in td.get_depended_udt() if udt in ch.udt]
        if len(cd_udt) > 0:
            _error("""
                User has objects dependent on following updated MADlib types!
                        {0}
                These objects need to be dropped before upgrading.
                """.format('\n\t\t\t'.join(cd_udt)), False)

            # we add special handling for 'linregr_result'
            if 'linregr_result' in cd_udt:
                _info("""Dependency on 'linregr_result' could be due to objects
                        created from the output of the aggregate 'linregr'.
                        Please refer to the Linear Regression documentation
                        <http://madlib.apache.org/docs/latest/group__grp__linreg.html#warning>
                        for the recommended solution.
                        """, False)
            abort = True

        c_udoc = ch.get_udoc_oids()
        d_udoc = td.get_depended_udoc_oids()
        cd_udoc = [udoc for udoc in d_udoc if udoc in c_udoc]
        if len(cd_udoc) > 0:
            _error("""
                User has objects dependent on the following updated MADlib operator classes!
                        oid={0}
                These objects need to be dropped before upgrading.
                """.format('\n\t\t\t'.join(cd_udoc)), False)
            abort = True

    if vd.has_dependency():
        _info("*" * 50, True)
        _info("\tFollowing user views are dependent on MADlib objects:", True)
        _info(vd.get_dependency_graph_str(), True)
        _info("*" * 50, True)

        c_udf = ch.get_udf_signature()
        d_udf = vd.get_depended_func_signature('UDF')
        cd_udf = [udf for udf in d_udf if udf in c_udf]
        if len(cd_udf) > 0:
            _error("""
                User has objects dependent on following updated MADlib functions!
                    {0}
                These objects will fail to work with the updated functions and
                need to be dropped before starting upgrade again.
                """.format('\n\t\t\t\t\t'.join(cd_udf)), False)
            abort = True

        c_uda = ch.get_uda_signature()
        d_uda = vd.get_depended_func_signature('UDA')
        cd_uda = [uda for uda in d_uda if uda in c_uda]
        if len(cd_uda) > 0:
            _error("""
                User has objects dependent on following updated MADlib aggregates!
                    {0}
                These objects will fail to work with the new aggregates and
                need to be dropped before starting upgrade again.
                """.format('\n\t\t\t\t\t'.join(cd_uda)), False)
            abort = True

        c_udo = ch.get_udo_oids()
        d_udo = vd.get_depended_opr_oids()
        cd_udo = [udo for udo in d_udo if udo in c_udo]
        if len(cd_udo) > 0:
            _error("""
                User has objects dependent on following updated MADlib operators!
                    oid={0}
                These objects will fail to work with the new operators and
                need to be dropped before starting upgrade again.
                """.format('\n\t\t\t\t\t'.join(cd_udo)), False)
            abort = True

    if abort:
        _error("""------- Upgrade aborted. -------
                Backup and drop all objects that depend on MADlib before trying upgrade again.
                Use madpack reinstall to automatically drop these objects only if appropriate.""", True)
    else:
        _info("No dependency problem found, continuing to upgrade ...", True)

    _info("\tReading existing UDAs/UDTs...", False)
    sc = ScriptCleaner(schema, portid, con_args, ch)
    _info("Script Cleaner initialized ...", False)

    ch.drop_changed_uda()
    ch.drop_changed_udoc()
    ch.drop_changed_udo()
    ch.drop_changed_udc()
    ch.drop_changed_udf()
    ch.drop_changed_udt()  # assume dependent udf for udt does not change
    ch.drop_traininginfo_4dt()  # used types: oid, text, integer, float
    _db_create_objects(schema, None, True, sc)

    _info("MADlib %s upgraded successfully in %s schema." % (str(rev), schema.upper()), True)
# ------------------------------------------------------------------------------


def _db_rename_schema(from_schema, to_schema):
    """
    Rename schema
        @param from_schema name of the schema to rename
        @param to_schema new name for the schema
    """

    _info("> Renaming schema %s to %s" % (from_schema.upper(), to_schema.upper()), True)
    try:
        _internal_run_query("ALTER SCHEMA %s RENAME TO %s;" % (from_schema, to_schema), True)
    except:
        _error('Cannot rename schema. Stopping installation...', False)
        raise Exception
# ------------------------------------------------------------------------------


def _db_create_schema(schema):
    """
    Create schema
        @param from_schema name of the schema to rename
        @param to_schema new name for the schema
    """

    _info("> Creating %s schema" % schema.upper(), True)
    try:
        _internal_run_query("CREATE SCHEMA %s;" % schema, True)
    except:
        _info('Cannot create new schema. Rolling back installation...', True)
        pass
# ------------------------------------------------------------------------------


def _db_create_objects(schema, old_schema, upgrade=False, sc=None, testcase="",
                       hawq_debug=False):
    """
    Create MADlib DB objects in the schema
        @param schema Name of the target schema
        @param sc ScriptCleaner object
        @param testcase Command-line args for modules to install

        @param hawq_debug
    """
    if not upgrade and not hawq_debug:
        # Create MigrationHistory table
        try:
            _info("> Creating %s.MigrationHistory table" % schema.upper(), True)
            _internal_run_query("DROP TABLE IF EXISTS %s.migrationhistory;" % schema, True)
            sql = """CREATE TABLE %s.migrationhistory
                   (id serial, version varchar(255),
                    applied timestamp default current_timestamp);""" % schema
            _internal_run_query(sql, True)
        except:
            _error("Cannot crate MigrationHistory table", False)
            raise Exception

        # Copy MigrationHistory table for record keeping purposes
        if old_schema:
            try:
                _info("> Saving data from %s.MigrationHistory table" % old_schema.upper(), True)
                sql = """INSERT INTO %s.migrationhistory (version, applied)
                       SELECT version, applied FROM %s.migrationhistory
                       ORDER BY id;""" % (schema, old_schema)
                _internal_run_query(sql, True)
            except:
                _error("Cannot copy MigrationHistory table", False)
                raise Exception

    # Stamp the DB installation
    try:
        _info("> Writing version info in MigrationHistory table", True)
        _internal_run_query("INSERT INTO %s.migrationhistory(version) "
                            "VALUES('%s')" % (schema, str(rev)), True)
    except:
        _error("Cannot insert data into %s.migrationhistory table" % schema, False)
        raise Exception

    # Run migration SQLs
    if upgrade:
        _info("> Creating/Updating objects for modules:", True)
    else:
        _info("> Creating objects for modules:", True)

    caseset = (set([test.strip() for test in testcase.split(',')])
               if testcase != "" else set())

    modset = {}
    for case in caseset:
        if case.find('/') > -1:
            [mod, algo] = case.split('/')
            if mod not in modset:
                modset[mod] = []
            if algo not in modset[mod]:
                modset[mod].append(algo)
        else:
            modset[case] = []

    # Loop through all modules/modules
    # portspecs is a global variable
    for moduleinfo in portspecs['modules']:

        # Get the module name
        module = moduleinfo['name']

        # Skip if doesn't meet specified modules
        if modset is not None and len(modset) > 0 and module not in modset:
            continue

        _info("> - %s" % module, True)

        # Find the Python module dir (platform specific or generic)
        if os.path.isdir(maddir + "/ports/" + portid + "/" + dbver + "/modules/" + module):
            maddir_mod_py = maddir + "/ports/" + portid + "/" + dbver + "/modules"
        else:
            maddir_mod_py = maddir + "/modules"

        # Find the SQL module dir (platform specific or generic)
        if os.path.isdir(maddir + "/ports/" + portid + "/modules/" + module):
            maddir_mod_sql = maddir + "/ports/" + portid + "/modules"
        elif os.path.isdir(maddir + "/modules/" + module):
            maddir_mod_sql = maddir + "/modules"
        else:
            # This was a platform-specific module, for which no default exists.
            # We can just skip this module.
            continue

        # Make a temp dir for log files
        cur_tmpdir = tmpdir + "/" + module
        _make_dir(cur_tmpdir)

        # Loop through all SQL files for this module
        mask = maddir_mod_sql + '/' + module + '/*.sql_in'
        sql_files = glob.glob(mask)

        if not sql_files:
            _error("No files found in: %s" % mask, True)

        # Execute all SQL files for the module
        for sqlfile in sql_files:
            algoname = os.path.basename(sqlfile).split('.')[0]
            if portid == 'hawq' and not is_hawq2 and algoname in ('svec'):
                continue

            # run only algo specified
            if module in modset and len(modset[module]) > 0 \
                    and algoname not in modset[module]:
                continue

            # Set file names
            tmpfile = cur_tmpdir + '/' + os.path.basename(sqlfile) + '.tmp'
            logfile = cur_tmpdir + '/' + os.path.basename(sqlfile) + '.log'
            retval = _run_sql_file(schema, maddir_mod_py, module, sqlfile,
                                   tmpfile, logfile, None, upgrade,
                                   sc)
            # Check the exit status
            if retval != 0:
                _error("TEST CASE RESULTed executing %s" % tmpfile, False)
                _error("Check the log at %s" % logfile, False)
                raise Exception
# ------------------------------------------------------------------------------


def _db_rollback(drop_schema, keep_schema):
    """
    Rollback installation
        @param drop_schema name of the schema to drop
        @param keep_schema name of the schema to rename and keep
    """
    _info("Rolling back the installation...", True)

    if not drop_schema:
        _error('No schema name to drop. Stopping rollback...', True)

    # Drop the current schema
    _info("> Dropping schema %s" % drop_schema.upper(), verbose)
    try:
        _internal_run_query("DROP SCHEMA %s CASCADE;" % (drop_schema), True)
    except:
        _error("Cannot drop schema %s. Stopping rollback..." % drop_schema.upper(), True)

    # Rename old to current schema
    if keep_schema:
        _db_rename_schema(keep_schema, drop_schema)

    _info("Rollback finished successfully.", True)
    raise Exception
# ------------------------------------------------------------------------------


def unescape(string):
    """
    Unescape separation characters in connection strings, i.e., remove first
    backslash from "\/", "\@", "\:", and "\\".
    """
    if string is None:
        return None
    else:
        return re.sub(r'\\(?P<char>[/@:\\])', '\g<char>', string)
# ------------------------------------------------------------------------------


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
# ------------------------------------------------------------------------------


def parse_arguments():
    parser = argparse.ArgumentParser(
        prog="madpack",
        description='MADlib package manager (' + str(rev) + ')',
        argument_default=False,
        formatter_class=argparse.RawTextHelpFormatter,
        epilog="""Example:

  $ madpack install -s madlib -p greenplum -c gpadmin@mdw:5432/testdb

  This will install MADlib objects into a Greenplum database called TESTDB
  running on server MDW:5432. Installer will try to login as GPADMIN
  and will prompt for password. The target schema will be MADLIB.
  """)

    help_msg = """One of the following options:
                  install        : run sql scripts to load into DB
                  upgrade        : run sql scripts to upgrade
                  uninstall      : run sql scripts to uninstall from DB
                  reinstall      : performs uninstall and install
                  version        : compare and print MADlib version (binaries vs database objects)
                  install-check  : test all installed modules

                  (uninstall is currently unavailable for the HAWQ port)"""
    choice_list = ['install', 'update', 'upgrade', 'uninstall',
                   'reinstall', 'version', 'install-check']

    parser.add_argument('command', metavar='COMMAND', nargs=1,
                        choices=choice_list, help=help_msg)

    parser.add_argument(
        '-c', '--conn', metavar='CONNSTR', nargs=1, dest='connstr', default=None,
        help="""Connection string of the following syntax:
                   [user[/password]@][host][:port][/database]
                 If not provided default values will be derived for PostgerSQL and Greenplum:
                 - user: PGUSER or USER env variable or OS username
                 - pass: PGPASSWORD env variable or runtime prompt
                 - host: PGHOST env variable or 'localhost'
                 - port: PGPORT env variable or '5432'
                 - db: PGDATABASE env variable or OS username""")

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

    parser.add_argument('-d', '--tmpdir', dest='tmpdir', default='/tmp/',
                        help="Temporary directory location for installation log files.")

    parser.add_argument('-t', '--testcase', dest='testcase', default="",
                        help="Module names to test, comma separated. Effective only for install-check.")

    # Get the arguments
    return parser.parse_args()


def main(argv):
    args = parse_arguments()

    global verbose
    verbose = args.verbose
    _info("Arguments: " + str(args), verbose)
    global keeplogs
    keeplogs = args.keeplogs

    global tmpdir
    try:
        tmpdir = tempfile.mkdtemp('', 'madlib.', args.tmpdir)
    except OSError, e:
        tmpdir = e.filename
        _error("cannot create temporary directory: '%s'." % tmpdir, True)

    # Parse SCHEMA
    if len(args.schema[0]) > 1:
        schema = args.schema[0].lower()
    else:
        schema = args.schema.lower()

    # Parse DB Platform (== PortID) and compare with Ports.yml
    global portid
    if args.platform:
        try:
            # Get the DB platform name == DB port id
            portid = args.platform[0].lower()
            ports[portid]
        except:
            portid = None
            _error("Can not find specs for port %s" % (args.platform[0]), True)
    else:
        portid = None

    # Parse CONNSTR (only if PLATFORM and DBAPI2 are defined)
    if portid:
        connStr = "" if args.connstr is None else args.connstr[0]
        (c_user, c_pass, c_host, c_port, c_db) = parseConnectionStr(connStr)

        # Find the default values for PG and GP
        if portid in SUPPORTED_PORTS:
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

        # Set connection variables
        global con_args
        con_args['host'] = c_host + ':' + c_port
        con_args['database'] = c_db
        con_args['user'] = c_user
        if c_pass is not None:
            con_args['password'] = c_pass

        # Try connecting to the database
        _info("Testing database connection...", verbose)

        try:
            # check for password only if required
            _internal_run_query("SELECT 1", False)
        except EnvironmentError:
            con_args['password'] = getpass.getpass("Password for user %s: " % c_user)
            _internal_run_query("SELECT 1", False)
        except:
            _error('Failed to connect to database', True)

        # Get DB version
        global dbver
        dbver = _get_dbver()
        global is_hawq2
        if portid == "hawq" and _is_rev_gte(_get_rev_num(dbver), _get_rev_num('2.0')):
            is_hawq2 = True
        else:
            is_hawq2 = False

        # HAWQ < 2.0 has hard-coded schema name 'madlib'
        if portid == 'hawq' and not is_hawq2 and schema.lower() != 'madlib':
            _error("*** Installation is currently restricted only to 'madlib' schema ***", True)

        # update maddir to use a relative path if available
        global maddir
        maddir = _get_relative_maddir(maddir, portid)

        # Get MADlib version in DB
        dbrev = _get_madlib_dbrev(schema)

        portdir = os.path.join(maddir, "ports", portid)
        supportedVersions = [dirItem for dirItem in os.listdir(portdir)
                             if os.path.isdir(os.path.join(portdir, dirItem)) and
                             re.match("^\d+", dirItem)]
        if dbver is None:
            dbver = ".".join(
                map(str, max([versionStr.split('.')
                              for versionStr in supportedVersions])))
            _info("Could not parse version string reported by {DBMS}. Will "
                  "default to newest supported version of {DBMS} "
                  "({version}).".format(DBMS=ports[portid]['name'],
                                        version=dbver), True)
        else:
            _info("Detected %s version %s." % (ports[portid]['name'], dbver),
                  True)

            if portid == "hawq":
                # HAWQ (starting 2.0) and GPDB (starting 5.0) uses semantic versioning,
                # which implies all HAWQ 2.x or GPDB 5.x versions will have binary
                # compatibility. Hence, we can keep single folder for all 2.X / 5.X.
                if (_is_rev_gte(_get_rev_num(dbver), _get_rev_num('2.0')) and
                        not _is_rev_gte(_get_rev_num(dbver), _get_rev_num('3.0'))):
                    is_hawq2 = True
                    dbver = '2'
            elif portid == 'greenplum':
                # similar to HAWQ above, collapse all 5.X versions
                if (_is_rev_gte(_get_rev_num(dbver), _get_rev_num('5.0')) and
                        not _is_rev_gte(_get_rev_num(dbver), _get_rev_num('6.0'))):
                    dbver = '5'
                elif (_is_rev_gte(_get_rev_num(dbver), _get_rev_num('6.0')) and
                        not _is_rev_gte(_get_rev_num(dbver), _get_rev_num('7.0'))):
                    dbver = '6'
                # Due to the ABI incompatibility between 4.3.4 and 4.3.5,
                # MADlib treats 4.3.5+ as DB version 4.3ORCA which is different
                # from 4.3. The name is suffixed with ORCA since optimizer (ORCA) is
                # 'on' by default in 4.3.5
                elif _is_rev_gte(_get_rev_num(dbver), _get_rev_num('4.3.4')):
                    dbver = '4.3ORCA'
                else:
                    # only need the first two digits for <= 4.3.4
                    dbver = '.'.join(dbver.split('.')[:2])

            if not os.path.isdir(os.path.join(portdir, dbver)):
                _error("This version is not among the %s versions for which "
                       "MADlib support files have been installed (%s)." %
                       (ports[portid]['name'], ", ".join(supportedVersions)), True)

        # Validate that db platform is correct
        if not _check_db_port(portid):
            _error("Invalid database platform specified.", True)

        # Adjust MADlib directories for this port (if they exist)
        global maddir_conf
        if os.path.isdir(maddir + "/ports/" + portid + "/" + dbver + "/config"):
            maddir_conf = maddir + "/ports/" + portid + "/" + dbver + "/config"
        else:
            maddir_conf = maddir + "/config"

        global maddir_lib
        if os.path.isfile(maddir + "/ports/" + portid + "/" + dbver +
                          "/lib/libmadlib.so"):
            maddir_lib = maddir + "/ports/" + portid + "/" + dbver + \
                "/lib/libmadlib.so"
        else:
            maddir_lib = maddir + "/lib/libmadlib.so"

        # Get the list of modules for this port
        global portspecs
        portspecs = configyml.get_modules(maddir_conf)
    else:
        con_args = None
        dbrev = None

    # Parse COMMAND argument and compare with Ports.yml
    # Debugging...
    # print "OS rev: " + str(rev) + " > " + str(_get_rev_num(rev))
    # print "DB rev: " + str(dbrev) + " > " + str(_get_rev_num(dbrev))

    # Make sure we have the necessary parameters to continue
    if args.command[0] != 'version':
        if not portid:
            _error("Missing -p/--platform parameter.", True)
        if not con_args:
            _error("Unknown problem with database connection string: %s" % con_args, True)

    # COMMAND: version
    if args.command[0] == 'version':
        _print_revs(rev, dbrev, con_args, schema)

    # COMMAND: uninstall/reinstall
    if args.command[0] in ('uninstall',) and (portid == 'hawq' and not is_hawq2):
        _error("madpack uninstall is currently not available for HAWQ", True)

    if args.command[0] in ('uninstall', 'reinstall') and (portid != 'hawq' or is_hawq2):
        if _get_rev_num(dbrev) == [0]:
            _info("Nothing to uninstall. No version found in schema %s." % schema.upper(), True)
            return

        # Find any potential data to lose
        affected_objects = _internal_run_query("""
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
                n1.nspname, relname, attname, typname""" % schema.lower(), True)

        _info("*** Uninstalling MADlib ***", True)
        _info("***********************************************************************************", True)
        _info("* Schema %s and all database objects depending on it will be dropped!" % schema.upper(), True)
        if affected_objects:
            _info("* If you continue the following data will be lost (schema : table.column : type):", True)
            for ao in affected_objects:
                _info('* - ' + ao['schema'] + ' : ' + ao['relation'] + '.' +
                      ao['column'] + ' : ' + ao['type'], True)
        _info("***********************************************************************************", True)
        _info("Would you like to continue? [Y/N]", True)
        go = raw_input('>>> ').upper()
        while go != 'Y' and go != 'N':
            go = raw_input('Yes or No >>> ').upper()

        # 2) Do the uninstall/drop
        if go == 'N':
            _info('No problem. Nothing dropped.', True)
            return

        elif go == 'Y':
            _info("> dropping schema %s" % schema.upper(), verbose)
            try:
                _internal_run_query("DROP SCHEMA %s CASCADE;" % (schema), True)
            except:
                _error("Cannot drop schema %s." % schema.upper(), True)

            _info('Schema %s (and all dependent objects) has been dropped.' % schema.upper(), True)
            _info('MADlib uninstalled successfully.', True)

        else:
            return

    # COMMAND: install/reinstall
    if args.command[0] in ('install', 'reinstall'):
        # Refresh MADlib version in DB, None for GP/PG
        if args.command[0] == 'reinstall':
            print "Setting MADlib database version to be None for reinstall"
            dbrev = None

        _info("*** Installing MADlib ***", True)

        # 1) Compare OS and DB versions.
        # noop if OS <= DB.
        _print_revs(rev, dbrev, con_args, schema)
        if _is_rev_gte(_get_rev_num(dbrev), _get_rev_num(rev)):
            _info("Current MADlib version already up to date.", True)
            return
        # proceed to create objects if nothing installed in DB or for HAWQ < 2.0
        elif dbrev is None or (portid == 'hawq' and not is_hawq2):
            pass
        # error and refer to upgrade if OS > DB
        else:
            _error("""Aborting installation: existing MADlib version detected in {0} schema
                    To upgrade the {0} schema to MADlib v{1} please run the following command:
                    madpack upgrade -s {0} -p {2} [-c ...]
                    """.format(schema, rev, portid), True)

        # 2) Run installation
        try:
            _plpy_check(py_min_ver)
            _db_install(schema, dbrev, args.testcase)
        except:
            _error("MADlib installation failed.", True)

    # COMMAND: upgrade
    if args.command[0] in ('upgrade', 'update'):
        _info("*** Upgrading MADlib ***", True)
        dbrev = _get_madlib_dbrev(schema)

        # 1) Check DB version. If None, nothing to upgrade.
        if not dbrev:
            _info("MADlib is not installed in {schema} schema and there "
                  "is nothing to upgrade. Please use install "
                  "instead.".format(schema=schema.upper()),
                  True)
            return

        # 2) Compare OS and DB versions. Continue if OS > DB.
        _print_revs(rev, dbrev, con_args, schema)
        if _is_rev_gte(_get_rev_num(dbrev), _get_rev_num(rev)):
            _info("Current MADlib version is already up-to-date.", True)
            return

        if float('.'.join(dbrev.split('.')[0:2])) < 1.0:
            _info("The version gap is too large, upgrade is supported only for "
                  "packages greater than or equal to v1.0.", True)
            return

        # 3) Run upgrade
        try:
            _plpy_check(py_min_ver)
            _db_upgrade(schema, dbrev)
        except Exception as e:
            # Uncomment the following lines when debugging
            print "Exception: " + str(e)
            print sys.exc_info()
            traceback.print_tb(sys.exc_info()[2])
            _error("MADlib upgrade failed.", True)

    # COMMAND: install-check
    if args.command[0] == 'install-check':

        # 1) Compare OS and DB versions. Continue if OS = DB.
        if _get_rev_num(dbrev) != _get_rev_num(rev):
            _print_revs(rev, dbrev, con_args, schema)
            _info("Versions do not match. Install-check stopped.", True)
            return

        # Create install-check user
        test_user = ('madlib_' +
                     rev.replace('.', '').replace('-', '_') +
                     '_installcheck')
        try:
            _internal_run_query("DROP USER IF EXISTS %s;" % (test_user), False)
        except:
            _internal_run_query("DROP OWNED BY %s CASCADE;" % (test_user), True)
            _internal_run_query("DROP USER IF EXISTS %s;" % (test_user), True)
        _internal_run_query("CREATE USER %s;" % (test_user), True)

        _internal_run_query("GRANT USAGE ON SCHEMA %s TO %s;" % (schema, test_user), True)

        # 2) Run test SQLs
        _info("> Running test scripts for:", verbose)

        caseset = (set([test.strip() for test in args.testcase.split(',')])
                   if args.testcase != "" else set())

        modset = {}
        for case in caseset:
            if case.find('/') > -1:
                [mod, algo] = case.split('/')
                if mod not in modset:
                    modset[mod] = []
                if algo not in modset[mod]:
                    modset[mod].append(algo)
            else:
                modset[case] = []

        # Loop through all modules
        for moduleinfo in portspecs['modules']:

            # Get module name
            module = moduleinfo['name']

            # Skip if doesn't meet specified modules
            if modset is not None and len(modset) > 0 and module not in modset:
                continue
            # JIRA: MADLIB-1078 fix
            # Skip pmml during install-check (when run without the -t option).
            # We can still run install-check on pmml with '-t' option.
            if not modset and module in ['pmml']:
                continue
            _info("> - %s" % module, verbose)

            # Make a temp dir for this module (if doesn't exist)
            cur_tmpdir = tmpdir + '/' + module + '/test'  # tmpdir is a global variable
            _make_dir(cur_tmpdir)

            # Find the Python module dir (platform specific or generic)
            if os.path.isdir(maddir + "/ports/" + portid + "/" + dbver + "/modules/" + module):
                maddir_mod_py = maddir + "/ports/" + portid + "/" + dbver + "/modules"
            else:
                maddir_mod_py = maddir + "/modules"

            # Find the SQL module dir (platform specific or generic)
            if os.path.isdir(maddir + "/ports/" + portid + "/modules/" + module):
                maddir_mod_sql = maddir + "/ports/" + portid + "/modules"
            else:
                maddir_mod_sql = maddir + "/modules"

            # Prepare test schema
            test_schema = "madlib_installcheck_%s" % (module)
            _internal_run_query("DROP SCHEMA IF EXISTS %s CASCADE; CREATE SCHEMA %s;" %
                                (test_schema, test_schema), True)
            _internal_run_query("GRANT ALL ON SCHEMA %s TO %s;" %
                                (test_schema, test_user), True)

            # Switch to test user and prepare the search_path
            pre_sql = '-- Switch to test user:\n' \
                      'SET ROLE %s;\n' \
                      '-- Set SEARCH_PATH for install-check:\n' \
                      'SET search_path=%s,%s;\n' \
                      % (test_user, test_schema, schema)

            # Loop through all test SQL files for this module
            sql_files = maddir_mod_sql + '/' + module + '/test/*.sql_in'
            for sqlfile in sorted(glob.glob(sql_files), reverse=True):
                # work-around for HAWQ
                algoname = os.path.basename(sqlfile).split('.')[0]
                # run only algo specified
                if module in modset and len(modset[module]) > 0 \
                        and algoname not in modset[module]:
                    continue

                # Set file names
                tmpfile = cur_tmpdir + '/' + os.path.basename(sqlfile) + '.tmp'
                logfile = cur_tmpdir + '/' + os.path.basename(sqlfile) + '.log'

                # If there is no problem with the SQL file
                milliseconds = 0

                # Run the SQL
                run_start = datetime.datetime.now()
                retval = _run_sql_file(schema, maddir_mod_py, module,
                                       sqlfile, tmpfile, logfile, pre_sql)
                # Runtime evaluation
                run_end = datetime.datetime.now()
                milliseconds = round((run_end - run_start).seconds * 1000 +
                                     (run_end - run_start).microseconds / 1000)

                # Check the exit status
                if retval != 0:
                    result = 'FAIL'
                    keeplogs = True
                # Since every single statement in the test file gets logged,
                # an empty log file indicates an empty or a failed test
                elif os.path.isfile(logfile) and os.path.getsize(logfile) > 0:
                    result = 'PASS'
                # Otherwise
                else:
                    result = 'ERROR'

                # Output result
                print "TEST CASE RESULT|Module: " + module + \
                    "|" + os.path.basename(sqlfile) + "|" + result + \
                    "|Time: %d milliseconds" % (milliseconds)

                if result == 'FAIL':
                    _error("Failed executing %s" % tmpfile, False)
                    _error("Check the log at %s" % logfile, False)
            # Cleanup test schema for the module
            _internal_run_query("DROP SCHEMA IF EXISTS %s CASCADE;" % (test_schema), True)

        # Drop install-check user
        _internal_run_query("DROP OWNED BY %s CASCADE;" % (test_user), True)
        _internal_run_query("DROP USER %s;" % (test_user), True)


# -----------------------------------------------------------------------
# Unit tests
# -----------------------------------------------------------------------
class RevTest(unittest.TestCase):

    def setUp(self):
        pass

    def tearDown(self):
        pass

    def test_get_rev_num(self):
        # not using assertGreaterEqual to keep Python 2.6 compatibility
        self.assertTrue(_get_rev_num('4.3.10') >= _get_rev_num('4.3.5'))
        self.assertTrue(_get_rev_num('1.9.10-dev') >= _get_rev_num('1.9.9'))
        self.assertNotEqual(_get_rev_num('1.9.10-dev'), _get_rev_num('1.9.10'))
        self.assertEqual(_get_rev_num('1.9.10'), [1, 9, 10])
        self.assertEqual(_get_rev_num('1.0.0+20130313144700'), [1, 0, 0, '20130313144700'])
        self.assertNotEqual(_get_rev_num('1.0.0+20130313144700'),
                            _get_rev_num('1.0.0-beta+exp.sha.5114f85'))

    def test_is_rev_gte(self):
        # 1.0.0-alpha < 1.0.0-alpha.1 < 1.0.0-alpha.beta <
        #       1.0.0-beta < 1.0.0-beta.2 < 1.0.0-beta.11 < 1.0.0-rc.1 < 1.0.0
        self.assertTrue(_is_rev_gte([], []))
        self.assertTrue(_is_rev_gte([1, 9], [1, None]))
        self.assertFalse(_is_rev_gte([1, None], [1, 9]))

        self.assertTrue(_is_rev_gte(_get_rev_num('4.3.10'), _get_rev_num('4.3.5')))
        self.assertTrue(_is_rev_gte(_get_rev_num('1.9.0'), _get_rev_num('1.9.0')))
        self.assertTrue(_is_rev_gte(_get_rev_num('1.9.1'), _get_rev_num('1.9.0')))
        self.assertTrue(_is_rev_gte(_get_rev_num('1.9.1'), _get_rev_num('1.9')))
        self.assertTrue(_is_rev_gte(_get_rev_num('1.9.0'), _get_rev_num('1.9.0-dev')))
        self.assertTrue(_is_rev_gte(_get_rev_num('1.9.1'), _get_rev_num('1.9-dev')))
        self.assertTrue(_is_rev_gte(_get_rev_num('1.9.0-dev'), _get_rev_num('1.9.0-dev')))
        self.assertTrue(_is_rev_gte([1, 9, 'rc', 1], [1, 9, 'dev', 0]))

        self.assertFalse(_is_rev_gte(_get_rev_num('1.9.1'), _get_rev_num('1.10')))
        self.assertFalse(_is_rev_gte([1, 9, 'dev', 1], [1, 9, 'rc', 0]))
        self.assertFalse(_is_rev_gte([1, 9, 'alpha'], [1, 9, 'alpha', 0]))
        self.assertFalse(_is_rev_gte([1, 9, 'alpha', 1], [1, 9, 'alpha', 'beta']))
        self.assertFalse(_is_rev_gte([1, 9, 'alpha.1'], [1, 9, 'alpha.beta']))
        self.assertFalse(_is_rev_gte([1, 9, 'beta', 2], [1, 9, 'beta', 4]))
        self.assertFalse(_is_rev_gte([1, 9, 'beta', '1'], [1, 9, 'rc', '0']))
        self.assertFalse(_is_rev_gte([1, 9, 'rc', 1], [1, 9, 0]))
        self.assertFalse(_is_rev_gte([1, 9, '0.2'], [1, 9, '0.3']))
        self.assertFalse(_is_rev_gte([1, 9, 'build2'], [1, 9, 'build3']))

        self.assertFalse(_is_rev_gte(_get_rev_num('1.0.0+20130313144700'),
                                     _get_rev_num('1.0.0-beta+exp.sha.5114f85')))


# ------------------------------------------------------------------------------
# Start Here
# ------------------------------------------------------------------------------
if __name__ == "__main__":
    RUN_TESTS = False

    if RUN_TESTS:
        unittest.main()
    else:
        # Run main
        main(sys.argv[1:])

        # Optional log files cleanup
        # keeplogs and tmpdir are global variables
        if not keeplogs:
            shutil.rmtree(tmpdir)
        else:
            print "INFO: Log files saved in " + tmpdir
