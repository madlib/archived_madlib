/* ----------------------------------------------------------------------- *//**
 *
 * @file WeightedSample_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_SAMPLE_WEIGHTED_SAMPLE_IMPL_HPP
#define MADLIB_MODULES_SAMPLE_WEIGHTED_SAMPLE_IMPL_HPP

#include <boost/tr1/random.hpp>

// Import TR1 names (currently used from boost). This can go away once we make
// the switch to C++11.
namespace std {
    using tr1::bernoulli_distribution;
}

namespace madlib {

namespace modules {

namespace sample {

template <class Container, class T>
inline
WeightedSampleAccumulator<Container, T>::WeightedSampleAccumulator(
    Init_type& inInitialization)
  : Base(inInitialization) {

    this->initialize();
}

template <class Container, class T>
inline
void
bindWeightedSampleAcc(
    WeightedSampleAccumulator<Container, T>& ioAccumulator,
    typename WeightedSampleAccumulator<Container, T>::ByteStream_type&
        inStream) {
    
    inStream >> ioAccumulator.weight_sum >> ioAccumulator.sample;
}

template <class Container>
inline
void
bindWeightedSampleAcc(
    WeightedSampleAccumulator<Container, MappedColumnVector>& ioAccumulator,
    typename WeightedSampleAccumulator<Container, MappedColumnVector>
        ::ByteStream_type& inStream) {
    
    inStream >> ioAccumulator.weight_sum >> ioAccumulator.header.width;
    uint32_t actualWidth = ioAccumulator.header.width.isNull()
        ? 0
        : static_cast<uint32_t>(ioAccumulator.header.width);
    inStream >> ioAccumulator.sample.rebind(actualWidth);
}

/**
 * @brief Bind all elements of the state to the data in the stream
 *
 * The bind() is special in that even after running operator>>() on an element,
 * there is no guarantee yet that the element can indeed be accessed. It is
 * cruicial to first check this.
 *
 * Provided that this methods correctly lists all member variables, all other
 * methods can, however, rely on that fact that all variables are correctly
 * initialized and accessible.
 */
template <class Container, class T>
inline
void
WeightedSampleAccumulator<Container, T>::bind(ByteStream_type& inStream) {
    bindWeightedSampleAcc(*this, inStream);
}

template <class Container, class T>
inline
void
prepareSample(WeightedSampleAccumulator<Container, T>&, const T&) { }

template <class Container>
inline
void
prepareSample(
    WeightedSampleAccumulator<Container, MappedColumnVector>& ioAccumulator,
    const MappedColumnVector& inX) {
    
    uint32_t width = static_cast<uint32_t>(inX.size());
    if (width > ioAccumulator.header.width) {
        ioAccumulator.header.width = width;
        ioAccumulator.resize();
    }
}

/**
 * @brief Update the accumulation state
 */
template <class Container, class T>
inline
WeightedSampleAccumulator<Container, T>&
WeightedSampleAccumulator<Container, T>::operator<<(
    const tuple_type& inTuple) {

    const T& x = std::get<0>(inTuple);
    const double& weight = std::get<1>(inTuple);

    // Instead of throwing an error, we will just ignore rows with a negative
    // weight
    if (weight > 0.) {
        weight_sum += weight;
        std::bernoulli_distribution success(weight / weight_sum);
        // Note that a NativeRandomNumberGenerator object is stateless, so it
        // is not a problem to instantiate an object for each RN generation...
        NativeRandomNumberGenerator generator;
        if (success(generator)) {
            prepareSample(*this, x);
            sample = x;
        }
    }
    
    return *this;
}

/**
 * @brief Merge with another accumulation state
 */
template <class Container, class T>
template <class OtherContainer>
inline
WeightedSampleAccumulator<Container, T>&
WeightedSampleAccumulator<Container, T>::operator<<(
    const WeightedSampleAccumulator<OtherContainer, T>& inOther) {

    // Initialize if necessary
    if (weight_sum == 0) {
        *this = inOther;
        return *this;
    }

    *this << tuple_type(inOther.sample, inOther.weight_sum);
    return *this;
}

template <class Container, class T>
template <class OtherContainer>
inline
WeightedSampleAccumulator<Container, T>&
WeightedSampleAccumulator<Container, T>::operator=(
    const WeightedSampleAccumulator<OtherContainer, T>& inOther) {

    this->copy(inOther);
    return *this;
}

} // namespace sample

} // namespace modules

} // namespace madlib

#endif // defined(MADLIB_MODULES_REGRESS_LINEAR_REGRESSION_IMPL_HPP)
