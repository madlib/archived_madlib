#!/usr/bin/env python
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

"""
@file sort-module.py

Sort input strings based on the module name in them such that
the module dependencies are resolved.  Example:

  $ sort-module.py a/regress/b a/svec/b a/array_ops/b
  > a/svec/b a/regress/b a/array_opts/b

Note this script assumes to be run at the exact current directory,
so you need change directory first to run it.
"""

import re
import sys

import configyml


def get_modules_in_order():
    """ Return a mapping between module_name and the order of the module.

    Returns:
        Dict. The output is of the form:
            {'module_a': 1, 'module_b': 2, ...}
    """
    portspecs = configyml.get_modules("../config")
    # get_modules returns a dictionary in following form:
    #   { 'modules': [{'name': 'array_ops'}, {'name': 'bayes'}, ...,
    #                 {'depends': ['array_ops'], 'name': 'stats'}, ... ]
    #                 }
    # The list of modules is pre-sorted using topological sort
    module_order = dict()
    for i, moduleinfo in enumerate(portspecs['modules']):
        module_order[moduleinfo['name']] = i
    return module_order
# ----------------------------------------------------------------------


module_order = get_modules_in_order()

# Not all modules are captured in the config file. For a missing module, assume
# the sort rank is maximum (i.e. missing modules are installed last).
MAX_RANK = len(module_order) + 1


def find_order(path):
    """ Return the position of a given file within the module order

    Args:
        @param: path: str, The path for a single SQL file. This path is assumed
                to have the form: '.../modules/<module_name>/.../<file_name>.sql'
    """
    mod_name = re.match(r'.+/modules/(.+)/.+', path).group(1)
    return module_order.get(mod_name, MAX_RANK)


def main(file_paths):
    """
    Args:
        @param: file_paths: List of paths to SQL files, where each path
                is of the form: '.../modules/<module_name>/...'.
    """
    file_order = sorted(file_paths, key=find_order)
    print " ".join(file_order)


if __name__ == '__main__':
    main(sys.argv[1:])
