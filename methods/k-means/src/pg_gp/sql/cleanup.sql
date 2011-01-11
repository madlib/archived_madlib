---------------------------------------------------------------------------
-- K-means unit test : cleanup
---------------------------------------------------------------------------
DROP FUNCTION IF EXISTS madlib_kmeans_test.random_array( INTEGER, INTEGER, INTEGER, FLOAT);
DROP FUNCTION IF EXISTS madlib_kmeans_test.rnorm( FLOAT, FLOAT);
DROP FUNCTION IF EXISTS madlib_kmeans_test.gaussian_sparse_array( FLOAT[], FLOAT);
DROP FUNCTION IF EXISTS madlib_kmeans_test.min( FLOAT, FLOAT);
DROP FUNCTION IF EXISTS madlib_kmeans_test.create_test_table( TEXT, INT, INT, INT, FLOAT);

DROP TABLE IF EXISTS madlib_kmeans_test.table123;

DROP SCHEMA madlib_kmeans_test CASCADE;