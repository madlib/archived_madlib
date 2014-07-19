/* ----------------------------------------------------------------------- *//**
 *
 * @file MultiResponseGLM_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_GLM_MULTIRESPONSEGLM_PROTO_HPP
#define MADLIB_MODULES_GLM_MULTIRESPONSEGLM_PROTO_HPP

#include "family.hpp"
#include "link.hpp"
#include <modules/convex/newton.hpp>

namespace madlib {

namespace modules {

namespace glm {

// Use Eigen
using namespace dbal;
using namespace dbal::eigen_integration;

template <class Container, class Family=Multinomial, class Link=MultiLogit>
class MultiResponseGLMAccumulator
  : public DynamicStruct<MultiResponseGLMAccumulator<Container,Family,Link>,Container> {
public:
    typedef DynamicStruct<MultiResponseGLMAccumulator,Container> Base;
    MADLIB_DYNAMIC_STRUCT_TYPEDEFS;
    typedef std::tuple<MappedColumnVector,double> tuple_type;

    MultiResponseGLMAccumulator(Init_type& inInitialization);
    void bind(ByteStream_type& inStream);
    MultiResponseGLMAccumulator& operator<<(const tuple_type& inTuple);
    template <class C, class F, class L>
    MultiResponseGLMAccumulator& operator<<(const MultiResponseGLMAccumulator<C,F,L>& inOther);
    template <class C, class F, class L>
    MultiResponseGLMAccumulator& operator=(const MultiResponseGLMAccumulator<C,F,L>& inOther);
    void apply();
    void reset();
    bool empty() const { return this->num_rows == 0; }

    uint16_type num_features;
    uint16_type num_categories;
    uint64_type num_rows;
    bool_type terminated;
    double_type loglik;

    convex::Newton<Container,MultiResponseGLMAccumulator> optimizer;

    Matrix_type vcov;
};

// ------------------------------------------------------------------------
class MultiResponseGLMResult {
public:
    template <class Container> MultiResponseGLMResult(
        const MultiResponseGLMAccumulator<Container>& inState);
    template <class Container> MultiResponseGLMResult& compute(
        const MultiResponseGLMAccumulator<Container>& inState);

    // native arrays returns to db without copying
    double loglik;
    MutableNativeMatrix coef;
    MutableNativeMatrix std_err;
    MutableNativeMatrix z_stats;
    MutableNativeMatrix p_values;
    uint64_t num_rows_processed;
};

} // namespace glm

} // namespace modules

} // namespace madlib

#endif // defined(MADLIB_MODULES_GLM_MULTIRESPONSEGLM_PROTO_HPP)
