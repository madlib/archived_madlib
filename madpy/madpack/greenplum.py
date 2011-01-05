#!/usr/bin/python
#
# Interactive post installation hook for Greenplum 
# Purpose: deploys shared object files to segment nodes

import os, sys
import subprocess

print "Deploying object code libraries to Greenplum segment nodes."

# get & check host_file
hostfile = raw_input("Enter full path to the host file: ")
while (not os.path.isfile( hostfile)):    
    hostfile = raw_input("File does not exist. Try again: ")

# check and get GPHOME 
gphome = os.getenv('GPHOME')
while (not os.path.isdir( gphome)):    
    gphome = raw_input("GPHOME not found. Enter full path to your Greenplum installation: ")

# use SCP to push the SO files
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
        subprocess.call(['scp ' + gphome + '/lib/postgresql/*.so ' + line + ':' + gphome + '/lib/postgresql/'], stdout = subprocess.PIPE, shell=True)
        line = f.readline()
except: 
    print sys.exc_info()
    print "Unexpected error during file copy (scp) to " + line

# done
print "Libraries copied to " + str(n) + " segment node(s)."