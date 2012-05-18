#!/usr/bin/python

"""
sort-module.py

Sort input strings based on the module name in them such that
the module dependencies are resolved.  Example:

  $ sort-module.py a/regress/b a/svec/b a/array_ops/b
  > a/svec/b a/regress/b a/array_opts/b

Note this script assumes to be run at the exact current directory,
so you need change directory first to run it.
"""

import configyml

portspecs = configyml.get_modules("../config")

def find_order(path):
    for i, moduleinfo in enumerate(portspecs['modules']):
        modname = moduleinfo['name']
        if modname in path:
            return i
    # return as the last if not found.
    return len(portspecs['modules'])

def main(args):
    print " ".join(sorted(args, key = find_order))

if __name__ == '__main__':
    import sys
    main(sys.argv[1:])
