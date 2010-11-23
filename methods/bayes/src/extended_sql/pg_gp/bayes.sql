-- Create a simple product aggregate. Trust on the optimizer for good performance.

CREATE FUNCTION madlib.product(DOUBLE PRECISION, DOUBLE PRECISION)
RETURNS DOUBLE PRECISION AS
$$
	SELECT $1 * $2
$$
LANGUAGE sql IMMUTABLE;

CREATE AGGREGATE madlib.product(DOUBLE PRECISION) (
	SFUNC=madlib.product,
	STYPE=DOUBLE PRECISION,
	PREFUNC=madlib.product,
	INITCOND='1.0'
);

-- In the following, we implement an argmax function where the "index set" is
-- INTEGER and we range over DOUBLE PRECISION values.
-- argmax should only be used on unsorted data because it will not exploit
-- indices, and its running time is Theta(n).
-- The implementation is in SQL, giving a flavor of functional programming.
-- Again, the hope is that the optimizer does a good job here...

CREATE TYPE madlib.ARGS_AND_VALUE_DOUBLE AS (
	args INTEGER[],
	value DOUBLE PRECISION
);

CREATE FUNCTION madlib.argmax_transition(
	oldmax madlib.ARGS_AND_VALUE_DOUBLE,
	newkey INTEGER,
	newvalue DOUBLE PRECISION)
RETURNS madlib.ARGS_AND_VALUE_DOUBLE AS
$$
	SELECT CASE WHEN $2 IS NULL OR $3 IS NULL OR $3 < $1.value THEN $1
				WHEN $3 = $1.value THEN ($1.args || $2, $3)::madlib.ARGS_AND_VALUE_DOUBLE
				ELSE (array[$2], $3)::madlib.ARGS_AND_VALUE_DOUBLE
		   END
$$
LANGUAGE sql IMMUTABLE;

CREATE FUNCTION madlib.argmax_combine(
	max1 madlib.ARGS_AND_VALUE_DOUBLE, max2 madlib.ARGS_AND_VALUE_DOUBLE)
RETURNS madlib.ARGS_AND_VALUE_DOUBLE AS
$$
	SELECT CASE WHEN $1 IS NULL OR $1.value < $2.value THEN $2
				WHEN $2 IS NULL OR $1.value > $2.value THEN $1
				ELSE ($1.args || $2.args, $1.value)::madlib.ARGS_AND_VALUE_DOUBLE
		   END
$$
LANGUAGE sql IMMUTABLE;

CREATE FUNCTION madlib.argmax_final(finalstate madlib.ARGS_AND_VALUE_DOUBLE)
RETURNS INTEGER[] AS
$$
	SELECT $1.args
$$
LANGUAGE sql IMMUTABLE;


CREATE AGGREGATE madlib.argmax(INTEGER, DOUBLE PRECISION) (
	SFUNC=madlib.argmax_transition,
	STYPE=madlib.ARGS_AND_VALUE_DOUBLE,
	PREFUNC=madlib.argmax_combine,
	FINALFUNC=madlib.argmax_final
);

-- bayes_training_sql(inTableName VARCHAR, inClassColumn VARCHAR,
--     inAttrColumn VARCHAR, inNumAttrs INTEGER)
-- --------------------------------------------------------------
-- Generate SQL that returns a query with all training data, in a form that
-- makes it easy to do Naive Bayes classification on new data.

CREATE FUNCTION madlib.bayes_training_sql(inTableName VARCHAR,
	inClassColumn VARCHAR,
	inAttrColumn VARCHAR,
	inNumAttrs INTEGER)
RETURNS TEXT AS $$
DECLARE
	sql	TEXT;
	tableName VARCHAR;
	classColumn VARCHAR;
	attrColumn VARCHAR;
BEGIN
	tableName := inTableName::REGCLASS;
	classColumn := quote_ident(inClassColumn);
	attrColumn := quote_ident(inAttrColumn);

	sql :=
	'SELECT
		triple_cnt.class,
		triple_cnt.attr,
		triple_cnt.value,
		class_cnt.cnt::BIGINT,
		triple_cnt.cnt::DOUBLE PRECISION / class_cnt.cnt AS prob
	FROM
	(
		SELECT class, attr, value, count(*) AS cnt FROM
		(
			SELECT
				' || inClassColumn || ' AS class,
				generate_series(1,' || inNumAttrs || ') AS attr,
				unnest( ' || inAttrColumn || '[1:' || inNumAttrs || '] ) AS value
			FROM ' || tableName || '
		) AS bayes_unnested
		GROUP BY class, attr, value
	) AS triple_cnt,
	(
		SELECT class, count(*) AS cnt FROM ' || tableName || '
		GROUP BY class
	) AS class_cnt
	WHERE triple_cnt.class = class_cnt.class';
	
	RETURN sql;
END;
$$
LANGUAGE plpgsql IMMUTABLE;

-- bayes_training(inTableName VARCHAR, inClassColumn VARCHAR,
--     inAttrColumn VARCHAR, inNumAttrs INTEGER)
-- ----------------------------------------------------------
-- Return Training data on the fly, i.e., this function can be used as a FROM-
-- source in other SQL statements.
-- Caveat: As of PostgreSQL 9/Greenplum 4, the result has to fit entirely into
-- memory. The view-creating function should be used for larger training data.
--
-- FIXME:  RETURN QUERY EXECUTE only works on recent PostgreSQL versions, not
-- FIXME:  on Greenplum. This will be fixed.

CREATE FUNCTION madlib.bayes_training(
	inTableName VARCHAR,
	inClassColumn VARCHAR,
	inAttrColumn VARCHAR,
	inNumAttrs INTEGER)
RETURNS TABLE(class INTEGER, attr INTEGER, value INTEGER, cnt BIGINT, prob DOUBLE PRECISION) AS $$
BEGIN
	RETURN QUERY EXECUTE madlib.bayes_training_sql(inTableName, inClassColumn, inAttrColumn, inNumAttrs);
END;
$$
LANGUAGE plpgsql STABLE;


-- create_bayes_training_view(inTableName VARCHAR, inClassColumn VARCHAR,
--     inAttrColumn VARCHAR, inNumAttrs INTEGER)
-- ----------------------------------------------------------------------
-- Create a view out of the training data, in a form that makes it easy to do
-- Naive Bayes classification on new data.
--
-- FIXME: This code is prone to code injection! The quoted view name is not used!

CREATE FUNCTION madlib.create_bayes_training_view(
	inTableName VARCHAR,
	inClassColumn VARCHAR,
	inAttrColumn VARCHAR,
	inNumAttrs INTEGER,
	inViewName VARCHAR)
RETURNS VOID AS $$
DECLARE
	viewName VARCHAR;
BEGIN
	viewName := quote_literal(inViewName);
	
	EXECUTE 'CREATE VIEW ' || inViewName || ' AS ' ||
		madlib.bayes_training_sql(inTableName, inClassColumn, inAttrColumn, inNumAttrs);
END;
$$
LANGUAGE plpgsql VOLATILE;



CREATE FUNCTION madlib.nb_classify(
	inTableName VARCHAR,
	inClassColumn VARCHAR,
	inAttrColumn VARCHAR,
	inNumAttrs INTEGER,
	inClassifyAttributes INTEGER[])
RETURNS INTEGER[] AS $$
DECLARE
	tableLiteral VARCHAR;
	classColumnLiteral VARCHAR;
	attrColumnLiteral VARCHAR;
	classifyattributesliteral VARCHAR;
	sql TEXT;
	classification INTEGER[];
BEGIN
	-- We have to be careful how to quote: A table name can be used as a string
	-- literal (when passed as an argument to a function in generated sql
	-- statements) or as an identifier (when used directly in a generated sql
	-- statement)

	tableLiteral := quote_literal(inTableName);
	classColumnLiteral := quote_literal(inClassColumn);
	attrColumnLiteral := quote_literal(inAttrColumn);
	classifyAttributesLiteral := 'array[' ||
		array_to_string(inClassifyAttributes, ',') || ']';
	
	sql :=
	'SELECT madlib.argmax(class, prob_scalar) FROM
	(
		SELECT
			training.class,
			CASE count(*) WHEN ' || inNumAttrs || ' THEN 1.0
						  ELSE 0.0
						  END
			* training.cnt * madlib.product(prob) AS prob_scalar
		FROM
			madlib.bayes_training(' || tableLiteral || ', '
				|| classColumnLiteral || ', '
				|| attrColumnLiteral || ', '
				|| inNumAttrs || ') AS training,
			(
				SELECT
					generate_series(1, ' || inNumAttrs || ') AS attr,
					unnest(' || classifyAttributesLiteral || ') AS value
			) AS data
		WHERE
			training.attr = data.attr AND training.value = data.value
		GROUP BY class, cnt
	) classes';
	
	EXECUTE sql INTO classification;
	
	RETURN classification;
END
$$
LANGUAGE plpgsql;


CREATE TABLE madlib.bayes
(
	id integer NOT NULL,
	class INTEGER,
	attributes INTEGER[],
	CONSTRAINT pk_bayes PRIMARY KEY (id)
);

-- The following is taken from the Greenplum Admin Guide

INSERT INTO madlib.bayes VALUES(1,	1,	'{1, 2, 3}');
INSERT INTO madlib.bayes VALUES(3,	1,	'{1, 4, 3}');
INSERT INTO madlib.bayes VALUES(5,	2,	'{0, 2, 2}');
INSERT INTO madlib.bayes VALUES(2,	1,	'{1, 2, 1}');
INSERT INTO madlib.bayes VALUES(4,	2,	'{1, 2, 2}');
INSERT INTO madlib.bayes VALUES(6,	2,	'{0, 1, 3}');

CREATE TABLE madlib.toclassify
(
	id SERIAL NOT NULL,
	attributes INTEGER[],
	CONSTRAINT pk_toclassify PRIMARY KEY (id)
);

INSERT INTO madlib.toclassify (attributes) VALUES ('{0, 2, 1}');
INSERT INTO madlib.toclassify (attributes) VALUES ('{1, 2, 3}');
