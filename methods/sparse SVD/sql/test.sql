---------------------------------------------------------------
-- SVD sprase unit test: main
---------------------------------------------------------------
set client_min_messages=warning;

-- Pick a test to run: Random or Sequential Sparse; and creat a test table
--SELECT madlib_svdsparse_test.Generate_Random(10000, 10000, 100);
SELECT madlib_svdsparse_test.Generate_Sparse(10000, 10000, 100);

-- Run SVD decomposition on 3 main features
EXPLAIN ANALYZE SELECT MADLIB_SCHEMA.SVD_run('test'::text, 'col_num'::text, 'row_num'::text, 'val'::text, 3);

-- Display portion of the results
SELECT * FROM MADLIB_SCHEMA.matrix_u ORDER BY col_num ORDER BY col_num, row_num LIMIT 100;
SELECT * FROM MADLIB_SCHEMA.matrix_v ORDER BY row_num ORDER BY row_num, col_num LIMIT 100;