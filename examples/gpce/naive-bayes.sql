\qecho =================================================================
\qecho === Running: Naive-Bayes Classification =========================
\qecho =================================================================
\qecho 
\qecho === Refresh table: bayes ========================================

SET client_min_messages = error;

DROP TABLE IF EXISTS bayes CASCADE;
DROP TABLE IF EXISTS toclassify CASCADE;

CREATE TABLE bayes
(
	id integer NOT NULL,
	class INTEGER,
	attributes INTEGER[],
	CONSTRAINT pk_bayes PRIMARY KEY (id)
);

COPY bayes (id, class, attributes) FROM stdin;
1	1	{1, 2, 3}
3	1	{1, 4, 3}
5	2	{0, 2, 2}
2	1	{1, 2, 1}
4	2	{1, 2, 2}
6	2	{0, 1, 3}
\.


\qecho === Show training data ==========================================

SELECT * FROM bayes;


\qecho === Refresh table: toclassify ===================================

CREATE TABLE toclassify
(
	id SERIAL NOT NULL,
	attributes INTEGER[],
	CONSTRAINT pk_toclassify PRIMARY KEY (id)
);

COPY toclassify (attributes) FROM stdin;
{0, 2, 1}
{1, 2, 3}
\.


\qecho === Show Data we want to run Naive Bayes classification on ======

SELECT * FROM toclassify;


\qecho === Precompute feature probabilities and class priors ===========

DROP TABLE IF EXISTS nb_class_priors CASCADE;
DROP TABLE IF EXISTS nb_feature_probs CASCADE;

SELECT madlib.create_nb_prepared_data_tables(
    'bayes', 'class', 'attributes', 3,
    'nb_feature_probs', 'nb_class_priors');


\qecho === Show feature probabilities and class priors =================

SELECT * FROM nb_feature_probs ORDER BY class, attr, value;

SELECT * FROM nb_class_priors ORDER BY class;


\qecho === Run Naive Bayes =============================================

DROP VIEW IF EXISTS nb_classify_view_fast;

SELECT madlib.create_nb_classify_view(
      'nb_feature_probs', 'nb_class_priors',
      'toclassify', 'id', 'attributes', 3,
      'nb_classify_view_fast');

SELECT * from nb_classify_view_fast;


\qecho === Look at the probabilities for each class
\qecho === (Note we use Laplacian Smoothing):

DROP VIEW IF EXISTS nb_probs_view_fast;

SELECT madlib.create_nb_probs_view(
    'nb_feature_probs', 'nb_class_priors',
    'toclassify', 'id', 'attributes', 3,
    'nb_probs_view_fast');

SELECT * FROM nb_probs_view_fast;

RESET client_min_messages;
