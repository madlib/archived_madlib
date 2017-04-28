-- KNOWN ISSUE: cannot detect cases when rettype or argument type have same
--   name but the content changes (e.g. add a field in composite type)

SET client_min_messages to ERROR;
\x on
CREATE OR REPLACE FUNCTION filter_schema(argstr text, schema_name text)
RETURNS text AS $$
    if argstr is None:
        return "NULL"
    return argstr.replace(schema_name + ".", '')
$$ LANGUAGE plpythonu;


CREATE OR REPLACE FUNCTION get_functions(schema_name text)
RETURNS VOID AS
$$
    import plpy
    plpy.execute("""
        CREATE TABLE functions_madlib_new_version AS
        SELECT
            "schema", "name", filter_schema("retype", 'madlib') retype,
            filter_schema("argtypes", 'madlib') argtypes, "type"
        FROM
        (

            SELECT n.nspname as "schema",
                p.proname as "name",
                CASE WHEN p.proretset THEN 'SETOF ' ELSE '' END ||
                pg_catalog.format_type(p.prorettype, NULL) as "retype",
                CASE WHEN proallargtypes IS NOT NULL THEN
                    pg_catalog.array_to_string(ARRAY(
                        SELECT
                            pio || ptyp
                        FROM
                        (
                            SELECT
                                CASE
                                  WHEN p.proargmodes[s.i] = 'i' THEN ''
                                  WHEN p.proargmodes[s.i] = 'o' THEN 'OUT '
                                  WHEN p.proargmodes[s.i] = 'b' THEN 'INOUT '
                                  WHEN p.proargmodes[s.i] = 'v' THEN 'VARIADIC '
                                END  AS pio,
                            --CASE
                            --  WHEN COALESCE(p.proargnames[s.i], '') = '' THEN ''
                            --  ELSE p.proargnames[s.i] || ' '
                            --END ||
                            pg_catalog.format_type(p.proallargtypes[s.i], NULL) AS ptyp
                          FROM
                            pg_catalog.generate_series(1, pg_catalog.array_upper(p.proallargtypes, 1)) AS s(i)
                        ) qx
                        WHERE pio = ''
                        ), ', ')
                ELSE
                    pg_catalog.array_to_string(ARRAY(
                      SELECT
                        --CASE
                        --  WHEN COALESCE(p.proargnames[s.i+1], '') = '' THEN ''
                        --  ELSE p.proargnames[s.i+1] || ' '
                        --  END ||
                        pg_catalog.format_type(p.proargtypes[s.i], NULL)
                      FROM
                        pg_catalog.generate_series(0, pg_catalog.array_upper(p.proargtypes, 1)) AS s(i)
                    ), ', ')
                END AS "argtypes",
                CASE
                  WHEN p.proisagg THEN 'agg'
                  WHEN p.prorettype = 'pg_catalog.trigger'::pg_catalog.regtype THEN 'trigger'
                  ELSE 'normal'
                END AS "type"
            FROM pg_catalog.pg_proc p
                 LEFT JOIN pg_catalog.pg_namespace n
                 ON n.oid = p.pronamespace
            WHERE n.nspname ~ '^(madlib)$'
            ORDER BY 1, 2, 4
        ) q
        """.format(schema_name=schema_name))
    plpy.execute("""
        CREATE TABLE functions_madlib_old_version AS
        SELECT
            "schema", "name", filter_schema("retype", '{schema_name}') retype,
            filter_schema("argtypes", '{schema_name}') argtypes, "type"
        FROM
        (

            SELECT n.nspname as "schema",
                p.proname as "name",
                CASE WHEN p.proretset THEN 'SETOF ' ELSE '' END ||
                pg_catalog.format_type(p.prorettype, NULL) as "retype",
                CASE WHEN proallargtypes IS NOT NULL THEN
                    pg_catalog.array_to_string(ARRAY(
                        SELECT
                            pio || ptyp
                        FROM
                        (
                            SELECT
                                CASE
                                  WHEN p.proargmodes[s.i] = 'i' THEN ''
                                  WHEN p.proargmodes[s.i] = 'o' THEN 'OUT '
                                  WHEN p.proargmodes[s.i] = 'b' THEN 'INOUT '
                                  WHEN p.proargmodes[s.i] = 'v' THEN 'VARIADIC '
                                END  AS pio,
                            --CASE
                            --  WHEN COALESCE(p.proargnames[s.i], '') = '' THEN ''
                            --  ELSE p.proargnames[s.i] || ' '
                            --END ||
                            pg_catalog.format_type(p.proallargtypes[s.i], NULL) AS ptyp
                          FROM
                            pg_catalog.generate_series(1, pg_catalog.array_upper(p.proallargtypes, 1)) AS s(i)
                        ) qx
                        WHERE pio = ''
                        ), ', ')
                ELSE
                    pg_catalog.array_to_string(ARRAY(
                      SELECT
                        --CASE
                        --  WHEN COALESCE(p.proargnames[s.i+1], '') = '' THEN ''
                        --  ELSE p.proargnames[s.i+1] || ' '
                        --  END ||
                        pg_catalog.format_type(p.proargtypes[s.i], NULL)
                      FROM
                        pg_catalog.generate_series(0, pg_catalog.array_upper(p.proargtypes, 1)) AS s(i)
                    ), ', ')
                END AS "argtypes",
                CASE
                  WHEN p.proisagg THEN 'agg'
                  WHEN p.prorettype = 'pg_catalog.trigger'::pg_catalog.regtype THEN 'trigger'
                  ELSE 'normal'
                END AS "type"
            FROM pg_catalog.pg_proc p
                 LEFT JOIN pg_catalog.pg_namespace n
                 ON n.oid = p.pronamespace
            WHERE n.nspname ~ '^({schema_name})$'
            ORDER BY 1, 2, 4
        ) q
        """.format(schema_name=schema_name))
$$ LANGUAGE plpythonu;


DROP TABLE IF EXISTS functions_madlib_old_version;
DROP TABLE IF EXISTS functions_madlib_new_version;

SELECT get_functions('madlib_old_vers');

SELECT
    --'\t-' || name || ':' || '\n\t\t-rettype: ' || retype || '\n\t\t-argument: ' || argtypes
    '    - ' || name || ':' || '\n        rettype: ' || retype || '\n        argument: ' || argtypes AS "Dropped UDFs"
    , type
FROM
(
SELECT
    old_version.name, old_version.retype, old_version.argtypes, old_version.type
FROM
    functions_madlib_old_version AS old_version
    LEFT JOIN
    functions_madlib_new_version AS new_version
    USING (name, retype, argtypes)
WHERE new_version.name IS NULL
) q
ORDER by type, "Dropped UDFs";

----------------------------------------

SELECT
    --'\t-' || name || ':' || '\n\t\t-rettype: ' || retype || '\n\t\t-argument: ' || argtypes
    '    - ' || name || ':' || '\n        rettype: ' || retype || '\n        argument: ' || argtypes AS "Changed UDFs"
--    , type
FROM
(
SELECT
    old_version.name, old_version.retype, old_version.argtypes, old_version.type
FROM
    functions_madlib_old_version AS old_version
    LEFT JOIN
    functions_madlib_new_version AS new_version
    USING (name, retype, argtypes)
-- WHERE FALSE
WHERE old_version.retype in ('') -- '__logregr_result', 'summary_result', 'linregr_result', 'mlogregr_result', 'marginal_logregr_result', 'marginal_mlogregr_result', 'intermediate_cox_prop_hazards_result', '__utils_scaled_data')
) q
ORDER by type, "Changed UDFs";

----------------------------------------

SELECT
    --'\t-' || name || ':' || '\n\t\t-rettype: ' || retype || '\n\t\t-argument: ' || argtypes
    '    - ' || name || ':' || '\n        rettype: ' || retype || '\n        argument: ' || argtypes AS "Suspected UDFs"
--    , type
FROM
(
SELECT
    old_version.name, old_version.retype, old_version.argtypes, old_version.type
FROM
    functions_madlib_old_version AS old_version
    LEFT JOIN
    functions_madlib_new_version AS new_version
    USING (name, retype, argtypes)
WHERE old_version.argtypes SIMILAR TO 'NOT-A-TYPE' -- '%(__logregr_result|summary_result|linregr_result|mlogregr_result|marginal_logregr_result|marginal_mlogregr_result|intermediate_cox_prop_hazards_result|__utils_scaled_data)%'
) q
ORDER by type, "Suspected UDFs";
