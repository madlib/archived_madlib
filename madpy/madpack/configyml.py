# routines to pull information out of the Config.yml and Version.yml files.

import yaml

def get_config(configdir):
    # fname = madpy.__path__[0] + '/Config.yml'
    fname = configdir + '/Config.yml'
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
	    # sanitize schema names to avoid SQL injection!  only alphanumerics
	    if not re.match(conf['target_schema'], '[A-Za-z0-9\.]+'):
	        print 'illegal character in target_schema of Config.yml'
	        exit(2)
	except:
		print "malformed Config.yml: no target_schema"
		exit(2)
		
    return conf

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
