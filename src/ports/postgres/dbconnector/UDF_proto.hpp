/* ----------------------------------------------------------------------- *//**
 *
 * @file UDF_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_UDF_PROTO_HPP
#define MADLIB_POSTGRES_UDF_PROTO_HPP

namespace madlib {

namespace dbconnector {

namespace postgres {

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
    OutputStreamBuffer<INFO> mOutStreamBuffer;
    OutputStreamBuffer<WARNING> mErrStreamBuffer;

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

} // namespace postgres

} // namespace dbconnector

} // namespace madlib

#endif // defined(MADLIB_POSTGRES_UDF_PROTO_HPP)
