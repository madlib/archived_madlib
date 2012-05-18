/* ----------------------------------------------------------------------- *//**
 *
 * @file NoSolutionFoundException_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_DBAL_NOSOLUTIONFOUNDEXCEPTION_PROTO_HPP
#define MADLIB_DBAL_NOSOLUTIONFOUNDEXCEPTION_PROTO_HPP

namespace madlib {

namespace dbal {

/**
 * @brief Exception indicating that no solution was found, e.g., because
 *     convergence was not reached
 */
class NoSolutionFoundException
  : public std::runtime_error {

public:
    explicit 
    NoSolutionFoundException()
      : std::runtime_error("Could not find a solution.") { }
    
    explicit 
    NoSolutionFoundException(const std::string& inMsg)
      : std::runtime_error(inMsg) { }
};

} // namespace dbal

} // namespace madlib

#endif // defined(MADLIB_DBAL_NOSOLUTIONFOUNDEXCEPTION_PROTO_HPP)
