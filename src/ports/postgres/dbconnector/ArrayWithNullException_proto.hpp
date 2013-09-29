/* ----------------------------------------------------------------------- *//**
 *
 * @file ArrayWithNullException_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_ARRAYWITHNULLEXCEPTION_PROTO_HPP
#define MADLIB_POSTGRES_ARRAYWITHNULLEXCEPTION_PROTO_HPP

namespace madlib {

namespace dbconnector {

namespace postgres {

/**
 * @brief Unspecified PostgreSQL backend expcetion
 */
class ArrayWithNullException : public std::runtime_error {
public:
    explicit
    ArrayWithNullException()
      : std::runtime_error("Error converting an array w/ NULL values to dense format.") { }
};

} // namespace postgres

} // namespace dbconnector

} // namespace madlib

#endif // defined(MADLIB_POSTGRES_ARRAYWITHNULLEXCEPTION_PROTO_HPP)
