class UDF
  : public AbstractionLayer,
    public AbstractionLayer::Allocator {

public:
    UDF(FunctionCallInfo inFCInfo) : AbstractionLayer::Allocator(inFCInfo),
        dbout(&mOutStreamBuffer), dberr(&mErrStreamBuffer) { }

    template <class Function>
    static inline Datum call(FunctionCallInfo fcinfo);

private:
    OutputStream<INFO> mOutStreamBuffer;
    OutputStream<WARNING> mErrStreamBuffer;

protected:
    std::ostream dbout;
    std::ostream dberr;
};
