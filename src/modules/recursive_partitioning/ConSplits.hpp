#ifndef MADLIB_MODULES_RP_CON_SPLITS_HPP
#define MADLIB_MODULES_RP_CON_SPLITS_HPP

#include <dbconnector/dbconnector.hpp>

namespace madlib {
namespace modules {
namespace recursive_partitioning {

// Use Eigen
using namespace dbal;
using namespace dbal::eigen_integration;
// -------------------------------------------------------------------------

template<class Container>
class ConSplitsSample
  : public DynamicStruct<ConSplitsSample<Container>, Container> {
public:
    typedef DynamicStruct<ConSplitsSample, Container> Base;
    MADLIB_DYNAMIC_STRUCT_TYPEDEFS;

    // functions
    ConSplitsSample(Init_type& inInitialization): Base(inInitialization) {
        this->initialize();
    }

    void bind(ByteStream_type& inStream) {
        inStream >> num_rows
            >> num_splits
            >> num_features
            >> buff_size;

        uint16_t n_features = 0u;
        uint32_t buff_sz = 0;
        if (!num_rows.isNull()) {
            n_features = num_features;
            buff_sz = buff_size;
        }

        inStream >> sample.rebind(n_features, buff_sz);
    }

    ConSplitsSample& operator<<(const MappedColumnVector& inVec) {
        sample.col(num_rows) = inVec;
        num_rows++;
        return *this;
    }

    bool empty() const { return this->num_rows == 0; }

    uint32_type num_rows;
    uint16_type num_splits;
    uint16_type num_features;
    uint32_type buff_size;
    Matrix_type sample;
};
// ------------------------------------------------------------

// The continuous split result is returned in a bytea8 data type
// So we need a dynamic struct.
// Unfortunately Python has difficulty in passing matrix around
template<class Container>
class ConSplitsResult
  : public DynamicStruct<ConSplitsResult<Container>, Container> {
public:
    typedef DynamicStruct<ConSplitsResult, Container> Base;
    MADLIB_DYNAMIC_STRUCT_TYPEDEFS;

    ConSplitsResult(Init_type& inInitialization): Base(inInitialization) {
        this->initialize();
    }

    void bind(ByteStream_type& inStream) {
        inStream >> num_features >> num_splits;
        uint16_t n_features = 0u;
        uint16_t n_splits = 0u;
        if (!num_features.isNull()) {
            n_features = num_features;
            n_splits = num_splits;
        }
        inStream >> con_splits.rebind(n_features, n_splits);
    }

    uint16_type num_features;
    uint16_type num_splits;
    Matrix_type con_splits;
};

} // namespace recursive_partitioning
} // namespace modules
} // namespace madlib

#endif // defined(MADLIB_MODULES_RP_CON_SPLITS_HPP)
