#!/usr/bin/python
#
# Script/module to call 'make' for each MADlib method port
# in the MADlib Config.yml file.

import yaml
import os
import re
import subprocess
import madpy
import madpy.madpack.configyml

# customize the config.mk according to Config.yml and write into methdir
# current customization is just to set the SCHEMA_PLACEHOLDER to the 
# target_schema in Config.yml
def __create_config_mk(conf, methdir):
    newfile = methdir + '/config.mk'
    if os.path.exists(newfile):
        print "old " + newfile + " found; must be deleted"
        exit(2)
    try:
        mkfd = open(newfile, 'w')
    except IOError:
        print "cannot create " + newfile
        exit(2)

    for line in open(madpy.__path__[0]+'/config/config.mk').readlines():
        mkfd.write(re.sub('SCHEMA_PLACEHOLDER', conf['target_schema'], line))

    mkfd.close()

# clean up the config.mk    
def __remove_config_mk(methdir):
    os.remove(methdir+'/config.mk')

# run 'make <mkarg>' for each method in the conf    
def make_methods(mkarg, conf):
    for m in conf['methods']:
        mdir = madpy.__path__[0]+'/../madlib/' + m['name'] + '/src/' + m['port'] + '/'
        # print "changing directory to " + mdir
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
        # generate config.mk in the method directory
        __create_config_mk(conf, os.getcwd())    
        if install['module'] != None:
            subprocess.call(['make', mkarg], stderr = subprocess.PIPE)
        # and remove config.mk
        __remove_config_mk(os.getcwd())
        os.chdir(curdir)
