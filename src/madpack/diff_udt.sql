SET client_min_messages to ERROR;

CREATE OR REPLACE FUNCTION filter_schema(argstr text, schema_name text)
RETURNS text AS $$
    if argstr is None:
        return "NULL"
    return argstr.replace(schema_name + ".", '')
$$ LANGUAGE plpythonu;

CREATE OR REPLACE FUNCTION get_types(schema_name text)
RETURNS VOID AS
$$
    import plpy
    plpy.execute("""
CREATE TABLE types_{schema_name} AS
SELECT n.nspname as "schema",
       filter_schema(pg_catalog.format_type(t.oid, NULL), '{schema_name}') AS "name"
FROM pg_catalog.pg_type t
     LEFT JOIN pg_catalog.pg_namespace n ON n.oid = t.typnamespace
WHERE (t.typrelid = 0 OR
       (SELECT c.relkind = 'c' FROM pg_catalog.pg_class c WHERE c.oid = t.typrelid))
  AND t.typname !~ '^_'
  AND n.nspname ~ '^({schema_name})$'
ORDER BY 1, 2;
    """.format(schema_name=schema_name))
$$ LANGUAGE plpythonu;

DROP TABLE IF EXISTS types_madlib;
DROP TABLE IF EXISTS types_madlib_v141;
SELECT get_types('madlib');
SELECT get_types('madlib_v141');

SELECT * FROM types_madlib;
SELECT * FROM types_madlib_v141;

SELECT
    v141.name
FROM
    types_madlib_v141 AS v141
    LEFT JOIN
    types_madlib AS v15
    USING (name)
WHERE v15.name IS NULL;
