template <int ErrorLevel>
class AbstractionLayer::OutputStream
  : public dbal::OutputStreamBase<OutputStream<ErrorLevel>, char> {

public:
    void output(char *inMsg, uint32_t inLength) const;
};
