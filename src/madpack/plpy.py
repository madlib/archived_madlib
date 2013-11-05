# ==============================================================================
# plpy.py
#
# This module provides the abstraction level between the database
# and the user python code (e.g. kmeans.py).
#
# ==============================================================================


import sys
from types import *


try:
    from pygresql import pg
except Exception, e:
    try:
        import pg
    except Exception, e:
        errorMsg = "unable to import The PyGreSQL Python module (pg.py) - %s\n" % str(e)
        sys.stderr.write(str(errorMsg))
        sys.exit(2)

# This method establishes the connection to a database.
#
# Example:
# ----- my_run_kmeans.py -----
# import plpy
# from kmeans
# ...
# plpy.setcon( 'kmeans', 'localhost', 5432, 'myuser', 'mypass')
# ...
# print kmeans.kmeans_run( 50, 1);
# ----------


def connect(dbname, host, port, user, passwd):
    global db
    db = pg.DB(dbname=dbname,
               host=host,
               port=port,
               user=user,
               passwd=passwd)


def close():
    db.close()

# The following functions should be used inside the user modules
# in order to make their code uniform for both external python scripts
# or from in-database pl/python functions.
#
# Example:
# ----- kmeans.py -----
# import plpy
# ...
# def kmeans_run():
# ...
# plpy.execute( 'CREATE TEMP TABLE a (a INT)');
# plpy.info( 'Created table a.');
# ----------


def execute(sql):
    rv = db.query(sql.encode('utf-8'))
    if type(rv) is NoneType:
        return 0
    elif type(rv) is StringType:
        return rv
    else:
        return rv.dictresult()


def info(msg):
    print 'INFO: ' + msg


def error(msg):
    print 'ERROR: ' + msg
    exit( 1)
