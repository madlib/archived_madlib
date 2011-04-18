-- Can use string_to_array() to convert texts into arrays.
-- E.g. string_to_array('This is good', ' ') produces {This,is,good}.
-- Can use R's lda.lexicalize() function to convert texts into the 
-- desired format.

DROP TABLE IF EXISTS madlib.lda_mycorpus;
CREATE TABLE madlib.lda_mycorpus ( id int4, contents int4[] ) DISTRIBUTED BY (id);
INSERT INTO madlib.lda_mycorpus VALUES 
 (1, '{1,2,3,4,5,6,7}'), 
 (2, '{8,9,10,11,12,10,6,13,14,15}'),
 (3, '{16,17,11,3,18,13}'),
 (4, '{13,19,1,13,20,21,10,17}'),
 (5, '{22,10,11,23,14,15,24,25,18}'),
 (6, '{16,26,10,27,28,29,30}'),
 (7, '{16,31,32,10,33,34,30}'),
 (8, '{32,35,36,37,10,30,19,38,39,29}'),
 (9, '{32,35,8,9}') ;

DROP TABLE IF EXISTS madlib.lda_mydict;
CREATE TABLE madlib.lda_mydict ( dict text[] ) DISTRIBUTED RANDOMLY;
insert into madlib.lda_mydict values
 ('{human,machine,interface,for,abc,computer,applications,a,survey,of,
    user,opinion,system,response,time,the,eps,management,and,engineering,
    testing,relation,perceived,to,error,generation,random,binary,order,tree,
    intersection,graph,path,in,minor,IV,widths,well,quasi}');


