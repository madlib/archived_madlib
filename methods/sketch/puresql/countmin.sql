-- Naive CountMin Sketch
-- We generate a family of k hashes by adding 2^i to the original hash for k values of i.
-- Cormode says that d<10 rows is typical.  http://dimacs.rutgers.edu/~graham/pubs/papers/cmencyc.pdf
-- We'll try d=8, with 2^16-1=65535 as our number of columns

-- DROP FUNCTION IF EXISTS sql_bytea_to_numeric(bytea) cascade;
-- CREATE FUNCTION sql_bytea_to_numeric(bitmap bytea) RETURNS numeric AS
-- $$
-- DECLARE
--   b numeric := 0;
--   i integer;
--   len CONSTANT integer := length(bitmap);
-- BEGIN
--   -- IF len > 8 THEN
--   --   RAISE NOTICE '%-byte bitmap too wide for int8.  Returning 0.', len;
--   -- END IF;
--   FOR i in 0..(len-1) LOOP
--     b = b + get_byte(bitmap, (len -1) - i) * pow(256, i)::numeric;
--   END LOOP;
-- 
--   RETURN b;
-- END;
-- $$
-- LANGUAGE PLPGSQL;

-- DROP FUNCTION IF EXISTS sql_bytea_add_pow2(bytea, integer) cascade;
-- CREATE FUNCTION sql_bytea_add_pow2(bitmap bytea, p integer) RETURNS bytea AS
-- $$
-- DECLARE
--   i integer;
--   bytelen CONSTANT integer := length(bitmap);
--   len CONSTANT integer := bytelen*8;
--   tmpmap bytea := bitmap;
--   funnyi integer;
-- BEGIN
--   IF p > len-1 THEN
--     RAISE NOTICE '%-byte bitmap not wide enough to add 2^%.  Returning unchanged.', bytelen, p;
--   RETURN (bitmap);
--   END IF;
--   FOR i in p..(len-1) LOOP
--   -- Postgres set_bit/get_bit is nuts.  It's left-to-right on bytes, but right-to-left on bits.
--   -- So get_bit(b, 8) returns the 16th bit from the left (lowest bit of 2nd byte from left).
--   -- funnyi turns this around so all bit offsets are left-to-right.  Then we can subtract from len.
--     funnyi := ((i/8)*8 + (7-(i%8)));
--     IF (get_bit(tmpmap, (len - 1) - funnyi) = 0) THEN
--       RETURN set_bit(tmpmap, (len - 1) - funnyi, 1);
--     ELSE
--       tmpmap := set_bit(tmpmap, (len - 1) - funnyi, 0);
--     END IF;
--   END LOOP;
-- END;
-- $$
-- LANGUAGE PLPGSQL;

DROP FUNCTION IF EXISTS sql_countmin_offset(bytea, integer) cascade;
CREATE FUNCTION sql_countmin_offset(c bytea, power integer) RETURNS integer AS
$$
DECLARE
  run bytea = substring(c from 2*power for 4);
  r RECORD;
--  c bytea := digest(val::text,'SHA1');
BEGIN
  FOR r IN EXECUTE 'SELECT x'''||run||'''::integer AS hex' LOOP
      RETURN r.hex;
  END LOOP;
END;
$$
LANGUAGE PLPGSQL;

DROP FUNCTION IF EXISTS sql_countmin_iter(integer[][], anyelement) cascade;
CREATE FUNCTION sql_countmin_iter(counters integer[][], input anyelement) RETURNS integer[][] AS
$$
DECLARE
  d CONSTANT integer := 8;
  w CONSTANT integer := 65535;
  h bytea;
  n integer;
  i integer;
  j integer;
  ctrcpy integer[][];
  row integer[] := '{}';
  c bytea = md5(input::text);
BEGIN
--  RAISE NOTICE 'bitmaps is % long', length(bitmaps);
  -- run SHA-1 on string.
  IF counters IS NULL THEN
    FOR j in 1..w LOOP
      row := row || 0;
    END LOOP;
    ctrcpy := ARRAY[row];
    RAISE NOTICE 'array_dims(ctrcpy) = %', array_dims(ctrcpy);
    FOR i in 1..(d-1) LOOP
      ctrcpy := ctrcpy || ARRAY[row];
      RAISE NOTICE 'array_dims(ctrcpy) = %', array_dims(ctrcpy);
    END LOOP;
  ELSE
    ctrcpy := counters;
  END IF;
  FOR i in 1..d LOOP
    n := sql_countmin_offset(c, i) % w + 1;
--    RAISE NOTICE 'looking up ctrcpy[%],[%]', i, n;
--    RAISE NOTICE 'ctrcpy: %', ctrcpy;
    ctrcpy[i][n] := ctrcpy[i][n]+1;
  END LOOP;
  return ctrcpy;
END;
$$
LANGUAGE PLPGSQL;

DROP FUNCTION IF EXISTS sql_countmin_getcount(integer[][]) cascade;
CREATE FUNCTION sql_countmin_getcount(counters integer[][]) RETURNS integer AS
$$
DECLARE
  d CONSTANT integer := 8;
  i integer;
  n integer;
  mini integer := 0;
  w CONSTANT integer := 65535;
  val bytea := md5('Joe Hellerstein <hellerstein@cs.berkeley.edu>');
BEGIN
  FOR i in 1..d LOOP
    n := sql_countmin_offset(val, i) % w + 1;
    RAISE NOTICE 'counters[%][%] = %',i,n,counters[i][n];
    IF (mini = 0) OR (counters[i][n] < mini) THEN
      mini := counters[i][n];
    END IF;
  END LOOP;
  RETURN mini;
END;
$$ 
LANGUAGE PLPGSQL;

CREATE AGGREGATE sql_cmcount(anyelement)
(
    sfunc = sql_countmin_iter,
    stype = integer[][], 
    finalfunc = sql_countmin_getcount
);
