/* ----------------------------------------------------------------------- *//**
 *
 * @file UDF_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief User-defined function
 */
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
    /**
     * @brief Informational output stream
     */
    std::ostream dbout;
    
    /**
     * @brief Warning and non-fatal error output stream
     */
    std::ostream dberr;
};
