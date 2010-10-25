#!/usr/bin/python
import sys
import psycopg2

# helper routine to munge the agg arguments
def munge_agg(x):
    if x.split("(")[0] == x:
        x += '(%%)'
    x += " AS " + x.split("(")[0]
    if x.lower().find("distinct") >= 0:
        x += "_distinct"
    return x + "_%%"

# helper routine to query the database catalog and get column names for
# table "table" return value is a list of lists; the first list is the
# integer columns, the second list is the non-integer columns
def catalog_columns(dbnamestr, dbuserstr, table):
    # Connect to an existing database
    conn = psycopg2.connect(dbnamestr, dbuserstr)

    # Open a cursor to perform database operations
    cur = conn.cursor()

    # Fetch numeric columnnames from table
    cur.execute("""select column_name from information_schema.columns where
                   table_name = %s and numeric_scale = 0 order by
                   ordinal_position;""",
                (str(table),))
    out = cur.fetchall()
    numcols = [c[0] for c in out]
    cur.close()

    # Fetch non-numeric columnnames from table
    cur = conn.cursor()
    cur.execute("""select column_name from information_schema.columns where
                   table_name = %s and numeric_scale is null or
                   numeric_scale > 0 order by ordinal_position;""",
                (table,))
    out = cur.fetchall()
    non_numcols = [c[0] for c in out]
    # Make the changes to the database persistent
    # conn.commit()

    # Close communication with the database
    cur.close()
    conn.close()
    return([numcols, non_numcols])

# Generates an SQL query for data profiling.  Arguments:
#   tablename: name of the table/view being profiled
#   numaggs: a list of aggregation functions to run on numeric columns
#   non_numaggs, a list of aggregation functions to run on all columns
#   numcols: a list of numeric column expressions
#   non_numcols: a list of non_numeric columns expressions
#  The lists of aggs can include forms like "SUM", "SUM(%%)" or even
#  "SUM(DISTINCT %%)", where the %% string is replaced by the corresponding
#  column expression
def gen_profile_query(tablename, numaggs, non_numaggs, numcols, non_numcols):
    retval = "SELECT COUNT(*),\n       "
    retval += ",\n       ".join([", ".join([munge_agg(a).replace('%%', c)
                                    for a in numaggs+non_numaggs])
                                        for c in numcols])
    retval += ",\n       "
    retval += ",\n       ".join([", ".join([munge_agg(a).replace('%%', c)
                                    for a in non_numaggs])
                                        for c in non_numcols])
    retval += "\n  FROM "
    retval += tablename + ";"
    return retval
