/* ----------------------------------------------------------------------- *//**
 *
 * @file Math.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MATH_HPP
#define MADLIB_MATH_HPP

#include <boost/utility/enable_if.hpp>

namespace madlib {

namespace utils {

/**
 * @brief Return the next higher power of 2.
 *
 * This implementation is for integers only.
 */
template <class T>
typename boost::enable_if_c<std::numeric_limits<T>::is_integer, T>::type
nextPowerOfTwo(T inValue) {
    if (std::numeric_limits<T>::is_signed && inValue == 0)
        return 1;

    inValue--;
    for (int i = 1; i < std::numeric_limits<T>::digits; i <<= 1)
        inValue = inValue | (inValue >> i);
    return inValue + 1;
}

/**
 * @brief Return if two floating point numbers are almost equal.
 */
template<class T>
typename boost::enable_if_c<!std::numeric_limits<T>::is_integer, bool>::type
almostEqual(T inX, T inY, int inULP) {
    // The machine epsilon has to be scaled to the magnitude of the larger value
    // and multiplied by the desired precision in ULPs (units in the last place)
    return std::fabs(inX - inY)
        <= std::numeric_limits<T>::epsilon()
            * std::max(std::fabs(inX), std::fabs(inY))
            * inULP;
}

/**
 * @brief Test if value is negative
 *
 * This is the implementation for unsigned T. We use SFINAE because
 * otherwise the compiler would warn that a comparison < 0 is always false
 * for unsigned types.
 */
template <class T>
inline
typename boost::enable_if_c<
    std::numeric_limits<T>::is_integer && !std::numeric_limits<T>::is_signed,
    bool
>::type
isNegative(const T&) {
    return false;
}

template <class T>
inline
typename boost::enable_if_c<
    !std::numeric_limits<T>::is_integer || std::numeric_limits<T>::is_signed,
    bool
>::type
isNegative(const T& inValue) {
    return inValue < static_cast<T>(0);
}

} // namespace utils

} // namespace regress

#endif
