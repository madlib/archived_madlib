#!/usr/bin/python
#
# Script to build/compile MADlib methods

import yaml
import os
import re
import glob
import sys
import subprocess
import fileinput
from distutils import file_util

## 
# Prepares the source files 
# @param rootdir the root dir of the source code
# @param port the port object with basic port info
# @param portspecs the parsed <port_id>.yml object (details about each port)
# @param stagedir the stage dir for the compilation
##
def __prep_src_files( rootdir, port, portspecs, stagedir):

    # Copy scr files into staging area
    for method in portspecs['methods']:
        mask = rootdir + '/methods/' + method['name'] + '/src/' + port['src'] + '/*.[c|h]'
        for file in glob.glob( mask):
            file_util.copy_file( file, stagedir)
            
    # Comment out PG_MODULE_MAGIC 
    for file in glob.glob( stagedir + '/*.c'):     
        for line in fileinput.FileInput( file, inplace=1):
            if re.match('^ *PG_MODULE_MAGIC', line): 
                line = re.sub( '^ *PG_MODULE_MAGIC', '//PG_MODULE_MAGIC', line)
            sys.stdout.write (line)
    
              
## 
# Creates a global Makefile for all the methods 
# @param confdir the directory with configuration files
# @param port the port object with basic port info
# @param portspecs the parsed <port_id>.yml object (details about each port)
# @param stagedir the stage dir for the compilation
##
def __prep_makefile( confdir, port, portspecs, stagedir):
    
    temp = confdir + '/' + port['id'] + '.mk'
    new = stagedir + '/Makefile'
    
    try:
        tempfd = open( temp, 'r')
    except IOError:
        print "cannot open file: " + temp
        exit(2)

    try:
        newfd = open( new, 'w')
    except IOError:
        print "cannot create file: " + new
        exit(2)
        
    # Make a list of source files
    src_list = ''
    for file in glob.glob( stagedir + '/*.c'):
        src_list = src_list + ' ' + os.path.basename( file)
     
    # Write the new Makefile    
    for line in tempfd.readlines():
        line1 = re.sub('<source_list>', src_list, line)
        newfd.write( line1)
        
    tempfd.close()
    newfd.close()
    
## 
# Prepares the stage env for building all methods into one SO file 
# @param rootdir the root dir of the source code
# @param confdir the directory with configuration files
# @param port the port object with basic port info
# @param portspecs the parsed <port_id>.yml object (details about each port)
# @param stagedir the stage dir for the compilation
##
def prep_make_env( rootdir, confdir, port, portspecs, stagedir):

    print "  > preparing env for global Makefile"

    __prep_src_files( rootdir, port, portspecs, stagedir)
    
    __prep_makefile( confdir, port, portspecs, stagedir)
    
## 
# Runs 'make <mkarg>' for each method in the CONF list using SRC dir
# @param portspecs the parsed <port_id>.yml object
# @param stagedir the stage dir
# @param mkarg the argument to the make command (install, clean, etc.)
##
def make_all_methods( portspecs, stagedir, mkarg):
    print "  > running: make " + mkarg
    
    curdir = os.getcwd()
    
    try:
        os.chdir(stagedir)
    except OSError:
        print "directory " + stagedir + " does not exist"
        exit(2)                              

    # Execute make            
    if mkarg == '':
        subprocess.call('make') #, env=portenv)                
    else:
        subprocess.call(['make', mkarg]) #, env=portenv)

    # Go back to the initial dir
    os.chdir(curdir)