/* ----------------------------------------------------------------------- *//**
 *
 * @file PGException_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

// Workaround for Doxygen: Ignore if not included by dbconnector.hpp
#ifdef MADLIB_DBCONNECTOR_HPP

/**
 * @brief Unspecified PostgreSQL backend expcetion
 */
class AbstractionLayer::PGException
  : public std::runtime_error {

public:
    explicit 
    PGException()
      : std::runtime_error("The backend raised an exception.") { }
};

#endif // MADLIB_DBCONNECTOR_HPP (workaround for Doxygen)
