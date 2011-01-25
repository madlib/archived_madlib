\qecho =================================================================
\qecho === Running: Naive-Bayes Classification =========================
\qecho =================================================================
\qecho 
\qecho === Refresh table: bayes ========================================

DROP TABLE bayes;
CREATE TABLE bayes (id INT, class INT, attributes INT[]) distributed by (ID);
INSERT INTO bayes VALUES(   1 ,     1 , array[1,2,3]);
INSERT INTO bayes VALUES(   3 ,     1 , array[1,4,3]);
INSERT INTO bayes VALUES(   5 ,     2 , array[0,2,2]);
INSERT INTO bayes VALUES(   2 ,     1 , array[1,2,1]);
INSERT INTO bayes VALUES(   4 ,     2 , array[1,2,2]);
INSERT INTO bayes VALUES(   6 ,     2 , array[0,1,3]);

SET CLIENT_MIN_MESSAGES=WARNING;

\qecho === Precompute feature probabilities and class priors ===========
DROP TABLE IF EXISTS nb_class_priors;
DROP TABLE IF EXISTS nb_feature_probs;
SELECT madlib.create_nb_prepared_data_tables(
     'bayes', 'class', 'attributes', 3,
     'nb_feature_probs', 'nb_class_priors');

select * from nb_class_priors;

select * from toclassify;

\qecho === Run Naive Bayes =============================================
select madlib.create_nb_classify_view(
      'nb_feature_probs', 'nb_class_priors',
      'toclassify', 'id', 'attributes', 3,
      'nb_classify_view_fast');
