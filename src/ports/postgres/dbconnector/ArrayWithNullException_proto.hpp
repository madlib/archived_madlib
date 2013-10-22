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
      : std::runtime_error("Error converting an array w/ NULL values to dense format."), array_size_(0) { }

    explicit
    ArrayWithNullException(size_t array_size)
      : std::runtime_error("Error converting an array w/ NULL values to dense format."), array_size_(array_size) { }

    size_t getArraySize() const { return array_size_; }

protected:
    const size_t array_size_;
};

} // namespace postgres

} // namespace dbconnector

} // namespace madlib

#endif // defined(MADLIB_POSTGRES_ARRAYWITHNULLEXCEPTION_PROTO_HPP)
