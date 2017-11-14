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

import re
from itertools import izip_longest

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
