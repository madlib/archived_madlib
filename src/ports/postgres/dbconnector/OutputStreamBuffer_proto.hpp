/* ----------------------------------------------------------------------- *//**
 *
 * @file OutputStreamBuffer_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_OUTPUTSTREAMBUFFER_PROTO_HPP
#define MADLIB_POSTGRES_OUTPUTSTREAMBUFFER_PROTO_HPP

namespace madlib {

namespace dbconnector {

namespace postgres {

/**
 * @brief Stream buffer that dispatches all output to PostgreSQL's ereport
 *        function
 */
template <int ErrorLevel, template <class> class Allocator = std::allocator>
class OutputStreamBuffer
  : public dbal::OutputStreamBufferBase<
        OutputStreamBuffer<ErrorLevel, Allocator>, char, Allocator<char> > {

public:
    void output(char *inMsg, std::size_t inLength) const;
};

} // namespace postgres

} // namespace dbconnector

} // namespace madlib

#endif // defined(MADLIB_POSTGRES_OUTPUTSTREAMBUFFER_PROTO_HPP)
