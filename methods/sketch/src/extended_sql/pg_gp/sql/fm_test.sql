\i sketches.sql
set search_path to "$user",public,madlib;

-- tests for "little" tables using sortasorts
select fmcount(R.i)
  from generate_series(1,100) AS R(i),
       generate_series(1,3) AS T(i);

select fmcount(CAST('2010-10-10' As date) + CAST((R.i || ' days') As interval))
  from generate_series(1,100) AS R(i),
       generate_series(1,3) AS T(i);

select fmcount(R.i::float)
  from generate_series(1,100) AS R(i),
       generate_series(1,3) AS T(i);

select fmcount(R.i::text)
  from generate_series(1,100) AS R(i),
       generate_series(1,3) AS T(i);


-- tests for "big" tables
select fmcount(T.i)
  from generate_series(1,3) AS R(i),
       generate_series(1,20000) AS T(i);

select fmcount(CAST('2010-10-10' As date) + CAST((T.i || ' days') As interval))
  from generate_series(1,3) AS R(i),
       generate_series(1,20000) AS T(i);

select fmcount(T.i::float)
  from generate_series(1,3) AS R(i),
       generate_series(1,20000) AS T(i);

select fmcount(T.i::text)
  from generate_series(1,3) AS R(i),
       generate_series(1,20000) AS T(i);

-- tests for all-NULL column
select fmcount(NULL::integer) from generate_series(1,10000) as R(i);
