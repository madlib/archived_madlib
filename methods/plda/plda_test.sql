\timing
drop table if exists madlib.lda_mymodel;
drop table if exists madlib.lda_corpus;
drop table if exists madlib.lda_testcorpus;
create table 
madlib.lda_testcorpus ( id int4, contents int4[], topics madlib.lda_topics_t ) 
distributed randomly;
insert into madlib.lda_testcorpus (select * from madlib.lda_mycorpus limit 10);
select madlib.lda_run('madlib.lda_mycorpus', 'madlib.lda_mydict', 'madlib.lda_mymodel', 'madlib.lda_corpus', 100,10,0.5,0.5);
select madlib.lda_label_test_documents('madlib.lda_testcorpus', 'madlib.lda_mymodel', 'madlib.lda_corpus', 'madlib.lda_mydict', 100,10,0.5,0.5);
select id, contents[1:5], (topics).topics[1:5], (topics).topic_d from madlib.lda_testcorpus;