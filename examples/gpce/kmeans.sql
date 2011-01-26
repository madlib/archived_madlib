\qecho =================================================================
\qecho === Running: k-means clustering =================================
\qecho =================================================================
\qecho 

set search_path="$user",madlib,public;
set client_min_messages=warning;

DROP SCHEMA madlib_kmeans_test CASCADE;

---------------------------------------------------------------------------
-- K-means unit test : setup
---------------------------------------------------------------------------

CREATE SCHEMA madlib_kmeans_test;

---------------------------------------------------------------------------
-- Random_Array: Generates random array
---------------------------------------------------------------------------
CREATE FUNCTION madlib_kmeans_test.random_array ( 
    size INTEGER, class INTEGER, total_classes INTEGER, sparsity FLOAT
) RETURNS FLOAT[] AS $$
declare
	g FLOAT[];
begin
	FOR i IN 1..size LOOP
		g[i] = 0;
	END LOOP;
	
	FOR i IN (1+(size*class/total_classes))..((size*class/total_classes)+(size/total_classes)) LOOP
		IF (sparsity > random()) THEN 
			g[i] = random();
		END IF;
	END LOOP;
	
	RETURN g;
end
$$ language plpgsql;

---------------------------------------------------------------------------
-- RNorm: Generates random number from normal distribution of (mean, sd)
---------------------------------------------------------------------------
CREATE FUNCTION madlib_kmeans_test.rnorm( mean FLOAT, sd FLOAT) RETURNS FLOAT AS $$
declare
begin
	RETURN (|/abs(-log(random()*(|/ (2*pi()*sd*sd)))*2*sd*sd))*(sign(random()-.5)) + mean;
end
$$ language plpgsql;

---------------------------------------------------------------------------
-- Gaussian_Sparse_Array: Generates an SVEC of ...
---------------------------------------------------------------------------
CREATE FUNCTION madlib_kmeans_test.gaussian_sparse_array( center FLOAT[], sparsity FLOAT) RETURNS madlib.svec AS $$
declare
	g FLOAT[];
	sd FLOAT := 1;
begin
	FOR i IN 1..array_upper(center,1) LOOP
		IF (center[i] > 0) THEN 
			g[i] = madlib_kmeans_test.RNorm(center[i], sd);
		ELSE
			g[i] = 0;
		END IF;
	END LOOP;
	RETURN madlib.svec_cast_float8arr(g);
end
$$ language plpgsql;

---------------------------------------------------------------------------
-- min: minimum of two FLOATs
---------------------------------------------------------------------------
-- DROP FUNCTION IF EXISTS madlib_kmeans_test.min( FLOAT, FLOAT);
CREATE FUNCTION madlib_kmeans_test.min( FLOAT, FLOAT) RETURNS FLOAT AS $$
declare
begin
    IF ($1 > $2) THEN
        RETURN $2;
    ELSIF ($2 > $1) THEN
        RETURN $1;
    ELSIF ($1 = $2) THEN
        RETURN $1;
    ELSE
        RETURN NULL;
    END IF;
end;
$$ language plpgsql;

---------------------------------------------------------------------------
-- CreateTestTable: 
--   Creates a table and populates it with random points for k-means testing.
--   Table structure: (pid BIGINT, position SVEC)
---------------------------------------------------------------------------
-- DROP FUNCTION IF EXISTS madlib_kmeans_test.create_test_table( 
--   p_table TEXT, p_num int, p_dim int, p_nr_of_clusters int, p_sparsity float
-- );
CREATE FUNCTION madlib_kmeans_test.create_test_table( 
    p_table TEXT, p_num int, p_dim int, p_nr_of_clusters int, p_sparsity float
) RETURNS TEXT AS $$
declare
	vector FLOAT[];
	sparsity FLOAT;
	check BIGINT; 
begin

    RAISE INFO 'Creating a source table with % points.', p_num;
    
    -- Create the Target table
    EXECUTE 'CREATE TABLE ' || p_table || '(
    	pid BIGINT,
    	position madlib.SVEC
    )';

    -- Create Temp table
    CREATE TEMP TABLE temp_kmeans_input(
    	pid BIGINT,
    	position madlib.SVEC
    );

	sparsity = p_sparsity * p_nr_of_clusters;

    -- Load some data
	FOR i in 1..p_num LOOP
		IF (i % 1000 = 1) THEN
			RAISE INFO '[ % ]', i;
		END IF; 
		IF (i % (p_num/p_nr_of_clusters) < 2) THEN
			vector = madlib_kmeans_test.Random_Array( p_dim, CAST(floor((i-1)/(p_num/p_nr_of_clusters)) AS INTEGER), p_nr_of_clusters, sparsity);
		END IF;
		INSERT INTO temp_kmeans_input VALUES(i,  madlib_kmeans_test.Gaussian_Sparse_Array(vector, sparsity));
	END LOOP;
	
	-- Move data from Temp to Target table
	EXECUTE 'INSERT INTO ' || p_table || ' SELECT * FROM temp_kmeans_input';
	
	RETURN 'Test table populated with ' || p_num || ' points (' || p_table || ').';
	
end
$$ language plpgsql;

\qecho === create test table ===========================================
drop table if exists madlib_kmeans_test.test_table;
select madlib_kmeans_test.create_test_table( 'madlib_kmeans_test.test_table', 1000, 100, 20, 0.1);

\qecho === run k-means algorythm =======================================
select madlib.kmeans_run( 'madlib_kmeans_test.test_table', 20, 1, 'mytest', 'madlib_kmeans_test');
