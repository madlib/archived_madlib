/* ----------------------------------------------------------------------- *//**
 *
 * @file FunctionHandle_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_FUNCTIONHANDLE_PROTO_HPP
#define MADLIB_POSTGRES_FUNCTIONHANDLE_PROTO_HPP

namespace madlib {

namespace dbconnector {

namespace postgres {

struct SystemInformation;
struct FunctionInformation;

class FunctionHandle {
public:
    enum { isMutable = false };

    enum FunctionCallOption {
        GarbageCollectionAfterCall = 0x01
    };

    FunctionHandle(SystemInformation* inSysInfo, Oid inFuncID);

    Oid funcID() const;
    void setFunctionCallOptions(uint32_t inFlags);
    uint32_t getFunctionCallOptions() const;
    AnyType invoke(AnyType& args);

    AnyType operator()();
#define MADLIB_OPERATOR_DECL(z, n, _ignored) \
    AnyType operator()( \
        BOOST_PP_ENUM_PARAMS_Z(z, BOOST_PP_INC(n), AnyType inArg) \
    );
    BOOST_PP_REPEAT(MADLIB_FUNC_MAX_ARGS, MADLIB_OPERATOR_DECL, 0 /* ignored */)
#undef MADLIB_OPERATOR_DECL

protected:
    template <typename T, class>
    friend struct TypeTraits;

    SystemInformation* getSysInfo() const;

    SystemInformation* mSysInfo;
    FunctionInformation* mFuncInfo;
    uint32_t mFuncCallOptions;
};

} // namespace postgres

} // namespace dbconnector

} // namespace madlib

#endif // defined(MADLIB_POSTGRES_FUNCTIONHANDLE_PROTO_HPP)
