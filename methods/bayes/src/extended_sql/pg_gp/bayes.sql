CREATE SCHEMA madlib;

CREATE LANGUAGE plpythonu;

-- In the following, we implement an argmax function where the "index set" is
-- INTEGER and we range over DOUBLE PRECISION values.
-- argmax should only be used on unsorted data because it will not exploit
-- indices, and its running time is Theta(n).
-- The implementation is in SQL, with a flavor of functional programming.
-- The hope is that the optimizer does a good job here.

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
	SELECT CASE WHEN $3 < $1.value OR $2 IS NULL OR ($3 IS NULL AND NOT $1.value IS NULL) THEN $1
				WHEN $3 = $1.value OR ($3 IS NULL AND $1.value IS NULL AND NOT $1.args IS NULL)
					THEN ($1.args || $2, $3)::madlib.ARGS_AND_VALUE_DOUBLE
				ELSE (array[$2], $3)::madlib.ARGS_AND_VALUE_DOUBLE
		   END
$$
LANGUAGE sql IMMUTABLE;


CREATE FUNCTION madlib.argmax_combine(
	max1 madlib.ARGS_AND_VALUE_DOUBLE, max2 madlib.ARGS_AND_VALUE_DOUBLE)
RETURNS madlib.ARGS_AND_VALUE_DOUBLE AS
$$
	-- If SQL guaranteed short-circuit evaluation, the following could become
	-- shorter. Unfortunately, this is not the case.
	-- Section 6.3.3.3 of ISO/IEC 9075-1:2008 Framework (SQL/Framework):
	--
	--  "However, it is implementation-dependent whether expressions are
	--   actually evaluated left to right, particularly when operands or
	--   operators might cause conditions to be raised or if the results of the
	--   expressions can be determined without completely evaluating all parts
	--   of the expression."
	--
	-- Again, the optimizer does its job hopefully.
	SELECT CASE WHEN $1 IS NULL THEN $2
				WHEN $2 IS NULL THEN $1
				WHEN ($1.value = $2.value) OR ($1.value IS NULL AND $2.value IS NULL)
					THEN ($1.args || $2.args, $1.value)::madlib.ARGS_AND_VALUE_DOUBLE
				WHEN $1.value IS NULL OR $1.value < $2.value THEN $2
				ELSE $1
		   END
$$
LANGUAGE sql IMMUTABLE;


CREATE FUNCTION madlib.argmax_final(finalstate madlib.ARGS_AND_VALUE_DOUBLE)
RETURNS INTEGER[] AS
$$
	SELECT $1.args
$$
LANGUAGE sql IMMUTABLE;

-- No warning message while creating aggregates. (PREFUNC is a Greenplum-only
-- attribute)
SET client_min_messages = error;

CREATE AGGREGATE madlib.argmax(INTEGER, DOUBLE PRECISION) (
	SFUNC=madlib.argmax_transition,
	STYPE=madlib.ARGS_AND_VALUE_DOUBLE,
	PREFUNC=madlib.argmax_combine,
	FINALFUNC=madlib.argmax_final
);

RESET client_min_messages;


CREATE FUNCTION madlib.create_nb_prepared_data_tables(
	"trainingSource" VARCHAR,
	"trainingClassColumn" VARCHAR,
	"trainingAttrColumn" VARCHAR,
	"numAttrs" INTEGER,
	"featureProbsDestName" VARCHAR,
	"classPriorsDestName" VARCHAR)
RETURNS VOID AS $$
	import bayes
	global whatToCreate

	whatToCreate = 'TABLE'
	bayes.create_prepared_data(**globals())
$$ LANGUAGE plpythonu VOLATILE;


CREATE FUNCTION madlib.create_nb_classify_view(
	"featureProbsSource" VARCHAR,
	"classPriorsSource" VARCHAR,
	"classifySource" VARCHAR,
	"classifyKeyColumn" VARCHAR,
	"classifyAttrColumn" VARCHAR,
	"numAttrs" INTEGER,
	"destName" VARCHAR)
RETURNS VOID AS $$
	import bayes
	global whatToCreate
	
	whatToCreate = 'VIEW'
	bayes.create_classification(**globals())
$$ LANGUAGE plpythonu VOLATILE;


CREATE FUNCTION madlib.create_nb_classify_view(
	"trainingSource" VARCHAR,
	"trainingClassColumn" VARCHAR,
	"trainingAttrColumn" VARCHAR,
	"classifySource" VARCHAR,
	"classifyKeyColumn" VARCHAR,
	"classifyAttrColumn" VARCHAR,
	"numAttrs" INTEGER,
	"destName" VARCHAR)
RETURNS VOID AS $$
	import bayes
	global whatToCreate
	
	whatToCreate = 'VIEW'
	bayes.create_classification(**globals())
$$ LANGUAGE plpythonu VOLATILE;


CREATE FUNCTION madlib.create_nb_probs_view(
	"trainingSource" VARCHAR,
	"trainingClassColumn" VARCHAR,
	"trainingAttrColumn" VARCHAR,
	"classifySource" VARCHAR,
	"classifyKeyColumn" VARCHAR,
	"classifyAttrColumn" VARCHAR,
	"numAttrs" INTEGER,
	"destName" VARCHAR)
RETURNS VOID AS $$
	import bayes
	global whatToCreate
	
	whatToCreate = 'VIEW'
	bayes.create_bayes_probabilities(**globals())
$$ LANGUAGE plpythonu VOLATILE;


CREATE FUNCTION madlib.create_nb_probs_view(
	"featureProbsSource" VARCHAR,
	"classPriorsSource" VARCHAR,
	"classifySource" VARCHAR,
	"classifyKeyColumn" VARCHAR,
	"classifyAttrColumn" VARCHAR,
	"numAttrs" INTEGER,
	"destName" VARCHAR)
RETURNS VOID AS $$
	import bayes
	global whatToCreate
	
	whatToCreate = 'VIEW'
	bayes.create_bayes_probabilities(**globals())
$$ LANGUAGE plpythonu VOLATILE;


CREATE FUNCTION madlib.create_nb_classify_fn(
	"featureProbsSource" VARCHAR,
	"classPriorsSource" VARCHAR,
	"numAttrs" INTEGER,
	"destName" VARCHAR)
RETURNS VOID AS $$
	import bayes
	bayes.create_classification_function(**globals())
$$ LANGUAGE plpythonu VOLATILE;


CREATE FUNCTION madlib.create_nb_classify_fn(
	"trainingSource" VARCHAR,
	"trainingClassColumn" VARCHAR,
	"trainingAttrColumn" VARCHAR,
	"numAttrs" INTEGER,
	"destName" VARCHAR)
RETURNS VOID AS $$
	import bayes
	bayes.create_classification_function(**globals())
$$ LANGUAGE plpythonu VOLATILE;


CREATE FUNCTION madlib.update_nb_classification(
	"featureProbsSource" VARCHAR,
	"classPriorsSource" VARCHAR,
	"numAttrs" INTEGER,
	"destName" VARCHAR,
	"classifyAttrColumn" VARCHAR,
	"destColumn" VARCHAR)
RETURNS VOID AS $$
	import bayes
	bayes.update_classification(**globals())
$$ LANGUAGE plpythonu VOLATILE;


CREATE FUNCTION madlib.init_python_paths()
RETURNS VOID AS
$$
	# FIXME: The following code should of course not reside in a specialized
	# module such as bayes.sql
	import sys

	dyld_paths = plpy.execute(
		"SHOW dynamic_library_path")[0]["dynamic_library_path"].split(':')
	before_default = True
	count = 0
	for path in dyld_paths:
		if path == "$libdir":
			before_default = False
		else:
			if before_default:
				sys.path.insert(count, path)
				count += 1
			else:
				sys.path.append(path)
$$ LANGUAGE plpythonu VOLATILE;

-- In the following part, create the functionality inherited from Greenplum

-- State type
CREATE TYPE nb_classification AS
   (classes TEXT[],
    accum DOUBLE PRECISION[],
    apriori BIGINT[]);

-- State transition function
CREATE FUNCTION madlib.nb_classify_accum(nb_classification, TEXT[], BIGINT,
	BIGINT[], BIGINT[])
RETURNS nb_classification
AS 'bayes'
LANGUAGE C
IMMUTABLE;

-- Function for merging two transition states
CREATE OR REPLACE FUNCTION madlib.nb_classify_combine(nb_classification,
	nb_classification)
RETURNS nb_classification
AS 'multLinRegression'
LANGUAGE C
IMMUTABLE;

-- Final function
CREATE FUNCTION madlib.nb_classify_final(nb_classification)
RETURNS TEXT
AS 'bayes'
LANGUAGE C 
IMMUTABLE STRICT;

-- No warning message while creating aggregates. (PREFUNC is a Greenplum-only
-- attribute)
SET client_min_messages = error;

CREATE AGGREGATE madlib.nb_classify(TEXT[], BIGINT, BIGINT[], BIGINT[]) (
	SFUNC=madlib.nb_classify_accum,
	STYPE=madlib.nb_classification,
	FINALFUNC=madlib.nb_classify_final,
	PREFUNC=madlib.nb_classify_combine
);

RESET client_min_messages;
