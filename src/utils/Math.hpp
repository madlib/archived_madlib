/* ----------------------------------------------------------------------- *//**
 *
 * @file Math.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MATH_HPP
#define MADLIB_MATH_HPP

namespace madlib {

namespace utils {

/**
 * @brief Return the next higher power of 2.
 */
template <class T>
T nextPowerOfTwo(T inValue) {
    BOOST_STATIC_ASSERT_MSG(std::numeric_limits<T>::is_integer,
        "nextPowerOfTwo not implemented for non-integer types");

    if (std::numeric_limits<T>::is_signed && inValue == 0)
        return 1;
    
    inValue--;
    for (int i = 1; i < std::numeric_limits<T>::digits; i <<= 1)
        inValue = inValue | (inValue >> i);
    return inValue + 1;
}

} // namespace utils

} // namespace regress

#endif
