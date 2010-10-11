\i sketches.sql
-- Basic methods
-- select cmsketch(i) from generate_series(1,10000) as T(i);
select cmsketch_getcount(cmsketch(i),5) from generate_series(1,10000) as T(i);
select cmsketch_rangecount(cmsketch(i),1,1025) from generate_series(1,10000) as T(i);
select cmsketch_histogram(cmsketch(i), min(i), max(i), 4) from generate_series(1,10000) as R(i);
select min(i),
       cmsketch_centile(cmsketch(i), 25) AS quartile1, 
       cmsketch_centile(cmsketch(i), 50) AS quartile2, 
       cmsketch_centile(cmsketch(i), 75) AS quartile3,
       max(i) 
  from generate_series(1,10000) as R(i);
-- tests for all-NULL column
-- select cmsketch(NULL) from generate_series(1,10000) as R(i) where i < 0;
select cmsketch_rangecount(cmsketch(i), 1,200) from generate_series(1,10000) as R(i);
select cmsketch_centile(cmsketch(NULL), 2) from generate_series(1,10000) as R(i) where i < 0;
