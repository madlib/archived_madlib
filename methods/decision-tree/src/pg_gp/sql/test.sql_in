DROP TABLE IF EXISTS MADLIB_SCHEMA.Points;
CREATE TABLE MADLIB_SCHEMA.Points(
	id SERIAL,
	feature MADLIB_SCHEMA.svec,
	class INTEGER
) DISTRIBUTED BY (id);

DROP TABLE IF EXISTS MADLIB_SCHEMA.Speedup;
CREATE TABLE MADLIB_SCHEMA.Speedup(
	id INTEGER
) DISTRIBUTED BY (id);

DROP TYPE IF EXISTS MADLIB_SCHEMA.elem CASCADE;
CREATE TYPE MADLIB_SCHEMA.elem AS(
	feature MADLIB_SCHEMA.svec,
	class INTEGER
);

CREATE OR REPLACE FUNCTION MADLIB_SCHEMA.RandomPoint(size INTEGER, density INTEGER, classes INTEGER, val INTEGER[]) RETURNS MADLIB_SCHEMA.elem AS $$
declare
	result MADLIB_SCHEMA.elem;
	feature FLOAT[];
	class INTEGER;
	i INTEGER;
begin
	class = CAST(FLOOR(random()*classes) AS INTEGER)+1;
	FOR i IN 1..size LOOP
		IF ((((i%classes)+1) == class) AND (random() < 1.0/CAST(density AS FLOAT))) THEN 
			feature[i] = class;
		ELSE
			IF (random() > .9) THEN
				feature[i] = CAST(random()*class AS INT);
			ELSE
				feature[i] = 0;
			END IF;
		END IF;
	END LOOP;
	
	result.feature = MADLIB_SCHEMA.svec_cast_float8arr(feature);
	result.class = class;
	RETURN result;
end
$$ language plpgsql;

INSERT INTO MADLIB_SCHEMA.Speedup SELECT G.a FROM generate_series(1, 5000) AS G(a);
INSERT INTO MADLIB_SCHEMA.Points (feature, class) SELECT (g.t).* FROM (SELECT id, MADLIB_SCHEMA.RandomPoint(1000, 1, 10, ARRAY[1,2,3,4,5,6,7,8,9,10]) AS t FROM MADLIB_SCHEMA.Speedup) As g;

SELECT MADLIB_SCHEMA.Train_Tree('MADLIB_SCHEMA.Points',10, 3000);
SELECT * FROM MADLIB_SCHEMA.tree ORDER BY id;
SELECT MADLIB_SCHEMA.Classify_Tree('MADLIB_SCHEMA.Points', 10);