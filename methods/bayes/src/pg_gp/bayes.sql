CREATE SCHEMA madlib;

CREATE LANGUAGE plpythonu;

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
	-- Hopefully, the optimizer does its job.
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


CREATE AGGREGATE madlib.argmax(INTEGER, DOUBLE PRECISION) (
	SFUNC=madlib.argmax_transition,
	STYPE=madlib.ARGS_AND_VALUE_DOUBLE,
	PREFUNC=madlib.argmax_combine,
	FINALFUNC=madlib.argmax_final
);


CREATE FUNCTION madlib.nb_create_prepared_data_tables(
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


CREATE FUNCTION madlib.nb_create_classify_view_with_prepared_data(
	"preparedTrainingSource" VARCHAR,
	"classesSource" VARCHAR,
	"classesIDColumn" VARCHAR,
	"inClassifySource" VARCHAR,
	"inClassifyKeyColumn" VARCHAR,
	"inClassifyAttrColumn" VARCHAR,
	"inNumAttrs" INTEGER,
	"inViewName" VARCHAR)
RETURNS VOID AS $$
	import bayes
	bayes.create_classification_view(globals());
$$ LANGUAGE plpythonu VOLATILE;


CREATE FUNCTION madlib.nb_create_classification_view(
	"inTrainingSource" VARCHAR,
	"inTrainingClassColumn" VARCHAR,
	"inTrainingAttrColumn" VARCHAR,
	"inClassifySource" VARCHAR,
	"inClassifyKeyColumn" VARCHAR,
	"inClassifyAttrColumn" VARCHAR,
	"inNumAttrs" INTEGER,
	"inViewName" VARCHAR)
RETURNS VOID AS $$
	import bayes;
	bayes.create_classification_view(globals());
$$ LANGUAGE plpythonu VOLATILE;


CREATE FUNCTION madlib.nb_create_classify_fn_with_prepared_data(
	"preparedTrainingSource" VARCHAR,
	"classesSource" VARCHAR,
	"classesIDColumn" VARCHAR,
	"numAttrs" INTEGER,
	"functionName" VARCHAR)
RETURNS VOID AS $$
	import bayes;
	bayes.create_classification_function(globals());
$$ LANGUAGE plpythonu VOLATILE;


CREATE FUNCTION madlib.nb_create_classify_fn(
	"trainingSource" VARCHAR,
	"trainingClassColumn" VARCHAR,
	"trainingAttrColumn" VARCHAR,
	"numAttrs" INTEGER,
	"functionName" VARCHAR)
RETURNS VOID AS $$
	import bayes;
	bayes.create_classification_function(globals());
$$ LANGUAGE plpythonu VOLATILE;


CREATE FUNCTION madlib.init_python_paths()
RETURNS VOID AS
$$
	# FIXME: The following code should of course not reside in bayes.sql
	import sys

	dyld_paths = plpy.execute("SHOW dynamic_library_path")[0]["dynamic_library_path"].split(':');
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
