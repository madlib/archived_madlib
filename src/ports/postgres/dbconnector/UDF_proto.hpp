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
class UDF : public Allocator {
public:
    typedef AnyType (*Pointer)(FunctionCallInfo, AnyType&);

    UDF(FunctionCallInfo inFCInfo) : Allocator(inFCInfo),
        dbout(&mOutStreamBuffer), dberr(&mErrStreamBuffer) { }

    template <class Function>
    static Datum call(FunctionCallInfo fcinfo);

    template <class Function>
    static AnyType invoke(FunctionCallInfo fcinfo, AnyType& args);

    template <class Function>
    static Datum SRF_invoke(FunctionCallInfo fcinfo);

protected:
    template <class Function>
    static FuncCallContext* SRF_percall_setup(FunctionCallInfo fcinfo);

    template <class Function>
    static bool SRF_is_firstcall(FunctionCallInfo fcinfo);

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
