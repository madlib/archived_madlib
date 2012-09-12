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
    typedef AnyType (*Pointer)(AnyType&);

    UDF() { }

    template <class Function>
    static Datum call(FunctionCallInfo fcinfo);

    template <class Function>
    static AnyType invoke(AnyType& args);
    
    // FIXME: The following code until the end of this class is a dirty hack
    // that needs to go
    template <class Function>
    static Datum SRF_invoke(FunctionCallInfo fcinfo);

protected:
    template <class Function>
    static FuncCallContext* SRF_percall_setup(FunctionCallInfo fcinfo);

    template <class Function>
    static bool SRF_is_firstcall(FunctionCallInfo fcinfo);
};

template <class Function>
UDF::Pointer funcPtr() {
    return UDF::invoke<Function>;
}

} // namespace postgres

} // namespace dbconnector

} // namespace madlib

#endif // defined(MADLIB_POSTGRES_UDF_PROTO_HPP)
