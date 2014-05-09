/* ----------------------------------------------------------------------- *//**
 *
 * @file DynamicStructHelper_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_DBAL_DYNAMICSTRUCT_HELPER_PROTO_HPP
#define MADLIB_DBAL_DYNAMICSTRUCT_HELPER_PROTO_HPP

namespace madlib { namespace dbal {

using namespace dbal;
using namespace madlib::dbal::eigen_integration;

template <class Container>
class DynamicStructHelper: public DynamicStruct<DynamicStructHelper<Container>, Container>
{
  public:
    typedef DynamicStruct<DynamicStructHelper, Container> Base;
    MADLIB_DYNAMIC_STRUCT_TYPEDEFS;

    DynamicStructHelper(Init_type& inInitialization);
    virtual void bind(ByteStream_type& inStream);
    virtual template <class OtherContainer> DynamicStructHelper& operator=(
        const DynamicStructHelper<OtherContainer>& inOther);
};

} } // namespace madlib::dbal

#endif // defined(MADLIB_DBAL_DYNAMICSTRUCT_HELPER_PROTO_HPP)
