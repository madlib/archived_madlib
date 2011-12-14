/* ----------------------------------------------------------------------- *//**
 *
 * @file OutputStreamBuffer_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

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
