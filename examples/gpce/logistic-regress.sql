\qecho =================================================================
\qecho === Running: logistic regression ================================
\qecho =================================================================
\qecho 
\qecho === Creating artificial random data =============================
\qecho === For the model
\qecho ===   E[y] = logit^(-1) ( x^T * [-2.4, 3.1, 0.6, -1.6, 1.3] )
\qecho === we generate 5000 rows (y, x[]), where each x[i] is a random
\qecho === variate corresponding to normal distribution with mean x[i]
\qecho === and standard deviation 1.


SET client_min_messages = error;

DROP TABLE IF EXISTS artificiallogreg;
DROP TABLE IF EXISTS randomdata;

CREATE TABLE artificiallogreg
(
	id SERIAL NOT NULL,
	y BOOLEAN,
	x REAL[]
);

RESET client_min_messages;

-- Create an array of normally distributed random variates
-- We use the Marsaglia polar method
CREATE OR REPLACE FUNCTION randomNormalArray(n INTEGER)
RETURNS DOUBLE PRECISION[] AS $$
DECLARE
	u DOUBLE PRECISION;
	v DOUBLE PRECISION;
	s DOUBLE PRECISION;
	x DOUBLE PRECISION;
	y DOUBLE PRECISION;
	i INTEGER;
	theArray DOUBLE PRECISION[];
BEGIN
	FOR i IN 1..(n+1)/2 LOOP
		LOOP
			u = random() * 2. - 1.;
			v = random() * 2. - 1.;
			s = u * u + v * v;
			EXIT WHEN s < 1.;
		END LOOP;
		x = u * sqrt(-2. * ln(s) / s);
		y = v * sqrt(-2. * ln(s) / s);

		theArray[2 * i - 1] = x;
		IF 2 * i <= n THEN
			theArray[2 * i] = y;
		END IF;
	END LOOP;
	RETURN theArray;
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION dotProduct(
    vec1 DOUBLE PRECISION[],
    vec2 DOUBLE PRECISION[])
RETURNS DOUBLE PRECISION AS $$
DECLARE
	sum DOUBLE PRECISION;
	dim INTEGER;
	i INTEGER;
BEGIN
	IF array_lower(vec1, 1) != 1 OR array_lower(vec2, 1) != 1 OR
		array_upper(vec1, 1) != array_upper(vec2, 1) THEN
		RETURN NULL;
	END IF;
	dim = array_upper(vec1, 1);
	sum = 0.;
	FOR i in 1..dim LOOP
		sum := sum + vec1[i] * vec2[i];
	END LOOP;
	RETURN sum;
END;
$$ LANGUAGE plpgsql;

SET client_min_messages = error;
CREATE TABLE randomdata AS
SELECT id, randomNormalArray(5)::REAL[] AS x
FROM generate_series(1,5000) AS id;
RESET client_min_messages;

INSERT INTO artificiallogreg (y, x)
SELECT
	random() < 1. / (1. + exp(-dotProduct(r.x, c.coef))),
	r.x
FROM randomdata AS r, (SELECT array[-2.4, 3.1, 0.6, -1.6, 1.3] AS coef) AS c;

\qecho === Show data sample (top 5 rows only) ==========================

SELECT * FROM artificiallogreg LIMIT 5;

\qecho === Calculate Coefficients from artificial data: ================

SELECT madlib.logregr_coef(
	'artificiallogreg', 'y', 'x', 20, 'irls', 0.001
)::REAL[];
