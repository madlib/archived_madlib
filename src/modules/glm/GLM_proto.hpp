/* ----------------------------------------------------------------------- *//**
 *
 * @file GLM_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_GLM_GLM_PROTO_HPP
#define MADLIB_MODULES_GLM_GLM_PROTO_HPP

#include "family.hpp"
#include "link.hpp"

namespace madlib {

namespace modules {

namespace glm {

// Use Eigen
using namespace dbal;
using namespace dbal::eigen_integration;

template <class Container, class Family=Gaussian, class Link=Identity>
class GLMAccumulator
  : public DynamicStruct<GLMAccumulator<Container,Family,Link>,Container> {
public:
    typedef DynamicStruct<GLMAccumulator,Container> Base;
    MADLIB_DYNAMIC_STRUCT_TYPEDEFS;
    typedef std::tuple<MappedColumnVector,double> tuple_type;

    GLMAccumulator(Init_type& inInitialization);
    void bind(ByteStream_type& inStream);
    GLMAccumulator& operator<<(const tuple_type& inTuple);
    template <class C, class F, class L>
    GLMAccumulator& operator<<(const GLMAccumulator<C,F,L>& inOther);
    template <class C, class F, class L>
    GLMAccumulator& operator=(const GLMAccumulator<C,F,L>& inOther);
    void apply();
    void reset();
    bool empty() const { return this->num_rows == 0; }

    uint16_type num_features;
    ColumnVector_type beta;

    uint64_type num_rows;
    bool_type terminated;
    double_type loglik;
    ColumnVector_type X_trans_W_Y;
    Matrix_type X_trans_W_X;

    Matrix_type xcov;

    double_type dispersion;
    double_type dispersion_accum; // to accumulate the dispersion
};

// ------------------------------------------------------------------------

class GLMResult {
public:
    template <class Container> GLMResult(
        const GLMAccumulator<Container>& inState);
    template <class Container> GLMResult& compute(
        const GLMAccumulator<Container>& inState);

    // native arrays returns to db without copying
    double loglik;
    MutableNativeColumnVector coef;
    MutableNativeColumnVector std_err;
    MutableNativeColumnVector z_stats;
    MutableNativeColumnVector p_values;
    uint64_t num_rows_processed;
    double dispersion;
};

} // namespace glm

} // namespace modules

} // namespace madlib

#endif // defined(MADLIB_MODULES_GLM_GLM_PROTO_HPP)
