set client_min_messages=warning;
-----------------------------------------------
-- Create few functions to populate INPUT table
-- with random SVEC data
-----------------------------------------------
-- madlib.kmeans_RandomArray
DROP FUNCTION IF EXISTS madlib.kmeans_RandomArray(INTEGER, INTEGER, INTEGER, FLOAT);
CREATE FUNCTION madlib.kmeans_RandomArray(size INTEGER, class INTEGER, total_classes INTEGER, sparsity FLOAT) RETURNS FLOAT[] AS $$
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

-- madlib.kmeans_RNorm
DROP FUNCTION IF EXISTS madlib.kmeans_RNorm(mean FLOAT, sd FLOAT);
CREATE FUNCTION madlib.kmeans_RNorm(mean FLOAT, sd FLOAT) RETURNS FLOAT AS $$
declare
begin
	RETURN (|/abs(-log(random()*(|/ (2*pi()*sd*sd)))*2*sd*sd))*(sign(random()-.5)) + mean;
end
$$ language plpgsql;

-- madlib.kmeans_GaussianSparseArray
DROP FUNCTION IF EXISTS madlib.kmeans_GaussianSparseArray(FLOAT[], FLOAT);
CREATE FUNCTION madlib.kmeans_GaussianSparseArray(center FLOAT[], sparsity FLOAT) RETURNS svec AS $$
declare
	g FLOAT[];
	sd FLOAT := 1;
begin
	FOR i IN 1..array_upper(center,1) LOOP
		IF (center[i] > 0) THEN 
			g[i] = madlib.min(CAST(1.0 AS FLOAT),madlib.kmeans_RNorm(center[i], sd))+1;
		ELSE
			g[i] = 0;
		END IF;
	END LOOP;
	RETURN svec_cast_float8arr(g);
end
$$ language plpgsql;

-- madlib.min
DROP FUNCTION IF EXISTS madlib.min( FLOAT, FLOAT);
CREATE FUNCTION madlib.min( FLOAT, FLOAT) RETURNS FLOAT AS $$
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

--
-- madlib.kmeans_CreateInputTable
--
DROP FUNCTION IF EXISTS madlib.kmeans_CreateInputTable( p_num int, p_dim int);
CREATE FUNCTION madlib.kmeans_CreateInputTable( p_num int, p_dim int) RETURNS void AS $$
declare
	vector FLOAT[];
	cluster_number INTEGER := 20;
	error_fruct FLOAT := 0;
	sparsity FLOAT := .01; 
begin

    DROP TABLE IF EXISTS madlib.kmeans_input;
    CREATE TABLE madlib.kmeans_input(
    	pid SERIAL,
    	position svec
    );

	sparsity = sparsity * cluster_number;

	FOR i in 1..p_num LOOP
		IF (i % 1000 = 1) THEN
			RAISE INFO '[ % ]', i;
		END IF; 
		IF (i % (p_num/cluster_number) < 2) THEN
			vector = madlib.kmeans_RandomArray( p_dim, CAST(floor((i-1)/(p_num/cluster_number)) AS INTEGER), cluster_number, sparsity);
		END IF;
		INSERT INTO madlib.kmeans_input VALUES(i,  madlib.kmeans_GaussianSparseArray(vector, sparsity));
	END LOOP;
end
$$ language plpgsql;

-----------------------------------------------
-- Create an INPUT table
-----------------------------------------------
SELECT madlib.kmeans_CreateInputTable( 10000, 1000);