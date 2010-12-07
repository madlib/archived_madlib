import yaml
import madpy
import madlib

def madpy_dir():
    return madpy.__path__[0]

def madlib_dir():
    return madlib.__path__[0]

def get_config():
    fname = madpy.config.configyml.madpy_dir() + '/config/Config.yml'
    try:
        fd = open(fname)
        conf = yaml.load(fd)
    except:
        print "missing or malformed Config.yml"
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
		
    return conf

def get_version():
    try:
        conf = yaml.load(open(madpy_dir() + '/config/Version.yml'))
    except:
        print "missing or malformed Version.yml"
        exit(2)
    try:
        conf['version']
    except:
        print "malformed Version.yml"
        exit(2)
    return str(conf['version'])
