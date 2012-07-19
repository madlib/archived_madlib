/* ----------------------------------------------------------------------- *//**
 *
 * @file NativeRandomNumberGenerator_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_NATIVERANDOMNUMBERGENERATOR_IMPL_HPP
#define MADLIB_POSTGRES_NATIVERANDOMNUMBERGENERATOR_IMPL_HPP

namespace madlib {

namespace dbconnector {

namespace postgres {

inline
NativeRandomNumberGenerator::NativeRandomNumberGenerator() { }

/**
 * @brief Sets the current state of the engine
 *
 * All this function does is calling the backend's seed functions.
 */
inline
void
NativeRandomNumberGenerator::seed(result_type inSeed) {
    DirectFunctionCall1(setseed, Float8GetDatum(inSeed));
}

/**
 * @brief Advances the engine's state and returns the generated value
 */
inline
NativeRandomNumberGenerator::result_type
NativeRandomNumberGenerator::operator()() {
    // There seems to be no DirectFunctionCall0().
    return DatumGetFloat8(DirectFunctionCall1(drandom, Datum(0)));
}

/**
 * @brief Return tight lower bound on the set of all values returned by
 *     <tt>operator()</tt>
 */
inline
NativeRandomNumberGenerator::result_type
NativeRandomNumberGenerator::min() {
    return 0.0;
}

/**
 * @brief Return smallest representable number larger than maximum of all values
 *     returned by <tt>operator()</tt>
 *
 * Boost specifies, if \c result_type is not integer, then return "the smallest
 * representable number larger than the tight upper bound on the set of all
 * values returned by operator(). In any case, the return value of this function
 * shall not change during the lifetime of the object."
 * http://www.boost.org/doc/libs/1_50_0/doc/html/boost_random/reference.html
 */
inline
NativeRandomNumberGenerator::result_type
NativeRandomNumberGenerator::max() {
    return 1.0;
}

} // namespace postgres

} // namespace dbconnector

} // namespace madlib

#endif // defined(MADLIB_POSTGRES_NATIVERANDOMNUMBERGENERATOR_IMPL_HPP)
