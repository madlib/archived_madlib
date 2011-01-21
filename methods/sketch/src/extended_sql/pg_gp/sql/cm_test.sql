set search_path to "$user",public,madlib;
-- Basic methods
select cmsketch_count(i,5) from generate_series(1,10000) as T(i);
select cmsketch_rangecount(i,1,1025) from generate_series(1,10000) as T(i);
select cmsketch_rangecount(i,1,200) from generate_series(1,10000) as R(i);
select cmsketch_width_histogram(cmsketch(i), min(i), max(i), 4) from generate_series(1,10000) as R(i);
select min(i),
       cmsketch_centile(i, 25) AS quartile1, 
       cmsketch_centile(i, 50) AS quartile2, 
       cmsketch_median(i) AS median,
       cmsketch_centile(i, 75) AS quartile3,
       max(i) 
  from generate_series(1,10000) as R(i);
select cmsketch_depth_histogram(i, 4) from generate_series(1,10000) as R(i);
-- tests for all-NULL column
select cmsketch(NULL) from generate_series(1,10000) as R(i) where i < 0;
select cmsketch_centile(NULL, 2) from generate_series(1,10000) as R(i) where i < 0;
