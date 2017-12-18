#!/usr/bin/env python#
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Madpack utilities
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

from itertools import izip_longest
import os
import re
import sys
import subprocess
import unittest


# Some read-only variables
this = os.path.basename(sys.argv[0])    # name of this script


def error_(src_name, msg, stop=False):
    """
    Error message wrapper
        @param msg error message
        @param stop program exit flag
    """
    # Print to stdout
    print("{0}: ERROR : {1}".format(src_name, msg))
    # stack trace is not printed
    if stop:
        exit(2)
# ------------------------------------------------------------------------------


def info_(src_name, msg, verbose=True):
    """
    Info message wrapper (verbose)
        @param msg info message
        @param verbose prints only if True (prevents caller from performing a check)
    """
    if verbose:
        print("{0}: INFO : {1}".format(src_name, msg))
# ------------------------------------------------------------------------------


def run_query(sql, con_args, show_error=True):
    # Define sqlcmd
    sqlcmd = 'psql'
    delimiter = ' <$madlib_delimiter$> '

    # Test the DB cmd line utility
    std, err = subprocess.Popen(['which', sqlcmd], stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE).communicate()
    if not std:
        error_(this, "Command not found: %s" % sqlcmd, True)

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
            error_("SQL command failed: \nSQL: %s \n%s" % (sql, err), False)
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
    except Exception:
        pass

    return results
# ------------------------------------------------------------------------------


def get_madlib_dbrev(con_args, schema):
    """
    Read MADlib version from database
        @param con_args database conection object
        @param schema MADlib schema name
    """
    try:
        n_madlib_versions = int(run_query(
            """
                SELECT count(*) AS cnt FROM pg_tables
                WHERE schemaname='{0}' AND tablename='migrationhistory'
            """.format(schema),
            con_args,
            True)[0]['cnt'])
        if n_madlib_versions > 0:
            madlib_version = run_query(
                """
                    SELECT version
                    FROM {0}.migrationhistory
                    ORDER BY applied DESC LIMIT 1
                """.format(schema),
                con_args,
                True)
            if madlib_version:
                return madlib_version[0]['version']
    except Exception:
        error_(this, "Failed reading MADlib db version", True)
    return None
# ------------------------------------------------------------------------------


def get_dbver(con_args, portid):
    """ Read version number from database (of form X.Y) """
    try:
        versionStr = run_query("SELECT pg_catalog.version()", con_args, True)[0]['version']
        if portid == 'postgres':
            match = re.search("PostgreSQL[a-zA-Z\s]*(\d+\.\d+)", versionStr)
        elif portid == 'greenplum':
            # for Greenplum the 3rd digit is necessary to differentiate
            # 4.3.5+ from versions < 4.3.5
            match = re.search("Greenplum[a-zA-Z\s]*(\d+\.\d+\.\d+)", versionStr)
        elif portid == 'hawq':
            match = re.search("HAWQ[a-zA-Z\s]*(\d+\.\d+)", versionStr)
        return None if match is None else match.group(1)
    except Exception:
        error_(this, "Failed reading database version", True)
# ------------------------------------------------------------------------------


def is_rev_gte(left, right):
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


def get_rev_num(rev):
    """
    Convert version string into number for comparison
        @param rev version text
                It is expected to follow Semantic Versioning (semver.org)
                Valid inputs:
                    1.9.0, 1.10.0, 2.5.0
                    1.0.0-alpha, 1.0.0-alpha.1, 1.0.0-0.3.7, 1.0.0-x.7.z.92
                    1.0.0+20130313144700, 1.0.0-beta+exp.sha.5114f85
    Returns:
        List. The numeric parts of version string are converted to int and
        non-numeric parts are returned as is.
        Invalid versions strings returned as [0]

    Examples:
        '1.9.0' -> [1, 9, 0]
        '1.9' -> [1, 9, 0]
        '1.9-alpha' -> [1, 9, 'alpha']
        '1.9-alpha+dc65ab' -> [1, 9, 'alpha', 'dc65ab']
        'a.123' -> [0]
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
    except (ValueError, TypeError):
        # invalid revision
        return [0]
# ------------------------------------------------------------------------------


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
        self.assertTrue(get_rev_num('4.3.10') >= get_rev_num('4.3.5'))
        self.assertTrue(get_rev_num('1.9.10-dev') >= get_rev_num('1.9.9'))
        self.assertNotEqual(get_rev_num('1.9.10-dev'), get_rev_num('1.9.10'))
        self.assertEqual(get_rev_num('1.9.10'), [1, 9, 10])
        self.assertEqual(get_rev_num('abc1.9.10'), [0])
        self.assertEqual(get_rev_num('1.0.0+20130313144700'),
                         [1, 0, 0, '20130313144700'])
        self.assertNotEqual(get_rev_num('1.0.0+20130313144700'),
                            get_rev_num('1.0.0-beta+exp.sha.5114f85'))

    def test_is_rev_gte(self):
        # 1.0.0-alpha < 1.0.0-alpha.1 < 1.0.0-alpha.beta <
        #       1.0.0-beta < 1.0.0-beta.2 < 1.0.0-beta.11 < 1.0.0-rc.1 < 1.0.0
        self.assertTrue(is_rev_gte([], []))
        self.assertTrue(is_rev_gte([1, 9], [1, None]))
        self.assertFalse(is_rev_gte([1, None], [1, 9]))

        self.assertTrue(is_rev_gte(get_rev_num('4.3.10'), get_rev_num('4.3.5')))
        self.assertTrue(is_rev_gte(get_rev_num('1.9.0'), get_rev_num('1.9.0')))
        self.assertTrue(is_rev_gte(get_rev_num('1.9.1'), get_rev_num('1.9.0')))
        self.assertTrue(is_rev_gte(get_rev_num('1.9.1'), get_rev_num('1.9')))
        self.assertTrue(is_rev_gte(get_rev_num('1.9.0'), get_rev_num('1.9.0-dev')))
        self.assertTrue(is_rev_gte(get_rev_num('1.9.1'), get_rev_num('1.9-dev')))
        self.assertTrue(is_rev_gte(get_rev_num('1.9.0-dev'), get_rev_num('1.9.0-dev')))
        self.assertTrue(is_rev_gte([1, 9, 'rc', 1], [1, 9, 'dev', 0]))

        self.assertFalse(is_rev_gte(get_rev_num('1.9.1'), get_rev_num('1.10')))
        self.assertFalse(is_rev_gte([1, 9, 'dev', 1], [1, 9, 'rc', 0]))
        self.assertFalse(is_rev_gte([1, 9, 'alpha'], [1, 9, 'alpha', 0]))
        self.assertFalse(is_rev_gte([1, 9, 'alpha', 1], [1, 9, 'alpha', 'beta']))
        self.assertFalse(is_rev_gte([1, 9, 'alpha.1'], [1, 9, 'alpha.beta']))
        self.assertFalse(is_rev_gte([1, 9, 'beta', 2], [1, 9, 'beta', 4]))
        self.assertFalse(is_rev_gte([1, 9, 'beta', '1'], [1, 9, 'rc', '0']))
        self.assertFalse(is_rev_gte([1, 9, 'rc', 1], [1, 9, 0]))
        self.assertFalse(is_rev_gte([1, 9, '0.2'], [1, 9, '0.3']))
        self.assertFalse(is_rev_gte([1, 9, 'build2'], [1, 9, 'build3']))

        self.assertFalse(is_rev_gte(get_rev_num('1.0.0+20130313144700'),
                                    get_rev_num('1.0.0-beta+exp.sha.5114f85')))


if __name__ == "__main__":
    unittest.main()
