/* ----------------------------------------------------------------------- *//**
 *
 * @file dbconnector.hpp
 *
 * @brief This file should be included by user code (and nothing else)
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_GREENPLUM_DBCONNECTOR_HPP
#define MADLIB_GREENPLUM_DBCONNECTOR_HPP

// On platforms based on PostgreSQL we can include a different set of headers.
#define MADLIB_POSTGRES_HEADERS

extern "C" {
    #include <postgres.h>
    #include <funcapi.h>
    #include <catalog/pg_proc.h>
    #include <catalog/pg_type.h>
    #include <executor/executor.h> // Greenplum requires this for GetAttributeByNum()
    #include <miscadmin.h>         // Memory allocation, e.g., HOLD_INTERRUPTS
    #include <utils/acl.h>
    #include <utils/array.h>
    #include <utils/builtins.h>    // needed for format_procedure()
    #include <utils/datum.h>
    #include <utils/lsyscache.h>   // for type lookup, e.g., type_is_rowtype
    #include <utils/memutils.h>
    #include <utils/syscache.h>    // for direct access to catalog, e.g., SearchSysCache()
    #include <utils/typcache.h>    // type conversion, e.g., lookup_rowtype_tupdesc
    #include "../../../../methods/svec/src/pg_gp/sparse_vector.h" // Legacy sparse vectors
} // extern "C"

#include "Compatibility.hpp"

#include "../../postgres/dbconnector/dbconnector.hpp"

#endif // defined(MADLIB_GREENPLUM_DBCONNECTOR_HPP)
