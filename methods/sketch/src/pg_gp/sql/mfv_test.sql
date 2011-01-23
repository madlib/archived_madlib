set search_path to "$user",public,madlib;

-- Basic methods
select mfvsketch_top_histogram(i,5) 
  from (select * from generate_series(1,100) union all select * from generate_series(10,15)) as T(i);
select mfvsketch_top_histogram(utc_offset,5) from pg_timezone_names;
select mfvsketch_top_histogram(NULL::bytea,5) from generate_series(1,100);