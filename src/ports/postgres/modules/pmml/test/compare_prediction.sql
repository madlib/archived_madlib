\timing off
SET client_min_messages=ERROR;
SET search_path=public,madlib;

CREATE TABLE no_grouping_compare (
    "1" float8,
    treatment float8,
    trait_anxiety float8,
    g1 char(1),
    g2 char(1),
    ground_truth boolean,
    madlib_prediction boolean,
    second_attack boolean);

COPY no_grouping_compare FROM
'/tmp/pmml/out_no_grouping.csv'
DELIMITER ',' CSV HEADER;

SELECT assert(count(*) = 20, 'no_grouping wrong answer!')
FROM no_grouping_compare
WHERE madlib_prediction = second_attack;

DROP TABLE no_grouping_compare;

---------------------------------------------------------------------------
CREATE TABLE simple_grouping_compare (
    "1" float8,
    treatment float8,
    trait_anxiety float8,
    g1 char(1),
    g2 char(1),
    ground_truth boolean,
    madlib_prediction boolean,
    second_attack boolean);

COPY simple_grouping_compare FROM
'/tmp/pmml/out_simple_grouping.csv'
DELIMITER ',' CSV HEADER;

SELECT assert(count(*) = 20, 'simple_grouping wrong answer!')
FROM simple_grouping_compare
WHERE madlib_prediction = second_attack;

DROP TABLE simple_grouping_compare;

---------------------------------------------------------------------------
CREATE TABLE complex_grouping_compare (
    "1" float8,
    treatment float8,
    trait_anxiety float8,
    g1 char(1),
    g2 char(1),
    ground_truth boolean,
    madlib_prediction boolean,
    second_attack boolean);

COPY complex_grouping_compare FROM
'/tmp/pmml/out_complex_grouping.csv'
DELIMITER ',' CSV HEADER;

SELECT assert(count(*) = 20, 'complex_grouping wrong answer!')
FROM complex_grouping_compare
WHERE madlib_prediction = second_attack;

DROP TABLE IF EXISTS complex_grouping_compare;

---------------------------------------------------------------------------
CREATE TABLE linregr_grouping_compare (
    "c.1" float8,
    "c.bedroom" float8,
    "c.bath" float8,
    "c.size" float8,
    g1 char(1),
    g2 char(1),
    ground_truth float8,
    madlib_prediction float8,
    "c.price" float8);

COPY linregr_grouping_compare FROM
'/tmp/pmml/out_linregr_grouping.csv'
DELIMITER ',' CSV HEADER;

SELECT assert(count(*) = 15, 'linregr_grouping wrong answer!')
FROM linregr_grouping_compare
WHERE relative_error(madlib_prediction, "c.price") < 1e-4;

DROP TABLE IF EXISTS linregr_grouping_compare;

-- DROP TABLE IF EXISTS patients CASCADE;
-- DROP TABLE IF EXISTS no_grouping CASCADE;
-- DROP TABLE IF EXISTS simple_grouping CASCADE;
-- DROP TABLE IF EXISTS complex_grouping CASCADE;
-- DROP TABLE IF EXISTS no_grouping_summary CASCADE;
-- DROP TABLE IF EXISTS simple_grouping_summary CASCADE;
-- DROP TABLE IF EXISTS complex_grouping_summary CASCADE;
