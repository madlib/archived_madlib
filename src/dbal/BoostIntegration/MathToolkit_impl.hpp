/* ----------------------------------------------------------------------- *//**
 *
 * @file MathToolkit_impl.hpp
 *
 * @brief User-defined error handling functions
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_DBAL_BOOST_INTEGRATION_MATH_TOOLKIT_IMPL_HPP
#define MADLIB_DBAL_BOOST_INTEGRATION_MATH_TOOLKIT_IMPL_HPP

#include <iomanip>

#include <boost/math/policies/error_handling.hpp>

namespace boost {

namespace math {

namespace policies {

/**
 * @brief Boost user-defined domain-error handling function
 *
 * This function is called by the domain_error<user_error> policy in case
 * function arguments (or parameters) are outside of the domain of the
 * probability function.
 *
 * Our policy is to let NaNs propagate. All other errors we handle by throwing
 * a std::domain_error containing the text supplied by boost.
 *
 * @param inFunction The name of the function that raised the error, this string
 *     contains one or more %1% format specifiers that should be replaced by the
 *     name of real type T, like float or double.
 * @param inMessage A message associated with the error, normally this contains
 *     a %1% format specifier that should be replaced with the value of value:
 *     however note that overflow and underflow messages do not contain this
 *     %1% specifier (since the value of value is immaterial in these cases).
 * @param inVal The value that caused the error: either an argument to the
 *     function if this is a domain or pole error, the tentative result if this
 *     is a denorm or evaluation error, or zero or infinity for underflow or
 *     overflow errors.
 */
template <class T>
inline
T
user_domain_error(const char*, const char* inMessage, const T& inVal) {
    if (std::isnan(inVal))
        return std::numeric_limits<double>::quiet_NaN();

    // The following line is taken from
    // http://www.boost.org/doc/libs/1_49_0/libs/math/doc/sf_and_dist/html/math_toolkit/policy/pol_tutorial/user_def_err_pol.html
    int prec = 2 + (std::numeric_limits<T>::digits * 30103UL) / 100000UL;

    std::string msg = (boost::format(inMessage)
            % boost::io::group(std::setprecision(prec), inVal)
        ).str();

    // Some Boost error messages contain a space before the punctuation mark,
    // which we will remove.
    std::string::iterator lastChar = msg.end() - 1;
    std::string::iterator secondLastChar = msg.end() - 2;
    if (std::ispunct(*lastChar) && std::isspace(*secondLastChar))
        msg.erase(secondLastChar, lastChar);

    throw std::domain_error(msg);
}

} // namespace policies

} // namespace math

} // namespace boost

namespace madlib {

typedef boost::math::policies::policy<
    boost::math::policies::domain_error<boost::math::policies::user_error>,
    boost::math::policies::overflow_error<boost::math::policies::ignore_error>
> boost_mathkit_policy;

} // namespace madlib

#endif // !defined(MADLIB_DBAL_BOOST_INTEGRATION_MATH_TOOLKIT_IMPL_HPP)
