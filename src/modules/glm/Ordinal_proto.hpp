/* ----------------------------------------------------------------------- *//**
 *
 * @file Ordinal_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_GLM_ORDINAL_PROTO_HPP
#define MADLIB_MODULES_GLM_ORDINAL_PROTO_HPP

#include "family.hpp"
#include "link.hpp"
#include <modules/convex/newton.hpp>

namespace madlib {

namespace modules {

namespace glm {

// Use Eigen
using namespace dbal;
using namespace dbal::eigen_integration;

template <class Container, class Family=Multinomial, class Link=OrdinalLogit>
class OrdinalAccumulator
  : public DynamicStruct<OrdinalAccumulator<Container,Family,Link>,Container> {
public:
    typedef DynamicStruct<OrdinalAccumulator,Container> Base;
    MADLIB_DYNAMIC_STRUCT_TYPEDEFS;
    typedef std::tuple<MappedColumnVector,double> tuple_type;

    OrdinalAccumulator(Init_type& inInitialization);
    void bind(ByteStream_type& inStream);
    OrdinalAccumulator& operator<<(const tuple_type& inTuple);
    template <class C, class F, class L>
    OrdinalAccumulator& operator<<(const OrdinalAccumulator<C,F,L>& inOther);
    template <class C, class F, class L>
    OrdinalAccumulator& operator=(const OrdinalAccumulator<C,F,L>& inOther);
    void apply();
    void reset();
    bool empty() const { return this->num_rows == 0; }

    uint16_type num_features;
    uint16_type num_categories;
    uint64_type num_rows;
    bool_type terminated;
    double_type loglik;

    convex::Newton<Container,OrdinalAccumulator> optimizer;

    Matrix_type vcov;
};

// ------------------------------------------------------------------------


class OrdinalResult {
public:
    template <class Container> OrdinalResult(
        const OrdinalAccumulator<Container>& inState);
    template <class Container> OrdinalResult& compute(
        const OrdinalAccumulator<Container>& inState);

    // native arrays returns to db without copying
    double loglik;
    MutableNativeColumnVector coef_alpha;
    MutableNativeColumnVector std_err_alpha;
    MutableNativeColumnVector z_stats_alpha;
    MutableNativeColumnVector p_values_alpha;
    uint64_t num_rows_processed;
    MutableNativeColumnVector coef_beta;
    MutableNativeColumnVector std_err_beta;
    MutableNativeColumnVector z_stats_beta;
    MutableNativeColumnVector p_values_beta;
};

} // namespace glm

} // namespace modules

} // namespace madlib

#endif // defined(MADLIB_MODULES_GLM_ORDINAL_PROTO_HPP)
