import sys
import psycopg2
import os
import profiling
from optparse import OptionParser

usage = "usage: %prog [options] tablename"
parser = OptionParser(usage=usage)
parser.add_option("-d", "--database", dest="database", nargs=1,
                  help="database name", default=os.environ['PGDATABASE'])
parser.add_option("-u", "--user", dest="user", nargs=1,
                  help="user name", default=os.environ['PGUSER'])
parser.add_option("-n", "--numeric", dest="numericaggs", nargs = 1,
                  help="array of aggs for numeric columns",
                  default='["MIN", "MAX", "AVG"]')
parser.add_option("-s", "--nonnumeric", dest="non_numericaggs", nargs = 1,
                help="array of aggs for non-numeric columns",
                default='["COUNT(DISTINCT %%)"]')
(options, args) = parser.parse_args()

try:
  table = sys.argv[1]
except:
  parser.error("missing tablename")
  sys.exit(2)
dbnamestr = "dbname=" + options.database
dbuserstr = "user=" + options.user
(numcols, non_numcols) = profiling.catalog_columns(dbnamestr, dbuserstr, table)
query = profiling.gen_profile_query(table,
                                    eval(options.numericaggs),
                                    eval(options.non_numericaggs),
                                    numcols, non_numcols)
print query
conn = psycopg2.connect(dbnamestr, dbuserstr)

# Open a cursor to perform database operations
cur = conn.cursor()

# Fetch numeric columnnames from table
cur.execute(query)
out = cur.fetchone()
for i in range(len(out)):
    print cur.description[i][0]+": "+str(out[i])
cur.close()
conn.close()
