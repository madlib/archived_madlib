#!/usr/bin/python
#
# Interactive post installation hook for Greenplum 
# Purpose: deploys shared object files to segment nodes

import os, sys
import subprocess

# get and check GPHOME 
gphome = os.getenv('GPHOME')
while (not os.path.isdir( gphome)):    
    gphome = raw_input("> GPHOME not found. Enter full path to your Greenplum installation: ")

# check the MADLIB dir
madlibdir = gphome + '/lib/postgresql/madlib'
if (not os.path.isdir( madlibdir)):
    print "> " + madlibdir + " does not exist. Nothing to push to segment nodes."
    exit(1)

print "> deploying object code libraries to Greenplum segment nodes"

# get & check host_file
hostfile = raw_input("> enter full path to the host file: ")
while (not os.path.isfile( hostfile)):    
    hostfile = raw_input("> file does not exist. Try again: ")

# use SCP to push the MADLIB dir with SO files
n=0
try:
    f = open( hostfile, "r")
    line = f.readline() 
except: 
    print sys.exc_info()[0]
    print "Unexpected error during opening the file: " + hostfile

try:
    while line:
        n+=1
        line = line.replace("\n","");
        print 'copying files to ' + line
        subprocess.call(['scp -pr ' + madlibdir + ' ' + line + ':' + gphome + '/lib/postgresql/' ], stdout = subprocess.PIPE, shell=True)
        line = f.readline()
except: 
    print sys.exc_info()
    print "Unexpected error during file copy (scp) to " + line

# done
print "> libraries copied to " + str(n) + " segment node(s)"
