/* ----------------------------------------------------------------------- *//**
 *
 * @file Math.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MATH_HPP
#define MADLIB_MATH_HPP

#include <boost/utility/enable_if.hpp>
#include <boost/math/special_functions/fpclassify.hpp>

namespace madlib {

namespace utils {

/**
 * @brief Return the smallest power of 2 that is greater or equal to a number
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
        // Note: No overflow can occur in the following line
        inValue = static_cast<T>(inValue | (inValue >> i));

    return static_cast<T>(inValue + 1);
}

/**
 * @brief Return if two floating point numbers are almost equal.
 *
 * If either of \c inX or \c inY is infinity, the other value has to be infinity
 * as well in order to be considered equal (irrespective of the units in the
 * last place).
 */
template<class T>
typename boost::enable_if_c<!std::numeric_limits<T>::is_integer, bool>::type
almostEqual(T inX, T inY, int inULP) {
    // The machine epsilon has to be scaled to the magnitude of the larger value
    // and multiplied by the desired precision in ULPs (units in the last place)
    return (
           std::fabs(inX - inY)
        <= std::numeric_limits<T>::epsilon()
            * std::max(std::fabs(inX), std::fabs(inY))
            * inULP
        ) || (
            boost::math::isinf(inX) && boost::math::isinf(inY)
            && ((inX < 0) == (inY < 0))
        );
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
