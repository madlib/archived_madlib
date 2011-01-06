-- Unit-Testing checklist
-- 1. Make sure the function does the right thing on normal input(s).
-- 2. Make sure the function is non-destructive to the input(s).
-- 3. Make sure the function handles null values in svecs correctly.
-- 4. Make sure the function handles length-1 svecs correctly.
-- 5. Make sure the function handles null svecs properly.
-- 6. For functions that handle float8[] input(s), make sure the two
--    representations of null values are handled properly.

\timing
select dmin(1000,1000.1);
select dmin(1000,NULL);
select dmin(NULL,1000);
select dmin(NULL,NULL);
select dmax(1000,1000.1);
select dmax(1000,NULL);
select dmax(NULL,1000);
select dmax(NULL,NULL);

drop table if exists test_pairs;
create table test_pairs( id int, a svec, b svec ) distributed by (id);
insert into test_pairs values 
       (0, '{1,100,1}:{5,0,5}', '{50,50,2}:{1,2,10}'),
       (1, '{1,100,1}:{-5,0,-5}', '{50,50,2}:{-1,-2,-10}');
insert into test_pairs values 
--       (10, null, null),
       (11, '{1}:{0}'::svec, '{1}:{1}'::svec),
       (12, '{1}:{5}'::svec, '{3}:{-8}'::svec),
       (13, '{1}:{0}'::svec, '{1}:{NULL}'::svec),
       (14, '{1,2,1}:{2,4,2}'::svec, '{2,1,1}:{0,3,5}'::svec),
       (15, '{1,2,1}:{2,4,2}'::svec, '{2,1,1}:{NULL,3,5}'::svec);


select id, svec_count(a,b) from test_pairs order by id;
select id, svec_plus(a,b) from test_pairs order by id;
select id, svec_plus(a,b) = svec_plus(b,a) from test_pairs order by id;
select id, svec_mult(a,b) from test_pairs order by id;
select id, svec_mult(a,b) = svec_mult(b,a) from test_pairs order by id;
select id, svec_div(a,b) = svec_mult(a, svec_pow(b,-1)) from test_pairs order by id;
select id, svec_minus(a, b) = svec_plus(svec_mult(-1,b), a) from test_pairs order by id;
select id, svec_pow(a,2) = svec_mult(a,a) from test_pairs order by id;

select id, svec_count(a,b::float8[]) from test_pairs order by id;
select id, svec_plus(a,b::float8[]) from test_pairs order by id;
select id, svec_plus(a,b::float8[]) = svec_plus(b,a::float8[]) from test_pairs order by id;
select id, svec_mult(a,b::float8[]) from test_pairs order by id;
select id, svec_mult(a,b::float8[]) = svec_mult(b,a::float8[]) from test_pairs order by id;
select id, svec_div(a,b::float8[]) = svec_mult(a, svec_pow(b,-1)::float8[]) from test_pairs order by id;
select id, svec_minus(a, b::float8[]) = svec_plus(svec_mult(-1,b), a::float8[]) from test_pairs order by id;
select id, svec_pow(a,2) = svec_mult(a,a::float8[]) from test_pairs order by id;

select id, svec_count(a::float8[],b) from test_pairs order by id;
select id, svec_plus(a::float8[],b) from test_pairs order by id;
select id, svec_plus(a::float8[],b) = svec_plus(b::float8[],a) from test_pairs order by id;
select id, svec_mult(a::float8[],b) from test_pairs order by id;
select id, svec_mult(a::float8[],b) = svec_mult(b::float8[],a) from test_pairs order by id;
select id, svec_div(a::float8[],b) = svec_mult(a::float8[], svec_pow(b,-1)) from test_pairs order by id;
select id, svec_minus(a::float8[], b) = svec_plus(svec_mult(-1,b)::float8[], a) from test_pairs order by id;
select id, svec_pow(a::float8[],2) = svec_mult(a::float8[],a) from test_pairs order by id;

select id, dot(a,b) from test_pairs order by id;
select id, dot(a,b) = dot(b,a) from test_pairs order by id;
select id, dot(a,b::float8[]) = dot(b,a::float8[]) from test_pairs order by id;
select id, dot(a::float8[],b) = dot(b::float8[],a) from test_pairs order by id;
select id, dot(a::float8[],b::float8[]) = dot(b::float8[],a::float8[]) from test_pairs order by id;

select id, l2norm(a), l2norm(a::float[]), l2norm(b), l2norm(b::float8[]) from test_pairs order by id;
select id, l1norm(a), l1norm(a::float[]), l1norm(b), l1norm(b::float8[]) from test_pairs order by id;

select svec_plus('{1,2,3}:{4,5,6}', 5);
select svec_plus(5, '{1,2,3}:{4,5,6}');
select svec_plus(500, '{1,2,3}:{4,null,6}');
select svec_div(500, '{1,2,3}:{4,null,6}');
select svec_div('{1,2,3}:{4,null,6}', 500);

-- Test operators between svec and float8[]
select ('{1,2,3,4}:{3,4,5,6}'::svec)           %*% ('{1,2,3,4}:{3,4,5,6}'::svec)::float8[];
select ('{1,2,3,4}:{3,4,5,6}'::svec)::float8[] %*% ('{1,2,3,4}:{3,4,5,6}'::svec);
select ('{1,2,3,4}:{3,4,5,6}'::svec)            /  ('{1,2,3,4}:{3,4,5,6}'::svec)::float8[];
select ('{1,2,3,4}:{3,4,5,6}'::svec)::float8[]  /  ('{1,2,3,4}:{3,4,5,6}'::svec);
select ('{1,2,3,4}:{3,4,5,6}'::svec)            *  ('{1,2,3,4}:{3,4,5,6}'::svec)::float8[];
select ('{1,2,3,4}:{3,4,5,6}'::svec)::float8[]  *  ('{1,2,3,4}:{3,4,5,6}'::svec);
select ('{1,2,3,4}:{3,4,5,6}'::svec)            +  ('{1,2,3,4}:{3,4,5,6}'::svec)::float8[];
select ('{1,2,3,4}:{3,4,5,6}'::svec)::float8[]  +  ('{1,2,3,4}:{3,4,5,6}'::svec);
select ('{1,2,3,4}:{3,4,5,6}'::svec)            -  ('{1,2,3,4}:{3,4,5,6}'::svec)::float8[];
select ('{1,2,3,4}:{3,4,5,6}'::svec)::float8[]  -  ('{1,2,3,4}:{3,4,5,6}'::svec);

-- these should produce error messages 
select '{10000000000000000000}:{1}'::svec ;
select '{1,null,2}:{2,3,4}'::svec;
select svec_count('{1,1,1}:{3,4,5}', '{2,2}:{1,3}');
select svec_plus('{1,1,1}:{3,4,5}', '{2,2}:{1,3}');
select svec_minus('{1,1,1}:{3,4,5}', '{2,2}:{1,3}');
select svec_mult('{1,1,1}:{3,4,5}', '{2,2}:{1,3}');
select svec_div('{1,1,1}:{3,4,5}', '{2,2}:{1,3}');

select unnest('{1}:{5}'::svec);
select unnest('{1,2,3,4}:{5,6,7,8}'::svec);
select unnest('{1,2,3,4}:{5,6,null,8}'::svec);
select id, unnest(a),a from test_pairs where id >= 10 order by id;

select vec_pivot('{1}:{5}', 2);
select vec_pivot('{1}:{5}', 5);
select vec_pivot('{1}:{5}', null);
select vec_pivot('{1}:{null}', 5);
select vec_pivot('{1}:{null}', null);
select vec_pivot('{1,2,3,4}:{5,6,7,8}'::svec, 2);
select id, vec_pivot(a, 5), a, vec_pivot(a,6), a, vec_pivot(a,2) from test_pairs order by id;
select id, vec_pivot(b, 5), b, vec_pivot(b,6), b, vec_pivot(b,null) from test_pairs order by id;

select vec_sum('{1}:{-5}'::svec);
select id, a, vec_sum(a), a, b, vec_sum(b), b from test_pairs order by id;
select id, vec_sum(a) = vec_sum(a::float8[]) from test_pairs order by id;
select id, vec_sum(b) = vec_sum(b::float8[]) from test_pairs order by id;

select id, a, vec_median(a), a from test_pairs order by id;
select id, b, vec_median(b), b from test_pairs order by id;
select id, vec_median(a) = vec_median(a::float8[]),
           vec_median(b) = vec_median(b::float8[]) from test_pairs order by id;

select id, a, b, svec_concat(a,b), a, b from test_pairs order by id;

select id, svec_concat_replicate(0, b), b from test_pairs order by id;
select id, svec_concat_replicate(1, b) = b from test_pairs order by id;
select id, svec_concat_replicate(3, b), b from test_pairs order by id;
select id, svec_concat_replicate(-2, b), b from test_pairs order by id;

select id, dimension(a), a, dimension(b), b from test_pairs order by id;

select svec_lapply('sqrt', null); 
select id, svec_lapply('sqrt', svec_lapply('abs', a)), a from test_pairs order by id;
select id, svec_lapply('sqrt', svec_lapply('abs', b)), b from test_pairs order by id;

select svec_append(null, 220, 20); -- FIX ME
select id, svec_append(a, 50, 100), a, svec_append(b, null, 50), b from test_pairs order by id;

select svec_proj(a,1), a, svec_proj(b,1), b from test_pairs order by id;
select svec_proj(a,2), a, svec_proj(b,2), b from test_pairs order by id; -- FIX ME

drop table if exists test;
create table test (a int, b svec) DISTRIBUTED BY (a);

insert into test (select 1,gp_extract_feature_histogram('{"one","two","three","four","five","six"}','{"twe","four","five","six","one","three","two","one"}'));
insert into test (select 2,gp_extract_feature_histogram('{"one","two","three","four","five","six"}','{"the","brown","cat","ran","across","three","dogs"}'));
insert into test (select 3,gp_extract_feature_histogram('{"one","two","three","four","five","six"}','{"two","four","five","six","one","three","two","one"}'));

-- Test the equals operator (should be only 3 rows)
select a,b::float8[] cross_product_equals from (select a,b from test) foo where b = foo.b order by a;

drop table if exists test2;
create table test2 as select * from test DISTRIBUTED BY (a);
-- Test the plus operator (should be 9 rows)
select (t1.b+t2.b)::float8[] cross_product_sum from test t1, test2 t2 order by t1.a;

-- Test ORDER BY
select (t1.b+t2.b)::float8[] cross_product_sum, l2norm(t1.b+t2.b) l2norm, (t1.b+t2.b) sparse_vector from test t1, test2 t2 order by 3;

 select (sum(t1.b))::float8[] as features_sum from test t1;
-- Test the div operator
 select (t1.b/(select sum(b) from test))::float8[] as weights from test t1 order by a;
-- Test the * operator
 select t1.b %*% (t1.b/(select sum(b) from test)) as raw_score from test t1 order by a;
-- Test the * and l2norm operators
 select (t1.b %*% (t1.b/(select sum(b) from test))) / (l2norm(t1.b) * l2norm((select sum(b) from test))) as norm_score from test t1 order by a;
-- Test the ^ and l1norm operators
select ('{1,2}:{20.,10.}'::svec)^('{1}:{3.}'::svec);
 select (t1.b %*% (t1.b/(select sum(b) from test))) / (l1norm(t1.b) * l1norm((select sum(b) from test))) as norm_score from test t1 order by a;

-- Test the multi-concatenation and show sizes compared with a normal array
drop table if exists corpus_proj;
drop table if exists corpus_proj_array;
create table corpus_proj as (select 10000 *|| ('{45,2,35,4,15,1}:{0,1,0,1,0,2}'::svec) result ) distributed randomly;
create table corpus_proj_array as (select result::float8[] from corpus_proj) distributed randomly;
-- Calculate on-disk size of sparse vector
select pg_size_pretty(pg_total_relation_size('corpus_proj'));
-- Calculate on-disk size of normal array
select pg_size_pretty(pg_total_relation_size('corpus_proj_array'));
\timing
-- Calculate L1 norm from sparse vector
select l1norm(result) from corpus_proj;
-- Calculate L1 norm from float8[]
select l1norm(result) from corpus_proj_array;
-- Calculate L2 norm from sparse vector
select l2norm(result) from corpus_proj;
-- Calculate L2 norm from float8[]
select l2norm(result) from corpus_proj_array;


drop table corpus_proj;
drop table corpus_proj_array;
drop table test;
drop table test2;


-- Test the pivot operator in the presence of NULL values
drop table if exists pivot_test;
create table pivot_test(a float8) distributed randomly;
insert into pivot_test values (0),(1),(NULL),(2),(3);
select array_agg(a) from pivot_test;
select l1norm(array_agg(a)) from pivot_test;
drop table if exists pivot_test;
-- Answer should be 5
select vec_median(array_agg(a)) from (select generate_series(1,9) a) foo;
-- Answer should be a 10-wide vector
select array_agg(a) from (select trunc(random()*10) a,generate_series(1,100000) order by a) foo;
-- Average is 4.50034, median is 5
select vec_median('{9960,9926,10053,9993,10080,10050,9938,9941,10030,10029}:{1,9,8,7,6,5,4,3,2,0}'::svec);
select vec_median('{9960,9926,10053,9993,10080,10050,9938,9941,10030,10029}:{1,9,8,7,6,5,4,3,2,0}'::svec::float8[]);
