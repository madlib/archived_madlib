#!/usr/env python

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

import re
import sys
from collections import namedtuple

""" Convert install-check results into a standardized JUnit XML format

Example of JUnit output:

<?xml version="1.0" encoding="UTF-8"?>
<testsuite tests="3">
    <testcase classname="foo1" name="ASuccessfulTest"/>
    <testcase classname="foo2" name="AnotherSuccessfulTest"/>
    <testcase classname="foo3" name="AFailingTest">
        <failure type="NotEnoughFoo"> details about failure </failure>
    </testcase>
</testsuite>
"""


TestResult = namedtuple("TestResult", 'name suite status duration')


def _test_result_factory(install_check_log):
    """
    Args:
        @param install_check_log: File name containing results from install-check

    Returns:
        Next result of type test_result
    """
    with open(install_check_log, 'r') as ic_log:
        for line in ic_log:
            m = re.match(r"^TEST CASE RESULT\|Module: (.*)\|(.*)\|(.*)\|Time: ([0-9]+)(.*)", line)
            if m:
                yield TestResult(name=m.group(2), suite=m.group(1),
                                 status=m.group(3), duration=m.group(4))
# ----------------------------------------------------------------------


def _add_header(out_log, n_tests):
    header = ['<?xml version="1.0" encoding="UTF-8"?>',
              '<testsuite tests="{0}">'.format(n_tests), '']
    out_log.write('\n'.join(header))


def _add_footer(out_log):
    header = ['', '</testsuite>']
    out_log.write('\n'.join(header))


def _add_test_case(out_log, test_results):
    for res in test_results:
        try:
            # convert duration from milliseconds to seconds
            duration = float(res.duration)/1000
        except TypeError:
            duration = 0.0
        output = ['<testcase classname="{t.suite}" name="{t.name}" '
                  'status="{t.status}" time="{d}">'.
                  format(t=res, d=duration)]
        output.append('</testcase>')
        out_log.write('\n'.join(output))


def main(install_check_log, test_output_log):

    # need number of test results - so have to create the iterable
    all_test_results = [i for i in _test_result_factory(install_check_log)]

    with open(test_output_log, 'w') as out_log:
        _add_header(out_log, len(all_test_results))
        _add_test_case(out_log, all_test_results)
        _add_footer(out_log)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
