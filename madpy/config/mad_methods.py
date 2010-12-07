#!/usr/bin/python
import yaml
import os
import subprocess
import configyml
    
def install_methods():
    conf = configyml.get_config('..')

    for m in conf['methods']:
        mdir = '../../madlib/' + m['name'] + '/src/' + m['port'] + '/'
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
            subprocess.call(['make','install'], stderr = subprocess.PIPE)
        os.chdir(curdir)
            
if __name__ == "__main__":
    install_methods()