/* ----------------------------------------------------------------------- *//**
 *
 * @file PGArmadilloGlue.cpp
 *
 * Linux' flat namespace and the way some DBMSs link to LAPACK do not play well
 * together. Since the core library is independent of DBMSs, this glue code is
 * necessary. See ports/greenplum/linux/dbconnector/GPArmadilloGlue.cpp for more
 * information.
 *
 *//* ----------------------------------------------------------------------- */

#include <armadillo>

namespace madlib {

namespace dbconnector {

using namespace arma;

extern "C" {

#undef arma_fortran



#define MADLIB_FORTRAN_QUALIFIER

#define arma_fortran(function) arma_fortran2(function)

#define MADLIB_FORTRAN_DECLARE(x) ;

#define MADLIB_FORTRAN

#define MADLIB_FORTRAN_RETURN

#include <dbconnector/ArmadilloDeclarations.hpp>

#undef MADLIB_FORTRAN_RETURN

#undef MADLIB_FORTRAN

#undef MADLIB_FORTRAN_DECLARE

#undef arma_fortran



#define arma_fortran(function) madlib_##function

#define MADLIB_FORTRAN_DECLARE(x) { x; }

#define MADLIB_FORTRAN(function) arma_fortran2(function)

#define MADLIB_FORTRAN_RETURN(function) return arma_fortran2(function)

#include <dbconnector/ArmadilloDeclarations.hpp>

} // extern "C"

} // namespace dbconnector

} // namespace madlib
