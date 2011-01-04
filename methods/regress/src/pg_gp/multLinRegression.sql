-- State transition function
CREATE FUNCTION madlib.float8_mregr_accum(DOUBLE PRECISION[], DOUBLE PRECISION, DOUBLE PRECISION[])
RETURNS DOUBLE PRECISION[]
AS 'multLinRegression'
LANGUAGE C
IMMUTABLE STRICT;

-- Function for merging two transition states
CREATE OR REPLACE FUNCTION madlib.float8_mregr_combine(DOUBLE PRECISION[], DOUBLE PRECISION[])
RETURNS DOUBLE PRECISION[]
AS 'multLinRegression'
LANGUAGE C
IMMUTABLE STRICT;

-- Final functions
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

CREATE TYPE madlib.LRegrState AS (
	len		INTEGER,
	count	BIGINT,
	coef	DOUBLE PRECISION[],
	dir		DOUBLE PRECISION[],
	grad	DOUBLE PRECISION[],
	beta	DOUBLE PRECISION,
	gradNew	DOUBLE PRECISION[],
	dTHd	DOUBLE PRECISION
);

CREATE FUNCTION madlib.float8_cg_update_accum(madlib.LRegrState, BOOLEAN, DOUBLE PRECISION[])
RETURNS madlib.LRegrState
AS 'multLinRegression'
LANGUAGE C;

CREATE FUNCTION madlib.float8_cg_update_final(madlib.LRegrState)
RETURNS madlib.LRegrState
AS 'multLinRegression'
LANGUAGE C STRICT;

CREATE AGGREGATE madlib.logregr_coef(BOOLEAN, DOUBLE PRECISION[]) (
	SFUNC=madlib.float8_cg_update_accum,
	STYPE=madlib.LRegrState,
	FINALFUNC=madlib.float8_cg_update_final
);
