/* ----------------------------------------------------------------------- *//**
 *
 * @file FunctionHandle_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_FUNCTIONHANDLE_IMPL_HPP
#define MADLIB_POSTGRES_FUNCTIONHANDLE_IMPL_HPP

namespace madlib {

namespace dbconnector {

namespace postgres {

inline
FunctionHandle::FunctionHandle(SystemInformation* inSysInfo, Oid inFuncID)
  : mSysInfo(inSysInfo),
    mFuncInfo(inSysInfo->functionInformation(inFuncID)),
    mFuncCallOptions(GarbageCollectionAfterCall) { }

/**
 * @brief Return the OID of this function
 */
inline
Oid
FunctionHandle::funcID() const {
    return mFuncInfo->oid;
}

inline
void
FunctionHandle::setFunctionCallOptions(uint32_t inFlags) {
    mFuncCallOptions = inFlags;
}

inline
uint32_t
FunctionHandle::getFunctionCallOptions() const {
    return mFuncCallOptions;
}

inline
AnyType
FunctionHandle::invoke(AnyType &args) {
    madlib_assert(args.isComposite(), std::logic_error(
        "FunctionHandle::invoke() called with simple type."));

    FunctionCallInfoData funcPtrCallInfo;
    // Initializes all the fields of a FunctionCallInfoData except for the arg[]
    // and argnull[] arrays
    madlib_InitFunctionCallInfoData(
        funcPtrCallInfo,

        // FmgrInfo *flinfo -- ptr to lookup info used for this call
        mFuncInfo->getFuncMgrInfo(),

        // short nargs -- # arguments actually passed
        args.numFields(),

        // Oid fncollation -- collation for function to use
        mSysInfo->collationOID,

        // fmNodePtr context -- pass info about context of call
        NULL,

        // fmNodePtr resultinfo -- pass or return extra info about result
        NULL
    );

    if (funcPtrCallInfo.nargs > mFuncInfo->nargs)
        throw std::invalid_argument(std::string("More arguments given than "
            "expected by '")
            + mSysInfo->functionInformation(mFuncInfo->oid)->getFullName()
            + "'.");

    bool hasNulls = false;
    for (uint16_t i = 0; i < funcPtrCallInfo.nargs; ++i) {
        funcPtrCallInfo.arg[i] = args[i].getAsDatum(&funcPtrCallInfo,
            mFuncInfo->getArgumentType(i));
        funcPtrCallInfo.argnull[i] = args[i].isNull();
        hasNulls |= funcPtrCallInfo.argnull[i];
    }

    // If function is strict, we must not call the function at all
    if (mFuncInfo->isstrict && hasNulls)
        return AnyType();

    MemoryContext oldContext = NULL;
    MemoryContext callContext = NULL;
    if (mFuncCallOptions | GarbageCollectionAfterCall) {
        // In order to do garbage collection, we need to create a new
        // memory context
        callContext = AllocSetContextCreate(CurrentMemoryContext,
            "C++ AL / FunctionHandle::invoke memory context",
            ALLOCSET_DEFAULT_MINSIZE,
            ALLOCSET_DEFAULT_INITSIZE,
            ALLOCSET_DEFAULT_MAXSIZE);
        oldContext = MemoryContextSwitchTo(callContext);
    }

    Datum result = 0;
    MADLIB_PG_TRY {
        result = FunctionCallInvoke(&funcPtrCallInfo);
    } MADLIB_PG_CATCH {
        throw std::runtime_error(std::string("Exception while invoking '")
            + mSysInfo->functionInformation(mFuncInfo->oid)->getFullName()
            + "'. Error was:\n" + MADLIB_PG_ERROR_DATA()->message);
    } MADLIB_PG_END_TRY;

    if (oldContext) {
        MemoryContextSwitchTo(oldContext);
        TypeInformation* typeInfo
            = mSysInfo->typeInformation(mFuncInfo->rettype);
        result = datumCopy(result, typeInfo->isByValue(), typeInfo->getLen());
        MemoryContextDelete(callContext);
    }

    return funcPtrCallInfo.isnull
        ? AnyType()
        : AnyType(mSysInfo, result, mFuncInfo->rettype, /* isMutable */ true);
}

inline
AnyType
FunctionHandle::operator()() {
    AnyType nil;
    return invoke(nil);
}

// In the following, we define
// AnyType operator()(AnyType& inArgs1, ..., AnyType& inArgsN)
// using Boost.Preprocessor.
#define MADLIB_APPEND_ARG(z, n, data) \
    << data ## n
#define MADLIB_OPERATOR_DEF(z, n, _ignored) \
    inline \
    AnyType \
    FunctionHandle::operator()( \
        BOOST_PP_ENUM_PARAMS_Z(z, BOOST_PP_INC(n), AnyType inArg) \
    ) { \
        AnyType args; \
        args BOOST_PP_REPEAT(BOOST_PP_INC(n), MADLIB_APPEND_ARG, inArg); \
        return invoke(args); \
    }
BOOST_PP_REPEAT(MADLIB_FUNC_MAX_ARGS, MADLIB_OPERATOR_DEF, 0 /* ignored */)
#undef MADLIB_OPERATOR_DEF
#undef MADLIB_APPEND_ARG

inline
SystemInformation*
FunctionHandle::getSysInfo() const {
    return mSysInfo;
}

} // namespace postgres

} // namespace dbconnector

} // namespace madlib

#endif // defined(MADLIB_POSTGRES_FUNCTIONHANDLE_IMPL_HPP)
