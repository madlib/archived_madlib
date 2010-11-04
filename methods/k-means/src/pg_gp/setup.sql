--------------------------------------------------------------------------------
-- MADlib K-Means Clustering setup script
--------------------------------------------------------------------------------
-- Author: 		Eugene Fratkin
-- Co-author: 	Aleks Gorajek 
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- Call method:
-- psql -f setup.sql -v schema=<your_target_shema>
--------------------------------------------------------------------------------

BEGIN;

--------------------------------------------------------------------------------
-- Set target schema name
-- XXX for now it is static, need to make it flexible
--------------------------------------------------------------------------------
\set target_schema madlib

-- DROP SCHEMA :target_schema CASCADE;
CREATE SCHEMA :target_schema;

--------------------------------------------------------------------------------
-- @aggregate: 
--        *._kmeans_distFromCenter
--
-- @doc:
--        Determines if a table is an AOT; returns true if OID refers to an AOT,
--        false if OID refers to a non-AOT relation; empty rowset if OID is invalid
--        
--------------------------------------------------------------------------------
-- SFunc
DROP FUNCTION IF EXISTS :target_schema.kmeans_UpdDistFromCenter( FLOAT, SVEC, SVEC) CASCADE;
CREATE OR REPLACE FUNCTION :target_schema.kmeans_UpdDistFromCenter( FLOAT, SVEC, SVEC) RETURNS FLOAT AS $$
DECLARE
	d FLOAT;
BEGIN
	IF ($1 IS NULL) THEN
		RETURN l2norm( $3 - $2);
	ELSE
		d = l2norm( $3 - $2);
		IF (d < $1) THEN 
			RETURN d;
		ELSE
			RETURN $1;
		END IF;
	END IF;
END
$$ LANGUAGE plpgsql;

-- PreFunc
DROP FUNCTION IF EXISTS :target_schema.kmeans_AggrDistFromCenter( FLOAT, FLOAT) CASCADE;
CREATE OR REPLACE FUNCTION :target_schema.kmeans_AggrDistFromCenter( FLOAT, FLOAT) RETURNS FLOAT AS $$
DECLARE
BEGIN
	IF ($1 < $2) THEN
		RETURN $1;
	ELSE
		RETURN $2;
	END IF;
END
$$ LANGUAGE plpgsql;

-- FinalFunc
DROP FUNCTION IF EXISTS :target_schema.kmeans_FinalizeDistFromCenter( FLOAT) CASCADE;
CREATE OR REPLACE FUNCTION :target_schema.kmeans_FinalizeDistFromCenter( FLOAT) RETURNS FLOAT AS $$
DECLARE
BEGIN
	RETURN $1*(random()^(1.0/5.0));
END
$$ LANGUAGE plpgsql;

-- UDA
DROP AGGREGATE IF EXISTS :target_schema.kmeans_DistFromCenter( SVEC, SVEC);
CREATE AGGREGATE :target_schema.kmeans_DistFromCenter( SVEC, SVEC) (
  SFUNC = :target_schema.kmeans_UpdDistFromCenter,
  PREFUNC = :target_schema.kmeans_AggrDistFromCenter,
  FINALFUNC = :target_schema.kmeans_FinalizeDistFromCenter,
  STYPE = FLOAT
);


--------------------------------------------------------------------------------
-- UDA : updateCentroids : START
--------------------------------------------------------------------------------
DROP FUNCTION IF EXISTS :target_schema.kmeans_mean_finalize(SVEC) CASCADE;
CREATE OR REPLACE FUNCTION :target_schema.kmeans_mean_finalize(SVEC) RETURNS SVEC AS $$
DECLARE
	new_location FLOAT[];
	new_location2 FLOAT[];
	sum FLOAT;
BEGIN
	new_location = SVEC_return_array($1);
	sum = new_location[array_upper(new_location, 1)];
	FOR i in 1..(array_upper(new_location, 1)-1) LOOP
		new_location2[i] = new_location[i]/sum;
	END LOOP;
	RETURN SVEC_cast_float8arr(new_location2);
END
$$ LANGUAGE plpgsql;

DROP FUNCTION IF EXISTS :target_schema.kmeans_mean_product(SVEC, SVEC) CASCADE;
CREATE OR REPLACE FUNCTION :target_schema.kmeans_mean_product(SVEC, SVEC) RETURNS SVEC AS $$
DECLARE
	new_location SVEC;
BEGIN

new_location = SVEC_concat($2,SVEC_cast_float8(1.0));
IF ($1 IS NOT NULL) THEN
	new_location = $1 + new_location;
END IF;

RETURN new_location;
END
$$ LANGUAGE plpgsql;

DROP FUNCTION IF EXISTS :target_schema.kmeans_mean_aggr(SVEC, SVEC) CASCADE;
CREATE OR REPLACE FUNCTION :target_schema.kmeans_mean_aggr(SVEC, SVEC) RETURNS SVEC AS $$
DECLARE
BEGIN

IF (($1 IS NOT NULL) AND ($2 IS NOT NULL)) THEN
	RETURN $1 + $2;
END IF;
IF ($1 IS NOT NULL) THEN
	RETURN $1;
END IF;
IF ($2 IS NOT NULL) THEN
	RETURN $2;
END IF;

END
$$ LANGUAGE plpgsql;

DROP AGGREGATE IF EXISTS :target_schema.kmeans_updateCentroids(SVEC);
CREATE AGGREGATE :target_schema.kmeans_updateCentroids(SVEC) (
  STYPE = SVEC,
  SFUNC = :target_schema.kmeans_mean_product,
  PREFUNC = :target_schema.kmeans_mean_aggr,
  FINALFUNC = :target_schema.kmeans_mean_finalize
);
--------------------------------------------------------------------------------
-- UDA : updateCentroids : END
--------------------------------------------------------------------------------


--------------------------------------------------------------------------------
-- UDA : goodnessFitTest : START
--------------------------------------------------------------------------------
DROP FUNCTION IF EXISTS :target_schema.kmeans_fit_finalize(FLOAT[]) CASCADE;
CREATE OR REPLACE FUNCTION :target_schema.kmeans_fit_finalize(FLOAT[]) RETURNS FLOAT AS $$
DECLARE
BEGIN
	RETURN $1[1]/$1[2];
END
$$ LANGUAGE plpgsql;

DROP FUNCTION IF EXISTS :target_schema.kmeans_fit_product(FLOAT[], SVEC, SVEC) CASCADE;
CREATE OR REPLACE FUNCTION :target_schema.kmeans_fit_product(FLOAT[], SVEC, SVEC) RETURNS FLOAT[] AS $$
DECLARE
	temp FLOAT[];
BEGIN

IF ($1[1] IS NULL) THEN
	temp[1] = l2norm($2 - $3);
	temp[2] = 1;
ELSE
	temp[1] = $1[1] + l2norm($2 - $3);
	temp[2] = $1[2] + 1;
END IF;

RETURN temp;
END
$$ LANGUAGE plpgsql;

DROP FUNCTION IF EXISTS :target_schema.kmeans_fit_aggr(FLOAT[], FLOAT[]) CASCADE;
CREATE OR REPLACE FUNCTION :target_schema.kmeans_fit_aggr(FLOAT[], FLOAT[]) RETURNS FLOAT[] AS $$
DECLARE
	temp FLOAT[];
BEGIN

IF (($1[1] IS NOT NULL) AND ($2[1] IS NOT NULL)) THEN
	temp[1] = $1[1] + $2[1];
	temp[2] = $1[2] + $2[2];
END IF;
IF ($1[1] IS NOT NULL) THEN
	RETURN $1;
END IF;
IF ($2[1] IS NOT NULL) THEN
	RETURN $2;
END IF;

RETURN temp;
END
$$ LANGUAGE plpgsql;

DROP AGGREGATE IF EXISTS :target_schema.kmeans_goodnessFitTest(SVEC, SVEC);
CREATE AGGREGATE :target_schema.kmeans_goodnessFitTest(SVEC, SVEC) (
  STYPE = FLOAT[],
  SFUNC = :target_schema.kmeans_fit_product,
  PREFUNC = :target_schema.kmeans_fit_aggr,
  FINALFUNC = :target_schema.kmeans_fit_finalize
);
--------------------------------------------------------------------------------
-- UDA : goodnessFitTest : END
--------------------------------------------------------------------------------


--------------------------------------------------------------------------------
-- UDF : updateSubAssignment : START
--------------------------------------------------------------------------------
DROP FUNCTION IF EXISTS :target_schema.kmeans_updateSubAssignment(SVEC, SVEC[]) CASCADE;
CREATE OR REPLACE FUNCTION :target_schema.kmeans_updateSubAssignment(SVEC, SVEC[]) RETURNS INTEGER AS $$
DECLARE
	min_ind INTEGER := 1;
	min_val FLOAT;
	temp_val FLOAT := 0;
BEGIN
	min_val = l2norm($1 - $2[1]);
	FOR i IN 2..array_upper($2,1) LOOP
		temp_val = l2norm($1 - $2[i]);
		IF (temp_val < min_val) THEN
			min_val = temp_val;
			min_ind = i;
		END IF;
	END LOOP;
	
	RETURN min_ind;
END
$$ LANGUAGE plpgsql;
--------------------------------------------------------------------------------
-- UDF : updateSubAssignment : END
--------------------------------------------------------------------------------


--------------------------------------------------------------------------------
-- UDF : updateSubAssignmentFinal : START
--------------------------------------------------------------------------------
DROP FUNCTION IF EXISTS :target_schema.kmeans_updateSubAssignmentFinal(SVEC, SVEC[]) CASCADE;
CREATE OR REPLACE FUNCTION :target_schema.kmeans_updateSubAssignmentFinal(SVEC, SVEC[]) RETURNS FLOAT[] AS $$
DECLARE
	min_ind INTEGER := 1;
	min_val FLOAT;
	temp_val FLOAT := 0;
BEGIN
	min_val = l2norm($1 - $2[1]);
	FOR i IN 2..array_upper($2,1) LOOP
		temp_val = l2norm($1 - $2[i]);
		IF (temp_val < min_val) THEN
			min_val = temp_val;
			min_ind = i;
		END IF;
	END LOOP;
	
	RETURN ARRAY[CAST(min_ind AS FLOAT), min_val];
END
$$ LANGUAGE plpgsql;
--------------------------------------------------------------------------------
-- UDF : updateSubAssignmentFinal : END
--------------------------------------------------------------------------------


--------------------------------------------------------------------------------
-- UDF : min : START
-- This function could go be moved to utils: madlib.min( FLOAT, FLOAT)
--------------------------------------------------------------------------------
DROP FUNCTION IF EXISTS min(a FLOAT, b FLOAT);
CREATE FUNCTION min(a FLOAT, b FLOAT) RETURNS FLOAT AS $$
DECLARE
BEGIN
	IF (a > b) THEN
		RETURN b;
	ELSE
		RETURN a;
	END IF;
END
$$ language plpgsql;
--------------------------------------------------------------------------------
-- UDF : min : END
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- initKMeans - Performs initial centroid initialization as prescribed by K-Means++ algorithm
-- Inputs:
-- k - number of centroids
-- Outputs: 
-- Tables: TempTable1, TempTable2 are created
-- table gpkms.Centroid is created and filled with values
--------------------------------------------------------------------------------
DROP FUNCTION IF EXISTS :target_schema.kmeans_init( INTEGER);
CREATE FUNCTION :target_schema.kmeans_init( k INTEGER) RETURNS TEXT AS $$
DECLARE
	pick INTEGER := 1;
	estSize INTEGER := 1;
	numCentroids INTEGER;
	-- Basic variables
	v_start_ts	TIMESTAMP;
BEGIN
	-- ----------------------------------------
	-- Record the start time
	-- ----------------------------------------
	SELECT INTO v_start_ts date_trunc( 'sec', clock_timestamp()::timestamp);
	
	/* Find how many points we need to look though to be 99.9% sure that
	we have seen a point from each cluster */ 
	numCentroids = floor(-ln(1-(.999)^(1/CAST(k AS FLOAT)))*k);
	
	/* Create all auxiliary tables */
	DROP TABLE IF EXISTS TempTable1 CASCADE;
	CREATE TEMP TABLE TempTable1(
		pid BIGINT, 
		position SVEC, 
		cid INTEGER
	)DISTRIBUTED BY (pid);
	
	DROP TABLE IF EXISTS TempTable2 CASCADE;
	CREATE TEMP TABLE TempTable2(
		pid BIGINT, 
		position SVEC, 
		cid INTEGER
	) DISTRIBUTED BY (pid);
	
	-- Create full output table
	EXECUTE 'DROP TABLE IF EXISTS madlib.kmeans_output_points';
	EXECUTE 'CREATE TABLE madlib.kmeans_output_points (
				pid BIGINT, 
				position SVEC, 
				cid FLOAT
				--distance FLOAT
			  ) DISTRIBUTED BY (pid)';
	
	-- Create centroids output table
	EXECUTE 'DROP TABLE IF EXISTS madlib.kmeans_output_centroids';
	EXECUTE 'CREATE TABLE madlib.kmeans_output_centroids(
				cid INT,
				position SVEC
			) DISTRIBUTED BY (cid)';
	
	/* Find initial centroids positions so that they are likely to represent new clusters */
	EXECUTE '
		INSERT INTO madlib.kmeans_output_centroids (cid, position) 
		SELECT ' || estSize || ', p.position 
		FROM
			(SELECT position, random()^(1/5) AS distance FROM madlib.kmeans_input LIMIT ' || numCentroids || ') AS p 
		ORDER BY distance DESC 
		LIMIT 1
		';
	RAISE INFO 'CENTROID % SEEDED', estSize;
	
	WHILE (estSize < k) LOOP
		estSize := estSize + 1;
		EXECUTE '
		INSERT INTO madlib.kmeans_output_centroids (cid, position) 
		SELECT ' || estSize || ', pt.position 
		FROM 
			madlib.kmeans_input pt,
			(
				-- Find a random point (that is furthest away from current Centroids)
				SELECT p.pid, madlib.kmeans_DistFromCenter (p.position, c.position) AS distance 
				FROM
					(SELECT position, pid FROM madlib.kmeans_input ORDER BY random() LIMIT ' || numCentroids || ') AS p -- K random points
					CROSS JOIN 
					(SELECT position FROM madlib.kmeans_output_centroids) AS c -- current centroids
				GROUP BY p.pid ORDER BY distance DESC LIMIT 1
			) AS pc 
		WHERE pc.pid = pt.pid
		';
		RAISE INFO 'CENTROID % SEEDED', estSize;
	END LOOP;

	RETURN 'Finished seeding of ' || estSize || ' centroids.' ||
   	   E'\nTime elapsed: ' || date_trunc( 'sec', clock_timestamp()::timestamp) - v_start_ts;
   	   	
END
$$ language plpgsql;


--------------------------------------------------------------------------------
-- performKMeans - Performs k-means algorithm.
-- Inputs:
-- ksize - number of centroids
-- goodness - if 1, will compute and return mean distance to the centroid per point; this can be used for 'elbow' method of number of clusters estimation.   
-- Outputs: 
-- Table TempTable2 is created and contains the values of the points and their cluster assignment
-- Table gpkms.Centroid is created and contains centroid IDs and locations
--------------------------------------------------------------------------------
DROP FUNCTION IF EXISTS :target_schema.kmeans_run( INTEGER, INTEGER);
CREATE FUNCTION :target_schema.kmeans_run( ksize INTEGER, goodness INTEGER) RETURNS FLOAT AS $$
DECLARE
	/* 
	* These variables are used to determine when process should be terminated. 
	* Variables that can be set for performance fine-tuning have a comment next to them.
	*/
	error1 FLOAT[];
	error_limit FLOAT := .001; -- smaller number will mean more itterations for closer convergence
	
	derivative FLOAT[] := '{4,3,2,1}';
	to_exit INTEGER := 0;
	
	max_number_iterations INTEGER := 20; -- larger number may mean more itterations for closer convergence
	done_iterations INTEGER := 0;
	
	need_to_expand BOOLEAN := false;
	
	/* These variables used to store sizes of the full set and sampled core set */
	
	no_sampling INTEGER := 0; -- if 1 core sampling will be turened off and algorithm will be executed on the full set of points 
	existing_size INTEGER; 
	sampling_size INTEGER := 100; -- core set will be of sufficent size to have at least 'samplin_size' elements from each cluster with p = .999
	max_sample_size INTEGER := 10000000; -- at not circumstances would size of core set be larger than this. 
	
	/* Some other intermediate and timing variables */
	
	time  timestamp;
	total_time timestamp;
	
	temp_centroids SVEC[];
	temp_c SVEC;
	gof FLOAT := 0;
	
BEGIN

	total_time = clock_timestamp();
	
	EXECUTE 'SELECT count(*) FROM madlib.kmeans_input' INTO existing_size;
	error1 = ARRAY[existing_size,0];
	
	/* Find senctroids as described in k-means++ algorithm */
	PERFORM madlib.kmeans_init( ksize); 
	
	/* Find size of the core set such that assuming that all the clusters are of equal size
	there are at least 200 points from each cluster
	*/
	sampling_size = min(sampling_size*floor(-ln(1-(.999)^(1/CAST(ksize AS FLOAT)))*ksize), max_sample_size);
	RAISE INFO 'CORE SET SIZE: %', min(sampling_size, existing_size);
	
	SET gp_autostats_mode TO 'none';
		
	/* Create table either with full set of point or the core subset */	
	TRUNCATE TABLE TempTable2;
	--EXECUTE 'CREATE TEMP TABLE TMP' || done_iterations || ' ( pid BIGINT, position SVEC, cid INTEGER) DISTRIBUTED BY (pid)';
	IF((existing_size > sampling_size) AND (no_sampling <> 1)) THEN 
		--INSERT INTO TempTable2 SELECT pid, position, cid FROM gpkms.Points ORDER BY random() LIMIT sampling_size;
		EXECUTE '
			INSERT INTO TempTable2 
			SELECT pid, position, 0 FROM madlib.kmeans_input 
			ORDER BY random()'
			LIMIT sampling_size;
		need_to_expand := true;
	ELSE
		--INSERT INTO TempTable2 SELECT pid, position, cid FROM gpkms.Points ORDER BY random();
		EXECUTE '
			INSERT INTO TempTable2 
			SELECT pid, position, 0 FROM madlib.kmeans_input 
			ORDER BY random()';
		need_to_expand := false;
	END IF;

	/* Create in memory array containing all the cluster centroids */
	FOR i IN 1..ksize LOOP
		EXECUTE 'SELECT position FROM madlib.kmeans_output_centroids WHERE cid = ' || i INTO temp_c;
		temp_centroids[i] = temp_c;
	END LOOP;
	
	/* Start the core iterative procedure */
	
	/* there are three possible tremination ctria:
	1) Difference in number of points changing cluster membership falls below error_limit threshhold
	2) Maximum number of iterations is exceeded
	3) Progress in converging to the final enaswer is too slow - usually and indication of non-meaningful clusters being formed 
	*/
	LOOP
		done_iterations = done_iterations + 1;
		time = clock_timestamp();
		
		/* Update assignment of points to clusters */

		TRUNCATE TABLE TempTable1;
		INSERT INTO TempTable1 SELECT 
			p.pid, 
			p.position, 
			madlib.kmeans_updateSubAssignment(p.position, temp_centroids) AS cid
		FROM TempTable2 p;
		
		RAISE INFO 'Assign Points To Clusters := %',  clock_timestamp() - time;
		time = clock_timestamp();
		
		/* Update centroid locations */
		
		EXECUTE 'TRUNCATE TABLE madlib.kmeans_output_centroids';
		EXECUTE 'INSERT INTO madlib.kmeans_output_centroids SELECT cid, tc.position FROM (SELECT 
			madlib.kmeans_updateCentroids(tp.position) AS position, 
			tp.cid AS cid 
		FROM TempTable1 tp GROUP BY tp.cid) AS tc';
		
		/* Create in memory array containing all the cluster centroids */

		FOR i IN 1..ksize LOOP
			EXECUTE 'SELECT position FROM madlib.kmeans_output_centroids WHERE cid = ' || i INTO temp_c;
			temp_centroids[i] = temp_c;
		END LOOP;

		RAISE INFO 'Recompute Centroid Locations := %',  clock_timestamp() - time;
		time = clock_timestamp();
		
		/* Update assignment of points to clusters */

		TRUNCATE TABLE TempTable2;
		INSERT INTO TempTable2 SELECT 
			p.pid, 
			p.position, 
			madlib.kmeans_updateSubAssignment(p.position, temp_centroids) AS cid 
		FROM TempTable1 p;
		
		RAISE INFO 'Assign Points To Clusters := %',  clock_timestamp() - time;
		time = clock_timestamp();
		
		/* Update centroid locations */
		
		EXECUTE 'TRUNCATE TABLE madlib.kmeans_output_centroids';
		EXECUTE 'INSERT INTO madlib.kmeans_output_centroids SELECT cid, tc.position FROM (SELECT 
			madlib.kmeans_updateCentroids(tp.position) AS position, 
			tp.cid AS cid 
		FROM TempTable2 tp GROUP BY tp.cid) AS tc';
		
		/* Create in memory array containing all the cluster centroids */

		FOR i IN 1..ksize LOOP
			EXECUTE 'SELECT position FROM madlib.kmeans_output_centroids WHERE cid = ' || i INTO temp_c;
			temp_centroids[i] = temp_c;
		END LOOP;

		RAISE INFO 'Recompute Centroid Locations := %',  clock_timestamp() - time;
		time = clock_timestamp();
		
		/* estimate fraction of points that changed clusters */
		
		SELECT INTO error1 ARRAY(SELECT CAST((t1.e - t2.e) AS FLOAT)  FROM
			(SELECT count(t1.pid) AS e, t1.cid FROM TempTable1 t1 GROUP BY t1.cid) AS t1 JOIN 
			(SELECT count(t2.pid) AS e, t2.cid FROM TempTable2 t2 GROUP BY t2.cid) AS t2 ON t1.cid = t2.cid);
			
		/* look at trEND of convergence */
			
		derivative[1] = derivative[2];
		derivative[2] = derivative[3];
		derivative[3] = derivative[4];
 		derivative[4] = l1norm(error1)/existing_size;
 		IF((derivative[1] - derivative[4]) < 0) THEN 
 			to_exit = 1;
 		END IF;
		RAISE INFO 'Fraction of nodes changing cluster [ % ]', derivative[4];
		
		/* extrapolate results to all the points in the input, if termination condition is reached */

		IF ((l1norm(error1)/existing_size <= error_limit) OR (done_iterations >= max_number_iterations) OR (to_exit <> 0))  THEN
			IF (l1norm(error1)/existing_size <= error_limit) THEN
				RAISE INFO 'TERMINATION CONDITION REACHED: SMALL NUMBER OF POINTS CHANGE MEMBERSHIP (%)', error_limit;
			ELSEIF (done_iterations >= max_number_iterations) THEN
				RAISE INFO 'TERMINATION CONDITION REACHED: MAXIMUM NUMBER OF ITTERATIONS REACHED (%)', max_number_iterations;
			ELSE
				RAISE INFO 'TERMINATION CONDITION REACHED: NUMBER OF POINTS CHANGING MEMBERSHIP DID NOT DECREASE IN 4 ITTERATIONS(%)';
			END IF;
			
			
			IF (need_to_expand is True) THEN 
				TRUNCATE TABLE TempTable2;
				TRUNCATE TABLE TempTable1;
				--TRUNCATE TABLE FinalTable;
				INSERT INTO TempTable2 SELECT A.pid, A.position, cluster[1] --, cluster[2] 
				FROM (
				SELECT 
					p.pid, 
					p.position, 
					madlib.kmeans_updateSubAssignmentFinal( p.position, temp_centroids) AS cluster 
				FROM madlib.kmeans_input p) AS A;
			END IF;
			
			EXECUTE 'INSERT INTO madlib.kmeans_output_points SELECT pid, position, cid FROM TempTable2';
			
			EXIT;
		END IF;
	END LOOP;
	
	IF (goodness == 1) THEN
		SELECT INTO gof madlib.kmeans_goodnessFitTest(position, temp_centroids[cid]) FROM TempTable2;
	END IF;
	
	RAISE INFO 'TOTAL EXECUTION TIME := % SEC',  clock_timestamp() - total_time;
	
	RETURN gof;
END
$$ language plpgsql;

-- Finalize
COMMIT;

-- EOF
