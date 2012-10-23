/* ----------------------------------------------------------------------- *//**
 *
 * @file WeightedSample_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_SAMPLE_WEIGHTED_SAMPLE_PROTO_HPP
#define MADLIB_MODULES_SAMPLE_WEIGHTED_SAMPLE_PROTO_HPP

namespace madlib {

namespace modules {

namespace sample {

// Use Eigen
using namespace dbal;
using namespace dbal::eigen_integration;

template <class T, bool IsMutable>
struct WeightedSampleHeader { };

template <bool IsMutable>
struct WeightedSampleHeader<MappedColumnVector, IsMutable> {
    typename DynamicStructType<uint32_t, IsMutable>::type width;
};

template <class Container, class T>
class WeightedSampleAccumulator
  : public DynamicStruct<WeightedSampleAccumulator<Container, T>, Container> {

public:
    typedef DynamicStruct<WeightedSampleAccumulator, Container> Base;
    MADLIB_DYNAMIC_STRUCT_TYPEDEFS;
    typedef std::tuple<T, double> tuple_type;

    WeightedSampleAccumulator(Init_type& inInitialization);
    void bind(ByteStream_type& inStream);
    WeightedSampleAccumulator& operator<<(const tuple_type& inTuple);
    template <class OtherContainer> WeightedSampleAccumulator& operator<<(
        const WeightedSampleAccumulator<OtherContainer, T>& inOther);
    template <class OtherContainer> WeightedSampleAccumulator& operator=(
        const WeightedSampleAccumulator<OtherContainer, T>& inOther);

    double_type weight_sum;
    WeightedSampleHeader<T, isMutable> header;
    typename DynamicStructType<T, isMutable>::type sample;
};

} // namespace sample

} // namespace modules

} // namespace madlib

#endif // defined(MADLIB_MODULES_SAMPLE_WEIGHTED_SAMPLE_PROTO_HPP)
