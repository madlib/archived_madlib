#!/usr/bin/python
import yaml
import os
import argparse
import subprocess
import configyml
    
conf = configyml.get_config()

for m in conf['methods']:
    mdir = '../methods/' + m['name'] + '/src/' + m['port'] + '/'
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
        subprocess.check_call(['make',install['module']+'.so'], stderr = subprocess.PIPE)
    os.chdir(curdir)
            