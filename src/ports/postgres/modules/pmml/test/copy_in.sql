\timing off
SET client_min_messages=ERROR;
SET search_path=public,madlib;

DROP TABLE IF EXISTS patients;
DROP TABLE IF EXISTS no_grouping;
DROP TABLE IF EXISTS simple_grouping;
DROP TABLE IF EXISTS complex_grouping;
DROP TABLE IF EXISTS no_grouping_summary;
DROP TABLE IF EXISTS simple_grouping_summary;
DROP TABLE IF EXISTS complex_grouping_summary;

DROP TABLE IF EXISTS pmml_logregr_example;
DROP TABLE IF EXISTS example_pmml;
DROP TABLE IF EXISTS example_pmml_summary;

DROP TABLE IF EXISTS houses;
DROP TABLE IF EXISTS linregr_grouping;
DROP TABLE IF EXISTS linregr_grouping_summary;

\i table_to_pmml.sql_in

COPY (
    SELECT
        1 AS "1",
        treatment,
        trait_anxiety,
        g1,
        g2,
        second_attack AS ground_truth,
        logregr_predict(
            coef,
            ARRAY[1, treatment, trait_anxiety]
        ) AS madlib_prediction
    FROM patients, no_grouping
) TO '/tmp/pmml/in_no_grouping.csv'
DELIMITER ',' CSV HEADER;

COPY (
    SELECT
        1 AS "1",
        treatment,
        trait_anxiety,
        g1,
        g2,
        second_attack AS ground_truth,
        logregr_predict(
            coef,
            ARRAY[1, treatment, trait_anxiety]
        ) AS madlib_prediction
    FROM patients JOIN simple_grouping USING (g1)
) TO '/tmp/pmml/in_simple_grouping.csv'
DELIMITER ',' CSV HEADER;

COPY (
    SELECT
        1 AS "1",
        treatment,
        trait_anxiety,
        g1,
        g2,
        second_attack AS ground_truth,
        logregr_predict(
            coef,
            ARRAY[1, treatment, trait_anxiety]
        ) AS madlib_prediction
    FROM patients JOIN complex_grouping USING (g1, g2)
) TO '/tmp/pmml/in_complex_grouping.csv'
DELIMITER ',' CSV HEADER;

COPY (
    SELECT
        1 AS "c.1",
        bedroom AS "c.bedroom",
        bath AS "c.bath",
        size AS "c.size",
        g1,
        g2,
        price AS ground_truth,
        linregr_predict(
            coef,
            array[1, bedroom, bath, size]
        ) AS madlib_prediction
    FROM houses JOIN linregr_grouping USING (g1, g2)
) TO '/tmp/pmml/in_linregr_grouping.csv'
DELIMITER ',' CSV HEADER;


