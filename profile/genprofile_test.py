import profiling
import unittest
import psycopg2

class ProfileTest(unittest.TestCase):
    dbnamestr = "dbname=template1"
    dbuserstr = "user=joeh"
    table = "pg_type"
    testdb = "profile_regression"
    
    allcols = profiling.catalog_columns(dbnamestr, dbuserstr, table)
    query = profiling.gen_profile_query(table, ["MIN", "MAX", "cmsketch_centile(cmsketch(%%), 50)"], 
                            ["fmcount"], allcols[0], allcols[1])
                            
    def testQuery(self):
        # Connect to an existing database
        conn = psycopg2.connect(self.dbnamestr, self.dbuserstr)
        # autocommit mode required to run CREATE DATABASE
        conn.set_isolation_level(psycopg2.extensions.ISOLATION_LEVEL_AUTOCOMMIT)
        # Open a cursor to perform database operations
        cur = conn.cursor()
        
        # create test database and connect to it.
        cur.execute("CREATE DATABASE " + self.testdb + ";")
        cur.close()
        conn.close()
        conn = psycopg2.connect("dbname="+ self.testdb, self.dbuserstr)
        cur = conn.cursor()
        
        # load the sketches DDL
        f = open('../utils/sketch/postgresql/sketches.sql', 'r')
        script = f.read()
        cur.execute(script)

        # Run query
        cur.execute(self.query)
        out = cur.fetchall()
        cur.close()
        conn.close()
        self.assertEqual(out, [(283L, -2, 64, 4L, 12L, -1, 2, 0L, 2L, 0, 0, 0L, 1L, 283L, 3L, 1L, 2L, 4L, 13L, 2L, 1L, 2L, 150L, 60L, 60L, 72L, 71L, 61L, 60L, 11L, 11L, 2L, 4L, 3L, 1L, 4L, 1L, 1L)])
        
        # Drop test database
        conn = psycopg2.connect(self.dbnamestr, self.dbuserstr)
        # autocommit mode required to run CREATE DATABASE
        conn.set_isolation_level(psycopg2.extensions.ISOLATION_LEVEL_AUTOCOMMIT)
        cur = conn.cursor()
        cur.execute("DROP DATABASE " + self.testdb + ";")
        cur.close()
        conn.close()
if __name__ == "__main__":
    unittest.main()