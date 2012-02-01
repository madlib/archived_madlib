/* ----------------------------------------------------------------------- *//**
 *
 * @file OutputStreamBuffer_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

// Workaround for Doxygen: Ignore if not included by dbconnector.hpp
#ifdef MADLIB_DBCONNECTOR_HPP

/**
 * @brief Stream buffer that dispatches all output to PostgreSQL's ereport
 *        function
 */
template <int ErrorLevel>
class AbstractionLayer::OutputStreamBuffer
  : public dbal::OutputStreamBufferBase<OutputStreamBuffer<ErrorLevel>, char> {

public:
    void output(char *inMsg, uint32_t inLength) const;
};

#endif // MADLIB_DBCONNECTOR_HPP (workaround for Doxygen)
