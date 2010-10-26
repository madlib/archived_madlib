\i sketches.sql
set search_path to "$user",public,madlib;
-- Basic methods
select cmsketch_getcount(cmsketch(i),5) from generate_series(1,10000) as T(i);
select cmsketch_rangecount(cmsketch(i),1,1025) from generate_series(1,10000) as T(i);
select cmsketch_rangecount(cmsketch(i), 1,200) from generate_series(1,10000) as R(i);
select cmsketch_width_histogram(cmsketch(i), min(i), max(i), 4) from generate_series(1,10000) as R(i);
select mini,
       cmsketch_centile(sketch, 25) AS quartile1, 
       cmsketch_centile(sketch, 50) AS quartile2, 
       cmsketch_centile(sketch, 75) AS quartile3,
       maxi 
  from (select min(i) as mini, cmsketch(i) as sketch, max(i) as maxi 
          from generate_series(1,10000) as R(i)) as T;
select cmsketch_depth_histogram(cmsketch(i), 4) from generate_series(1,10000) as R(i);
-- tests for all-NULL column
select cmsketch(NULL) from generate_series(1,10000) as R(i) where i < 0;
select cmsketch_centile(cmsketch(NULL), 2) from generate_series(1,10000) as R(i) where i < 0;
