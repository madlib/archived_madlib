--! @file multLinRegression.sql
--! This file contains regression code.

--! State transition function.
--! @param state The transition state
--! @param y The y value of the current row
--! @param x The x array of the current row
CREATE FUNCTION madlib.float8_mregr_accum(state DOUBLE PRECISION[], y DOUBLE PRECISION, x DOUBLE PRECISION[])
RETURNS DOUBLE PRECISION[]
AS 'multLinRegression'
LANGUAGE C
IMMUTABLE STRICT;

--! Function for merging two transition states
--! @param state1 First state
--! @param state2 Second state
--! @returns A merged state
CREATE OR REPLACE FUNCTION madlib.float8_mregr_combine(state1 DOUBLE PRECISION[], state2 DOUBLE PRECISION[])
RETURNS DOUBLE PRECISION[]
AS 'multLinRegression'
LANGUAGE C
IMMUTABLE STRICT;

--! Final functions
CREATE FUNCTION madlib.float8_mregr_coef(DOUBLE PRECISION[])
RETURNS DOUBLE PRECISION[]
AS 'multLinRegression'
LANGUAGE C STRICT;

CREATE FUNCTION madlib.float8_mregr_r2(DOUBLE PRECISION[])
RETURNS DOUBLE PRECISION
AS 'multLinRegression'
LANGUAGE C STRICT;

CREATE FUNCTION madlib.float8_mregr_tstats(DOUBLE PRECISION[])
RETURNS DOUBLE PRECISION[]
AS 'multLinRegression'
LANGUAGE C STRICT;

CREATE FUNCTION madlib.float8_mregr_pvalues(DOUBLE PRECISION[])
RETURNS DOUBLE PRECISION[]
AS 'multLinRegression'
LANGUAGE C STRICT;

-- No warning message while creating aggregates. (PREFUNC is a Greenplum-only
-- attribute)
SET client_min_messages = error;

CREATE AGGREGATE madlib.mregr_coef(DOUBLE PRECISION, DOUBLE PRECISION[]) (
	SFUNC=madlib.float8_mregr_accum,
	STYPE=float8[],
	FINALFUNC=madlib.float8_mregr_coef,
	PREFUNC=madlib.float8_mregr_combine,
	INITCOND='{0}'
);

CREATE AGGREGATE madlib.mregr_r2(DOUBLE PRECISION, DOUBLE PRECISION[]) (
	SFUNC=madlib.float8_mregr_accum,
	STYPE=float8[],
	FINALFUNC=madlib.float8_mregr_r2,
	PREFUNC=madlib.float8_mregr_combine,
	INITCOND='{0}'
);

CREATE AGGREGATE madlib.mregr_tstats(DOUBLE PRECISION, DOUBLE PRECISION[]) (
	SFUNC=madlib.float8_mregr_accum,
	STYPE=float8[],
	FINALFUNC=madlib.float8_mregr_tstats,
	PREFUNC=madlib.float8_mregr_combine,
	INITCOND='{0}'
);

CREATE AGGREGATE madlib.mregr_pvalues(DOUBLE PRECISION, DOUBLE PRECISION[]) (
	SFUNC=madlib.float8_mregr_accum,
	STYPE=float8[],
	FINALFUNC=madlib.float8_mregr_pvalues,
	PREFUNC=madlib.float8_mregr_combine,
	INITCOND='{0}'
);

RESET client_min_messages;

CREATE FUNCTION madlib.student_t_cdf(INTEGER, DOUBLE PRECISION)
RETURNS DOUBLE PRECISION
AS 'multLinRegression'
LANGUAGE C
IMMUTABLE STRICT;

CREATE TYPE madlib.logregr_cg_state AS (
	iteration		INTEGER,
	len				INTEGER,
	coef			DOUBLE PRECISION[],
	dir				DOUBLE PRECISION[],
	grad			DOUBLE PRECISION[],
	beta			DOUBLE PRECISION,

	count			BIGINT,
	gradNew			DOUBLE PRECISION[],
	dTHd			DOUBLE PRECISION,
	logLikelihood	DOUBLE PRECISION
);

CREATE TYPE madlib.logregr_irls_state AS (
	coef			DOUBLE PRECISION[],
	logLikelihood	DOUBLE PRECISION
);

CREATE FUNCTION madlib.float8_cg_update_accum(
	madlib.logregr_cg_state,
	BOOLEAN,
	DOUBLE PRECISION[],
	madlib.logregr_cg_state)
RETURNS madlib.logregr_cg_state
AS 'multLinRegression'
LANGUAGE C;

CREATE FUNCTION madlib.float8_irls_update_accum(
	DOUBLE PRECISION[],
	BOOLEAN,
	DOUBLE PRECISION[],
	madlib.logregr_irls_state)
RETURNS DOUBLE PRECISION[]
AS 'multLinRegression'
LANGUAGE C;

CREATE FUNCTION madlib.float8_cg_update_final(madlib.logregr_cg_state)
RETURNS madlib.logregr_cg_state
AS 'multLinRegression'
LANGUAGE C STRICT;

CREATE FUNCTION madlib.float8_irls_update_final(DOUBLE PRECISION[])
RETURNS madlib.logregr_irls_state
AS 'multLinRegression'
LANGUAGE C STRICT;

CREATE AGGREGATE madlib.logreg_cg_step(BOOLEAN, DOUBLE PRECISION[], madlib.logregr_cg_state) (
	SFUNC=madlib.float8_cg_update_accum,
	STYPE=madlib.logregr_cg_state,
	FINALFUNC=madlib.float8_cg_update_final
);

CREATE AGGREGATE madlib.logreg_irls_step(BOOLEAN, DOUBLE PRECISION[], madlib.logregr_irls_state) (
	SFUNC=madlib.float8_irls_update_accum,
	STYPE=float8[],
	PREFUNC=madlib.float8_mregr_combine,
	FINALFUNC=madlib.float8_irls_update_final,
	INITCOND='{0}'
);

CREATE FUNCTION madlib.logreg_should_terminate(
	DOUBLE PRECISION[],
	DOUBLE PRECISION[],
	VARCHAR,
	DOUBLE PRECISION)
RETURNS BOOLEAN
AS 'multLinRegression'
LANGUAGE C STRICT;

CREATE FUNCTION madlib.logreg_coef(
	"source" VARCHAR,
	"depColumn" VARCHAR,
	"indepColumn" VARCHAR)
RETURNS DOUBLE PRECISION[] AS $$
	import logRegress
	return logRegress.compute_logreg_coef(**globals())
$$ LANGUAGE plpythonu VOLATILE;


CREATE FUNCTION madlib.logreg_coef(
	"source" VARCHAR,
	"depColumn" VARCHAR,
	"indepColumn" VARCHAR,
	"numIterations" INTEGER)
RETURNS DOUBLE PRECISION[] AS $$
	import logRegress
	return logRegress.compute_logreg_coef(**globals())
$$ LANGUAGE plpythonu VOLATILE;


CREATE FUNCTION madlib.logreg_coef(
	"source" VARCHAR,
	"depColumn" VARCHAR,
	"indepColumn" VARCHAR,
	"numIterations" INTEGER,
	"optimizer" VARCHAR)
RETURNS DOUBLE PRECISION[] AS $$
	import logRegress
	return logRegress.compute_logreg_coef(**globals())
$$ LANGUAGE plpythonu VOLATILE;


--! Logistic regression
--! 
--! @param source Name of the source relation containing the training data
--! @param depColumn Name of the dependent column (of type BOOLEAN)
--! @param indepColumn Name of the independent column (of type DOUBLE
--!		PRECISION[])
--! @param numIterations The maximum number of iterations
--! @param optimizer The optimizer to use (either
--!		<tt>'ilrs'</tt>/<tt>'newton'</tt> for iteratively reweighted least
--!		squares or <tt>'cg'</tt> for conjugent gradient)
--! @param precision The difference between log-likelihood values in successive
--!		iterations that should indicate convergence, or 0 indicating that
--!		log-likelihood values should be ignored
CREATE FUNCTION madlib.logreg_coef(
	"source" VARCHAR,
	"depColumn" VARCHAR,
	"indepColumn" VARCHAR,
	"numIterations" INTEGER,
	"optimizer" VARCHAR,
	"precision" DOUBLE PRECISION)
RETURNS DOUBLE PRECISION[] AS $$
	import logRegress
	return logRegress.compute_logreg_coef(**globals())
$$ LANGUAGE plpythonu VOLATILE;


CREATE FUNCTION madlib.logr_coef(iterations INTEGER)
RETURNS madlib.logregr_cg_state
AS $$
DECLARE
	i INTEGER;
	state madlib.logregr_cg_state;
BEGIN
	state := NULL;
	FOR i in 1..iterations LOOP
		state := (SELECT madlib.logregr_coef(state, a.y, a.x) FROM madlib_examples.artificiallogreg AS a);
		RAISE NOTICE 'After iteration %: %', i, state;
	END LOOP;
	RETURN state;
END;
$$ LANGUAGE plpgsql;

CREATE FUNCTION madlib.init_python_paths()
RETURNS VOID AS
$$
	# FIXME: The following code should of course not reside in a specialized
	# module such as regression.sql
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
