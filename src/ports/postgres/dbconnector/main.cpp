/* ----------------------------------------------------------------------- *//**
 *
 * @file main.cpp
 *
 * @brief Main file containing the entrance points into the C++ modules
 *
 * -------------------------------------------------------------------------- */

#include "dbconnector.hpp"

extern "C" {
    PG_MODULE_MAGIC;
} // extern "C"

// Include declarations declarations
#include <modules/declarations.hpp>

// Now export the symbols
#undef DECLARE_UDF
#define DECLARE_UDF DECLARE_UDF_EXTERNAL
#include <modules/declarations.hpp>
