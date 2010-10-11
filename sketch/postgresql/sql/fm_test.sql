\i sketches.sql
select fmcount(T.i)
  from generate_series(1,100) AS R(i),
       generate_series(1,10000,10) AS T(i);

select fmcount(CAST('2010-10-10' As date) + CAST((T.i || ' days') As interval))
  from generate_series(1,100) AS R(i),
       generate_series(1,10000,10) AS T(i);

select fmcount(T.i::float)
  from generate_series(1,100) AS R(i),
       generate_series(1,10000,10) AS T(i);

select fmcount(T.i::text)
  from generate_series(1,100) AS R(i),
       generate_series(1,10000,10) AS T(i);


