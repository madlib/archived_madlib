
#ifndef MADLIB_DENSE_LINEAR_SYSTEMS_STATES_
#define MADLIB_DENSE_LINEAR_SYSTEMS_STATES_

namespace madlib {
namespace modules {
namespace linear_systems {

using namespace dbal;
using namespace madlib::dbal::eigen_integration;

template <class Container>
class ResidualState: public DynamicStruct<ResidualState<Container>, Container>{
  public:
    typedef DynamicStruct<ResidualState, Container> Base;
    MADLIB_DYNAMIC_STRUCT_TYPEDEFS;

    ResidualState(Init_type& inInitialization);
    void bind(ByteStream_type& inStream);
    template <class OtherContainer> ResidualState& operator=(
        const ResidualState<OtherContainer>& inOther);

    uint64_type numRows;
    uint16_type widthOfA;
    double_type residual_norm;
    double_type b_norm;
    ColumnVector_type solution;


};

// ------------------------------------------------------------------------
template <class Container>
inline ResidualState<Container>::ResidualState (Init_type& inInitialization) :
    Base(inInitialization){
    this->initialize();
}

// ------------------------------------------------------------------------
template <class Container>
inline void ResidualState<Container>::bind(ByteStream_type& inStream){

    inStream >> numRows 
             >> widthOfA
             >> residual_norm
             >> b_norm;
    uint16_t actualWidthOfA = widthOfA.isNull() ? 0 : static_cast<uint16_t>(widthOfA);
    inStream >> solution.rebind(actualWidthOfA);

}

// ------------------------------------------------------------------------
template <class Container>
template <class OtherContainer>
inline ResidualState<Container>& ResidualState<Container>::operator= (
    const ResidualState<OtherContainer>& inOther){
    this->copy(inOther);
    return *this;
}


} // 
} // 
} // 
#endif


