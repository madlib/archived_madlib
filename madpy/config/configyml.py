import yaml
import madpy

def madpy_dir():
    return madpy.__path__[0]

def get_config():
    fname = madpy.config.configyml.madpy_dir() + '/Config.yml'
    try:
        fd = open(fname)
    except:
        print "missing " + fname
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
		
    return conf

def get_version():
    try:
        conf = yaml.load(open(madpy_dir() + '/Version.yml'))
    except:
        print "missing or malformed Version.yml"
        exit(2)
    try:
        conf['version']
    except:
        print "malformed Version.yml"
        exit(2)
    return str(conf['version'])
