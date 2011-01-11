---------------------------------------------------------
-- SVD sparse unit test: cleanup
---------------------------------------------------------
DROP FUNCTION IF EXISTS madlib_svdsparse_test.Generate_Sparse(INT, INT, INT);
DROP FUNCTION IF EXISTS madlib_svdsparse_test.Generate_Random(INT, INT, INT);
DROP TABLE IF EXISTS madlib.matrix_v;
DROP TABLE IF EXISTS madlib.matrix_u;

DROP SCHEMA madlib_svdsparse_test CASCADE;