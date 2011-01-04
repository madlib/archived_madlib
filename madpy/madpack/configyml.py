## \defgroup configyml
# \ingroup madpack
# routines to pull information out of the Config.yml, Version.yml, and 
# Install.yml files.
import yaml
import re
from itertools import chain

##
# A Python exception class for our use
class MadPackConfigError(Exception):
     def __init__(self, value):
         self.value = value
     def __str__(self):
         return repr(self.value)

## return name of configuration file
# @param configdir directory where we can find Config file
def get_configfile(configdir):
    return configdir + '/Config.yml'
    
## load conf object from Config.yml file
# typical Config.yml file looks like this:
        #  dbapi2:
        #     psycopg2
        #     
        # connect_args:
        #     - dbname=joeh
        #     - dbuser=joeh
        #     
        # target_schema:
        #     madlib
        # 
        # methods:
        #     - name: sketch
        #       port: extended_sql/pg_gp
# @param configdir the directory where we can find the Config.yml file
# @first_install whether this is being called from setup.py
def get_config(configdir, first_install):
    # fname = madpy.__path__[0] + '/Config.yml'
    fname = get_configfile(configdir)
    try:
        fd = open(fname)
    except:
        print "missing " + fname
        raise
        exit(2)
    try:
        conf = yaml.load(fd)
    except:
        print "yaml format error: Config.yml"
        exit(2)
    try:
        conf['methods']
    except:
        print "malformed Config.yml: no methods"
        exit(2)
    if not first_install:
        conf = topsort_methods(conf)   
    try:
        conf['connect_args']
    except:
        print "malformed Config.yml: no connect_args"
        exit(2)
    try:
        conf['dbapi2']
    except:
        print "malformed Config.yml: no dbapi2"
        exit(2)
    try:
        conf['prep_flags']
    except:
        conf['prep_flags'] = ""
        # print "malformed Config.yml: no prep_flags"
        # exit(2)
    try:
        # sanitize schema names to avoid SQL injection!  only alphanumerics and a few special chars
        m = re.match('[\w0-9\.\_\-]+', conf['target_schema'])
        if not m or m.group() != conf['target_schema']:
            print 'target_schema ' + \
                  conf['target_schema'] + " not allowed; must use alphanumerics, '.', '_' and '-'"
            exit(2)
    except:
        print "malformed Config.yml: no target_schema"
        exit(2)
    return conf

## load version string from Version.yml file
# typical Version.yml file:
    # version: 0.01
# @param configdir the directory where we can find the Config.yml file
def get_version(configdir):
    try:
        conf = yaml.load(open(configdir + '/Version.yml'))
    except:
        print "missing or malformed Version.yml"
        exit(2)
    try:
        conf['version']
    except:
        print "malformed Version.yml"
        exit(2)
    return str(conf['version'])
    
def flatten(listOfLists):
    "Flatten one level of nesting"
    return chain.from_iterable(listOfLists)

## quick and dirty topological sort
## currently does dumb cycle detection.
# @param depdict an edgelist dictionary, e.g. {'b': ['a'], 'z': ['m', 'n'], 'm': ['a', 'b']}
def topsort(depdict):
    out = dict()
    candidates = set()
    curlevel = 0

    while len(depdict) > 0:
        found = 0  # flag to check if we find anything new this iteration
        newdepdict = dict()
        # find the keys with no values
        keynoval = filter(lambda t: t[1] == [], depdict.iteritems())
        # find the values that are not keys
        valnotkey = set(flatten(depdict.itervalues())) - set(depdict.iterkeys())

        candidates = set([k[0] for k in keynoval]) | valnotkey
        for c in candidates:
            if c not in out:
                found += 1
                out[c] = curlevel

        for k in depdict.iterkeys():
            if depdict[k] != []:
                newdepdict[k] = filter(lambda v: v not in valnotkey, depdict[k])
        # newdepdict = dict(newdepdict)
        if newdepdict == depdict:
            raise MadPackConfigError(str(depdict))
        else:
            depdict = newdepdict
        if found > 0:
            curlevel += 1
    
    return out

## top-sort the methods in conf
# @param conf a madpack configuration
def topsort_methods(conf):
    import madpy

    depdict = dict()
    for m in conf['methods']:
        mdir = madpy.__path__[0]+'/../madlib/' + m['name'] + '/src/' + m['port'] + '/'
        try:
            install = yaml.load(open(mdir+'Install.yml'))
        except:
            print "method " + m['name'] + " misconfigured: missing Install.yml file"
            sys.exit(2)
        if 'depends' in install:
            depdict[m['name']] = install['depends']
        else:
            depdict[m['name']] = []
    try:
        method_dict = topsort(depdict)
    except MadPackConfigError as e:
        raise MadPackConfigError("invalid cyclic dependency between methods: " + e.value + "; check Install.yml files")
    missing = set(method_dict.keys()) - set(depdict.keys())
    inverted = dict()
    if len(missing) > 0:
        for k in depdict.iterkeys():
            for v in depdict[k]:
                if v not in inverted:
                    inverted[v] = set()
                inverted[v].add(k)
        print "required methods missing from Config.yml: " 
        for m in missing:
            print  "    " + m + " (required by " + str(list(inverted[m])) + ")"
        exit(2)
    conf['methods'] = sorted(conf['methods'], key=lambda m:method_dict[m['name']])
    return conf