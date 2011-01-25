\qecho =================================================================
\qecho === Running: SVD Matrix Factorisation ===========================
\qecho =================================================================
\qecho 

set client_min_messages=warning;

DROP TABLE IF EXISTS madlib.matrix_v;
DROP TABLE IF EXISTS madlib.matrix_u;
DROP SCHEMA if exists madlib_svdsparse_test CASCADE;

--------------------------------------------------------------------------------
-- SVD sparse unit test : setup
--------------------------------------------------------------------------------
set client_min_messages=warning;
CREATE SCHEMA madlib_svdsparse_test;

--------------------------------------------------------------------------------
-- Generate_sparse:
--	Creates a table represetint sparse matrix which is the result or products of two 
--	sequential valued rows.
--	Used for testing general convergence on data that has a discoverable structure
--
--      $1 - number of rows in the matrix
--	$2 - number of columns in the matrix
--	$3 - number of empty cells pre single cell containing a value
--------------------------------------------------------------------------------
CREATE OR REPLACE FUNCTION madlib_svdsparse_test.Generate_Sparse(INT, INT, INT) RETURNS void AS $$
declare
	i INT;
begin
DROP TABLE IF EXISTS madlib_svdsparse_test.test;
CREATE TABLE madlib_svdsparse_test.test(
	row_num INT, 
	col_num INT,
	val FLOAT
) DISTRIBUTED BY (row_num);

FOR i IN 1..$1 LOOP
	EXECUTE 'INSERT INTO madlib_svdsparse_test.test SELECT '||i||', gen.a, CAST((gen.a*'||i||') AS FLOAT) FROM (SELECT CAST((random()*'||$2||'+1) AS INT) AS a FROM generate_series(1,'||$2/$3||')) as gen';
END LOOP;
end
$$ LANGUAGE plpgsql;


--------------------------------------------------------------------------------
-- Generate_random:
--	Creates a table represetint sparse matrix with random values.
--	Used for testing general convergence poroperties on random data,
--	where convergence rate should be minimal
--
--      $1 - number of rows in the matrix
--	$2 - number of columns in the matrix
--	$3 - number of empty cells pre single cell containing a value
--------------------------------------------------------------------------------
CREATE OR REPLACE FUNCTION madlib_svdsparse_test.Generate_Random(INT, INT, INT) RETURNS void AS $$
declare
	i INT;
begin
DROP TABLE IF EXISTS madlib_svdsparse_test.test;
CREATE TABLE madlib_svdsparse_test.test(
	row_num INT, 
	col_num INT,
	val FLOAT
) DISTRIBUTED BY (row_num);

FOR i IN 1..$1 LOOP
	EXECUTE 'INSERT INTO madlib_svdsparse_test.test SELECT '||i||', gen.a, random() FROM (SELECT CAST((random()*'||$2||'+1) AS INT) AS a FROM generate_series(1,'||$2/$3||')) as gen';
END LOOP;
end
$$ LANGUAGE plpgsql;

---------------------------------------------------------------
-- SVD sprase unit test: main
---------------------------------------------------------------

-- Pick a test to run: Random or Sequential Sparse; and creat a test table
\qecho === generate test data ===========================
SELECT madlib_svdsparse_test.Generate_Sparse(100, 100, 100);

-- Run SVD decomposition on 3 main features
\qecho === run SVD decomposition ===========================
SELECT madlib.svdmf_run('madlib_svdsparse_test.test'::text, 'col_num'::text, 'row_num'::text, 'val'::text, 3);

-- Display portion of the results
\qecho === display some data ===========================
SELECT * FROM madlib.matrix_u ORDER BY col_num, row_num LIMIT 10;
