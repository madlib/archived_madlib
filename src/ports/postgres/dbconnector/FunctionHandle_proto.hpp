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

    UDF::Pointer funcPtr();
    Oid funcID() const;
    FunctionHandle& setFunctionCallOptions(uint32_t inFlags);
    FunctionHandle& unsetFunctionCallOptions(uint32_t inFlags);
    uint32_t getFunctionCallOptions() const;
    AnyType invoke(AnyType& args);

    AnyType operator()();

protected:
    /**
     * @brief Trivial (empty) subclass of AnyType that enforces lazy conversion
     *     to PostgreSQL Datums
     *
     * The purpose to use arguments of type FunctionArgument instead of type
     * AnyType is to enforce a "lazy" conversion to PostgreSQL Datums. This is
     * solely for performance reasons: When a C++ function is called via a
     * FunctionHandle, then it is desirable to keep the reference instead of
     * temporarily converting to a backend representation.
     */
    class Argument : public AnyType {
    public:
        Argument(AnyType inValue);
        template <typename T> Argument(const T& inValue);
    };

public:
#define MADLIB_OPERATOR_DECL(z, n, _ignored) \
    AnyType operator()( \
        BOOST_PP_ENUM_PARAMS_Z(z, BOOST_PP_INC(n), Argument inArg) \
    );
    BOOST_PP_REPEAT(MADLIB_FUNC_MAX_ARGS, MADLIB_OPERATOR_DECL, 0 /* ignored */)
#undef MADLIB_OPERATOR_DECL

protected:
    template <typename T>
    friend struct TypeTraits;

    Datum internalInvoke(FunctionCallInfo inFCInfo);
    SystemInformation* getSysInfo() const;

    SystemInformation* mSysInfo;
    FunctionInformation* mFuncInfo;
    uint32_t mFuncCallOptions;
};

} // namespace postgres

} // namespace dbconnector

} // namespace madlib

#endif // defined(MADLIB_POSTGRES_FUNCTIONHANDLE_PROTO_HPP)
