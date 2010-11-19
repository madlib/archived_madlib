--------------------------------------------------------------------------------
-- MADlib K-Means Clustering setup script
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- Set target schema name
-- XXX for now it is static, we need to make it flexible
--------------------------------------------------------------------------------
\set target_schema madlib

CREATE SCHEMA :target_schema;
CREATE LANGUAGE plpythonu;
CREATE LANGUAGE plpgsql;

BEGIN;

--------------------------------------------------------------------------------
-- @function: 
--        *._kmeans_closestID ( SVEC, SVEC[])
--
-- @doc:
--        This function takes single SVEC (a) and an array of SVEC (b)
--        and returns the index of (b) for which the smallest distance 
--        between vectors a and b[i].
--        
--------------------------------------------------------------------------------
DROP FUNCTION IF EXISTS :target_schema._kmeans_closestID( SVEC, SVEC[]);
CREATE OR REPLACE FUNCTION :target_schema._kmeans_closestID( p_point SVEC, p_centroids SVEC[]) RETURNS INTEGER
AS $$
DECLARE
    minCID      INTEGER := 1;
    min_val     FLOAT;
    temp_val    FLOAT;
BEGIN

    -- Check the arguments
    IF p_point is NULL or p_centroids is NULL THEN
        RETURN null;
    END IF;

    min_val = l2norm( p_point - p_centroids[1]);

    FOR i IN 2..array_upper( p_centroids, 1) 
    LOOP
        temp_val = l2norm( p_point - p_centroids[i]);
        IF ( temp_val < coalesce( min_val, temp_val + 1) ) THEN
            min_val = temp_val;
            minCID = i;
        END IF;
    END LOOP;
    
    RETURN minCID;
END
$$ LANGUAGE plpgsql;


--------------------------------------------------------------------------------
-- @aggregate: 
--        *._kmeans_meanPosition( SVEC)
--
-- @doc:
--        Finds the mean value for a set of SVECs
--        
--------------------------------------------------------------------------------
-- FINALFUNC
DROP FUNCTION IF EXISTS :target_schema._kmeans_mean_finalize( SVEC) CASCADE;
CREATE OR REPLACE FUNCTION :target_schema._kmeans_mean_finalize( SVEC) RETURNS SVEC AS $$
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

-- SFUNC
DROP FUNCTION IF EXISTS :target_schema._kmeans_mean_product(SVEC, SVEC) CASCADE;
CREATE OR REPLACE FUNCTION :target_schema._kmeans_mean_product(SVEC, SVEC) RETURNS SVEC AS $$
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

-- PREFUNC
DROP FUNCTION IF EXISTS :target_schema._kmeans_mean_aggr(SVEC, SVEC) CASCADE;
CREATE OR REPLACE FUNCTION :target_schema._kmeans_mean_aggr(SVEC, SVEC) RETURNS SVEC AS $$
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

-- Aggregate
DROP AGGREGATE IF EXISTS :target_schema._kmeans_meanPosition(SVEC);
CREATE AGGREGATE :target_schema._kmeans_meanPosition(SVEC) (
  STYPE = SVEC,
  SFUNC = :target_schema._kmeans_mean_product,
  PREFUNC = :target_schema._kmeans_mean_aggr,
  FINALFUNC = :target_schema._kmeans_mean_finalize
);

--------------------------------------------------------------------------------
-- @aggregate: 
--        *.kmeans_run( k int, goodness int);
--
-- @doc:
--        Runs the k-means algorythm (imported from kmeans.py)
--        
--------------------------------------------------------------------------------
DROP FUNCTION IF EXISTS :target_schema.kmeans_run( input_table text, k int, goodness int, run_id text, output_table text);
CREATE OR REPLACE FUNCTION :target_schema.kmeans_run( input_table text, k int, goodness int, run_id text, output_table text)
  RETURNS text
AS $$

  from kmeans import *

  plpy.execute( 'set client_min_messages=warning');
  return kmeans_run( input_table, k, goodness, run_id, output_table);
 
$$ LANGUAGE plpythonu;

-- Finalize
COMMIT;

-- EOF
