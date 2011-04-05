/* ----------------------------------------------------------------------- *//**
 *
 * @file postgres.cpp
 *
 * @brief PostgreSQL database abstraction layer 
 *
 *//* -------------------------------------------------------------------- *//**
 *
 * Postgres is a platform where the C interface supports reflection, so all we
 * need to do is to include the PostgreSQL database abstraction layer and the
 * default declarations.
 */

#include <madlib/ports/postgres/main.hpp>

#include <madlib/modules/declarations.hpp>

extern "C" {
    PG_MODULE_MAGIC;
} // extern "C"
