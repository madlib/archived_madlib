import yaml
import os
import argparse
import subprocess
import madpy.config.configyml
import madlib
    
def install_methods():
    conf = madpy.config.configyml.get_config()

    for m in conf['methods']:
        mdir = madlib.__path__[0] + '/' + m['name'] + '/src/' + m['port'] + '/'
        print "changing directory to " + mdir
        curdir = os.getcwd()
        try:
            os.chdir(mdir)
        except OSError:
            print "directory " + mdir + " does not exist"
            exit(2)            
        try:
            install = yaml.load(open('Install.yml'))
        except:
            print "method " + m['name'] + " misconfigured: missing Install.yml file"
            sys.exit(2)
        try:
            install['module']
        except:
            print "method " + m['name'] + " misconfigured: Install.yml missing module"
            sys.exit(2)
        if install['module'] != None:
            subprocess.check_call(['make'], stderr = subprocess.PIPE)
        os.chdir(curdir)
            
