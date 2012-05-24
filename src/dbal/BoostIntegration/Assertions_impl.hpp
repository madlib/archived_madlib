/* ----------------------------------------------------------------------- *//**
 *
 * @file Assertions_impl.cpp
 *
 * @brief Boost assertion handler
 *
 * See the comments in BoostIntegration.hpp.
 *
 *//* ----------------------------------------------------------------------- */

#include <sstream>
#include <stdexcept>

#include <boost/assert.hpp>

namespace boost {

/**
 * @brief Boost failed assertion handler, with message
 *
 * @internal
 * If the macro \c BOOST_ENABLE_ASSERT_HANDLER is defined when
 * <tt><boost/assert.hpp></tt> is included, instead of sending a error
 * message to an output stream, the expression
 * <tt>::boost::assertion_failed_msg(#expr, BOOST_CURRENT_FUNCTION, __FILE__, __LINE__)</tt>
 * is evaluated.
 */
inline
void
assertion_failed_msg(char const *inExpr, char const *inMsg,
    char const *inFunction, char const *inFile, long inLine) {

    std::stringstream tmp;
    tmp << inMsg
        << "\nDetails (for developers): \n"
        "Failed BOOST_ASSERT: " << inExpr <<
        "\nFunction: " << inFunction <<
        "\nFile: " << inFile << ":" << inLine;

    throw std::runtime_error( tmp.str() );
}

/**
 * @brief Boost failed assertion handler, without message
 *
 * @internal
 * If the macro \c BOOST_ENABLE_ASSERT_HANDLER is defined when
 * <tt><boost/assert.hpp></tt> is included, <tt>BOOST_ASSERT(expr)</tt>
 * evaluates \c expr and, if the result is false, evaluates the expression
 * <tt>::boost::assertion_failed(#expr, BOOST_CURRENT_FUNCTION, __FILE__, __LINE__)</tt>
 */
inline
void
assertion_failed(char const *inExpr, char const *inFunction,
    char const *inFile, long inLine) {

    assertion_failed_msg("A run-time error occured.", inExpr, inFunction,
        inFile, inLine);
}

} // namespace boost
