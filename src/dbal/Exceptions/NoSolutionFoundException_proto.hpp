/* ----------------------------------------------------------------------- *//**
 *
 * @file NoSolutionFoundException_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

// Workaround for Doxygen: Ignore if not included by dbconnector.hpp
#ifdef MADLIB_DBCONNECTOR_HPP

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

#endif // MADLIB_DBCONNECTOR_HPP (workaround for Doxygen)
