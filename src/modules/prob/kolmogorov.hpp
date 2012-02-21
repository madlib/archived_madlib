/* ----------------------------------------------------------------------- *//**
 *
 * @file kolmogorov.hpp
 *
 *//* ----------------------------------------------------------------------- */

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#if !defined(_DOXYGEN_IGNORE_HEADER_FILE)

/**
 * @brief Kolmogorov cumulative distribution function
 */
DECLARE_UDF(kolmogorov_cdf)

#if !defined(DECLARE_LIBRARY_EXPORTS)

double kolmogorovCDF(double t);

#endif // !defined(DECLARE_LIBRARY_EXPORTS)

#endif // workaround for Doxygen
