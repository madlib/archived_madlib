import os
import shutil
import sqlparse
import sys
import imp
import traceback
import hashlib

mig_dir = "."
default_prefix_len = 3

mig_prolog = """#!/usr/bin/python
import sys
from madpy.migrate.migrations import MadlibMigration

class Migration(MadlibMigration):
"""

mig_forwards_prolog = """\tdef forwards(self):\n\t\tcur = self.dbconn.cursor()\n"""
mig_backwards_prolog = """\tdef backwards(self):\n\t\tcur = self.dbconn.cursor()\n"""

class MadlibMigrationError(Exception):
    pass
    
class MadlibMigration:
    def __init__(self, api, connect_args):
        dbapi2 = __import__(api, globals(), locals(), [], -1)
        
        # Connect to DB
        self.dbconn = dbapi2.connect(*connect_args)

    def __del__(self):
        self.dbconn.close()
        
    def __prefix_to_n(self,instr,prefix,n):
        return "".join([prefix for i in range(0,n - len(instr))]) + instr

    # from http://code.davidjanes.com/blog/2008/11/27/how-to-dynamically-load-python-code/
    def __load_module(self, code_path):
        try:
            try:
                code_dir = os.path.dirname(code_path)
                code_file = os.path.basename(code_path)

                fin = open(code_path, 'rb')
                h = hashlib.md5()
                h.update(code_path)
                return  imp.load_source(h.hexdigest(), code_path, fin)
            finally:
                try: fin.close()
                except: pass
        except ImportError, x:
            traceback.print_exc(file = sys.stderr)
            raise
        except:
            traceback.print_exc(file = sys.stderr)
            raise
        
    def max_file(self):
        # find highest-numbered migration file.  By convention, migrations start with  
        # a numeric prefix then an underbar.
        numlist = [int(i.split('_')[0]) for i in os.listdir(mig_dir) if i.split('_')[0].isdigit()]
        if len(numlist) > 0:
            return max(numlist)
        else:
            return 0

    # find current migration number in database
    def current_mig_number(self):
        cur = self.dbconn.cursor()
        try:
            cur.execute("SELECT migration FROM madlib.migrationhistory ORDER BY id DESC LIMIT 1;")
        except:
            print sys.exc_info()[0]
            print "Unexpected error creating madlib.migrationhistory in database:"
            raise
        row = cur.fetchone()
        if row == None:
            return -1
        filename = row[0]
        return int(filename.split("_")[0])
        
    def fw_files(self):
        files = [i for i in os.listdir(mig_dir) if i.split('_')[0].isdigit() and int(i.split('_')[0]) > self.current_mig_number() and i.split(".")[-1] == "py"]
        files.sort()
        return files

    def bw_files(self):
        files = [i for i in os.listdir(mig_dir) if i.split('_')[0].isdigit() and int(i.split('_')[0]) <= self.current_mig_number() and i.split(".")[-1] == "py"]
        files.sort(reverse=True)
        return files
        
    def __gen_filename(self, name):
        next_num = self.current_mig_number()+1
        name = self.__prefix_to_n(str(next_num),'0',default_prefix_len)+"_"+name
        if name.split('.')[-1] != "py":
            name += ".py"
        return name

    def __wrap_sql(self,sqlfile,indent_width):
        fd = open(sqlfile)
        sqltext = "".join(fd.readlines())
        stmts = sqlparse.split(sqltext)
        retval = ""
        for s in stmts:
            if s.strip() != "":
                retval += "".join(["\t" for i in range(0,indent_width)]) + "cur.execute(\"\"\"" + s.strip() + "\"\"\")"
                retval += "\n"
        return retval

    def generate(self, dir, name, upfiles, downfiles):
        self.setup()
        filename = self.__gen_filename(name)
        # while os.path.exists(dir + filename)
        fd = open(dir + "/" + filename, 'w')
        fd.write(mig_prolog)
        fd.write(mig_forwards_prolog)
        for f in upfiles:
            fd.write(self.__wrap_sql(f,2))
        fd.write("\n\n\n")
        fd.write(mig_backwards_prolog)
        for f in downfiles:
            fd.write(self.__wrap_sql(f,2))
        return filename
        
    def setup(self):
        # create migrations directory and metadata in DB
        # schema is:
        #    id:          serial
        #    migration:   varchar(255)
        #    applied:     timestamp with time zone
        cur = self.dbconn.cursor()
        try:
            cur.execute("""SELECT table_schema, table_name 
                             FROM information_schema.tables
                            WHERE table_schema = 'madlib' AND table_name = 'migrationhistory';""")
        except:
            raise MadlibMigrationError("Unexpected error checking madlib schema in database")
            
        if cur.fetchone() == None:
            # check for schema
            cur.close()
            cur = self.dbconn.cursor()
            try:
                cur.execute("""SELECT * FROM information_schema.schemata
                                WHERE schema_name = 'madlib';""")
            except:
                print sys.exc_info()[0]
                print "Unexpected error checking madlib schema in database:"
                raise
            if cur.fetchone() == None:
                print sys.exc_info()[0]
                raise MadlibMigrationError("madlib schema not defined in database")
            cur.close()
            cur = self.dbconn.cursor()
            try:
                cur.execute("""CREATE TABLE madlib.migrationhistory 
                                            (id serial, migration varchar(255),
                                            applied timestamp with time zone)""")
            except:
                print sys.exc_info()[0]
                print "Unexpected error creating madlib.migrationhistory in database:"
                raise
            cur.close()
            self.dbconn.commit()

    def record_migration(self, filename):
        cur = self.dbconn.cursor()
        try:
            cur.execute("INSERT INTO madlib.migrationhistory (migration, applied) VALUES (%s, now());", (filename,))
        except:
            print sys.exc_info()[0]
            raise #MadlibMigrationError("Unexpected error recording into madlib.migrationhistory in database:")
        return True
        
    def delete_migration(self, filename):
        cur = self.dbconn.cursor()
        try:
            cur.execute("DELETE FROM madlib.migrationhistory WHERE migration = %s;",(filename,))
        except:
            print sys.exc_info()[0]
            print "Unexpected error deleting from madlib.migrationhistory in database:"
            raise
        return True
        
    # roll migrations fw/bw from current to mignumber
    def migrate(self, api, connect_args, mignumber=None):
       cur = self.current_mig_number()
       maxfile = self.max_file()
       if maxfile == None:
           return
       if mignumber == None:
           mignumber = maxfile
       if mignumber > maxfile:
           print sys.exc_info()[0]
           raise MadlibMigrationError("migration number " + str(mignumber) + " is larger than max file number " + str(maxfile))
       if cur == None or mignumber > cur:
           files = [f for f in self.fw_files() if int(f.split("_")[0]) <= mignumber]
           if len(files) > 0:
               print "- migrating forwards to " + files[-1]
           # rolling fw
           for f in files:
               num = int(f.split("_")[0])
               print "> " + f
               mod = self.__load_module(f)
               m = mod.Migration(api, connect_args)
               m.forwards()
               m.record_migration(f)
               m.dbconn.commit()
       elif mignumber < cur:
           # rolling bw
           files = [f for f in self.bw_files() if int(f.split("_")[0]) > mignumber]
           if len(files) > 0:
               print "- migrating backwards to before " + files[-1]
           for f in files:
               num = int(f.split("_")[0])
               print "> " + f
               mod = self.__load_module(f)
               m = mod.Migration(api, connect_args)
               m.backwards()
               m.delete_migration(f)
               m.dbconn.commit()