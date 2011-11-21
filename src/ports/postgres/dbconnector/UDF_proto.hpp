class UDF
  : public AbstractionLayer,
    public AbstractionLayer::Allocator {

public:
    UDF(FunctionCallInfo inFCInfo) : AbstractionLayer::Allocator(inFCInfo) { }

    template <class Function>
    static inline Datum call(FunctionCallInfo fcinfo);
};
