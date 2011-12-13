/* ----------------------------------------------------------------------- *//**
 *
 * @file OutputStream_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Stream buffer that dispatches all output to PostgreSQL's ereport
 *        function
 */
template <int ErrorLevel>
class AbstractionLayer::OutputStream
  : public dbal::OutputStreamBase<OutputStream<ErrorLevel>, char> {

public:
    void output(char *inMsg, uint32_t inLength) const;
};
