#!/usr/bin/python
import yaml
import os
import argparse
import configyml
from migrate.migrations import *



def prep(targetdir):
    conf = configyml.get_config()
    version = configyml.get_version()
    mig = MadlibMigration(conf['dbapi2'], conf['connect_args'])
    if not os.path.exists(targetdir):
        os.mkdir(targetdir)   
    else:
        print "scripts directory already exists"
        sys.exit(2)
        
    forwards = []
    backwards = []

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
            install['fw'] and install['bw']
        except:
            print "method " + m['name'] + " misconfigured: Install.yml misformatted"
            sys.exit(2)
        if install['fw'] != None:
            forwards.append(os.getcwd()+"/"+install['fw'])
        if install['bw'] != None:
            backwards.append(os.getcwd()+"/"+install['bw'])
        os.chdir(curdir)
        
    fname = mig.generate(targetdir, configyml.get_version(), forwards, backwards)
    
def install_number(migdir, num):
    conf = configyml.get_config()
    m = MadlibMigration(conf['dbapi2'], conf['connect_args'])
    try:
        os.chdir(migdir)
    except OSError:
        print "directory " + migdir + " does not exist"
        exit(2)
    try:
        m.setup()
    except MadlibMigrationNoSchemaError:
        print "no schema named 'madlib' in database"
        exit(2)
        
    if num == None:
        m.migrate(conf['dbapi2'], conf['connect_args'])
    else:
        m.migrate(conf['dbapi2'], conf['connect_args'], num)
    
def uninstall(migdir):
    install_number(migdir, -1)
        
def install(migdir):
    install_number(migdir, None)
    
def main(argv):
    migdir = os.getcwd() + "/scripts"

    parser = argparse.ArgumentParser(description='configure madlib installation', argument_default=False)
    parser.add_argument('-p', '--prep-only', dest='preponly', action="store_true",
                        help="prepare scripts but do not install")
    parser.add_argument('-i', '--install-only', dest='installonly', action="store_true",
                        help="install via pre-existing scripts")
    parser.add_argument('-u', '--uninstall', dest='remove', action="store_true",
                        help="remove madlib library from database")

    args = parser.parse_args()

    if args.remove:
        if not os.path.exists(migdir):
            print "scripts directory missing"
            sys.exit(2)
        uninstall(migdir)
        sys.exit()

    if not args.installonly:        
        prep(migdir)
    if not args.preponly:
        install(migdir)
    
if __name__ == "__main__":
    main(sys.argv[1:])