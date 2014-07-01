
#ifndef MADLIB_CLUSTERED_ERRORS_
#define MADLIB_CLUSTERED_ERRORS_

namespace madlib {
namespace modules {
namespace regress {

using namespace dbal;
using namespace madlib::dbal::eigen_integration;

template <class Container>
class ClusteredState: public DynamicStruct<ClusteredState<Container>, Container>
{
  public:
    typedef DynamicStruct<ClusteredState, Container> Base;
    MADLIB_DYNAMIC_STRUCT_TYPEDEFS;

    ClusteredState(Init_type& inInitialization);
    void bind(ByteStream_type& inStream);
    template <class OtherContainer> ClusteredState& operator=(
        const ClusteredState<OtherContainer>& inOther);

    uint64_type numRows;
    uint16_type widthOfX;
    uint16_type numCategories;
    uint16_type refCategory;
    ColumnVector_type coef;
    Matrix_type bread;
    Matrix_type meat_half;
};

// ------------------------------------------------------------------------

template <class Container>
inline ClusteredState<Container>::ClusteredState (Init_type& inInitialization) :
    Base(inInitialization)
{
    this->initialize();
}

// ------------------------------------------------------------------------

template <class Container>
inline void ClusteredState<Container>::bind(ByteStream_type& inStream)
{
    inStream >> numRows >> widthOfX >> numCategories >> refCategory;
    uint16_t actualWidthOfX = widthOfX.isNull() ? static_cast<uint16_t>(0) : static_cast<uint16_t>(widthOfX);
    inStream >> coef.rebind(actualWidthOfX)
             >> meat_half.rebind(1, actualWidthOfX)
             >> bread.rebind(actualWidthOfX, actualWidthOfX);
}

// ------------------------------------------------------------------------

template <class Container>
template <class OtherContainer>
inline ClusteredState<Container>& ClusteredState<Container>::operator= (
    const ClusteredState<OtherContainer>& inOther)
{
    this->copy(inOther);
    return *this;
}

}
}
}

#endif
