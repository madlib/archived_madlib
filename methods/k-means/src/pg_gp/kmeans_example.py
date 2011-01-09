#!/usr/bin/env python
#
# Sample Run of MADlib k-means clustering
#
# Usage: kmeans_run --help

#
# Imports
#
import sys,getpass

try:
    import plpy
except Exception, e:
    sys.stderr.write("unable to import MADlib plpy module. Please check your $PYTHONPATH\n")
    sys.exit(1)
    
try:
    from madlib import kmeans
except ImportError:
    sys.stderr.write("unable to import MADlib kmeans module. Please check your $PYTHONPATH\n")
    sys.exit(2)

# 
# Usage
#
def usage( error = None):
    print '''
kmeans_run [options] 

Options:
    -h hostname: host to connect to
    -p port:     port to connect to
    -U username: user to connect as
    -d database: database to connect to
    --help
    --version
    ''';
    sys.stdout.flush()
    if error:
        sys.stderr.write('ERROR: ' + error + '\n')
        sys.stderr.write('\n')
        sys.stderr.flush()
    sys.exit(2)
    
#
# Arguments
#
db_host = None;
db_port = None;
db_user = None;
db_database = None;
db_pass = None;
argv = sys.argv[1:]
while argv:
    try:
        try:
            if argv[0]=='-h':
                db_host = argv[1]
                argv = argv[2:]
            elif argv[0]=='-p':
                db_port = int(argv[1])
                argv = argv[2:]
            elif argv[0]=='-U':
                db_user = argv[1]
                argv = argv[2:]
            elif argv[0]=='-d':
                db_database = argv[1]
                argv = argv[2:]
            elif argv[0]=='--version':
                sys.stderr.write("kmeans version 0.1 1\n")
                sys.exit(0)
            elif argv[0]=='--help':
                usage()
            else:
                break
        except IndexError:
            sys.stderr.write("ERROR: Option %s needs a parameter.\n"%argv[0])
            sys.exit(2)
    except ValueError:
        sys.stderr.write("ERROR: Parameter for option %s must be an integer.\n"%argv[0])
        sys.exit(2)

#
# Setup Connection
#  
if db_host == None:
    usage( 'host not defined');
if db_port == None:
    usage( 'port not defined');
if db_user == None:
    usage( 'user not defined');
if db_database == None:
    usage( 'database not defined');
if db_pass == None:
    getpass.getpass()
    
plpy.connect ( db_database, db_host, db_port, db_user, db_pass)

#
# Main - Example (requires running "create_input.sql" before this)
#
# For parameter definition see README.
#
print kmeans.kmeans_run( 'madlib.kmeans_input', 10, 1, 'testrun', 'madlib');

plpy.close()