DROP TYPE IF EXISTS madlib.lda_topics_t CASCADE;
DROP FUNCTION IF EXISTS madlib.lda_zero_array(int4);
DROP FUNCTION IF EXISTS madlib.lda_random_topics(int4, int4);
-- DROP FUNCTION IF EXISTS madlib.lda_sample_new_topics(int4[], int4[], int4[], int4[], int4[], int4, int4, float, float);
DROP AGGREGATE IF EXISTS madlib.lda_cword_agg(int4[], int4[], int4, int4, int4);
DROP FUNCTION IF EXISTS madlib.lda_cword_count(int4[], int4[], int4[], int4, int4, int4);
DROP FUNCTION IF EXISTS madlib.lda_train(int4, int4, int4, float, float, text, text, text, text);
DROP TYPE IF EXISTS madlib.lda_word_weight CASCADE;
DROP TYPE IF EXISTS madlib.lda_word_distrn CASCADE;
DROP FUNCTION IF EXISTS madlib.lda_word_topic_distrn(int4[], int4, int4);
DROP  FUNCTION IF EXISTS madlib.lda_label_test_documents(text, text, text, text, int4, int4, float, float);

DROP FUNCTION IF EXISTS madlib.lda_run(text, text, text, text, int4, int4, float, float);
