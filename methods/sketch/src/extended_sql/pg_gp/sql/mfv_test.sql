create schema madlib;
\i sketches_drop.sql
\i sketches.sql
set search_path to "$user",public,madlib;

-- Basic methods
select frequent_values(i,5) 
  from (select * from generate_series(1,100) union all select * from generate_series(10,15)) as T(i);
select frequent_values(substring(name from '^.*/'), 9) from pg_timezone_names;
select frequent_values(NULL::bytea,5) from generate_series(1,100);