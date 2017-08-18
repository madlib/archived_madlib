/* ------------------------------------------------------
 *
 * @file feature_encoding.cpp
 *
 * @brief Feature encoding functions for Decision Tree models
 *
 *
 */ /* ----------------------------------------------------------------------- */

#include <sstream>
#include <algorithm>
#include <dbconnector/dbconnector.hpp>
#include "feature_encoding.hpp"
#include "ConSplits.hpp"

namespace madlib {

namespace modules {

namespace recursive_partitioning {

// Use Eigen
using namespace dbal;
using namespace dbal::eigen_integration;
// ------------------------------------------------------------


AnyType
print_con_splits::run(AnyType &args){
    ConSplitsResult<RootContainer> state = args[0].getAs<ByteString>();
    AnyType tuple;
    tuple << state.con_splits;
    return tuple;
}

AnyType
dst_compute_con_splits_transition::run(AnyType &args){
    ConSplitsSample<MutableRootContainer> state = args[0].getAs<MutableByteString>();
    if (!state.empty() && state.num_rows >= state.buff_size) {
        return args[0];
    }
    // NULLs are handled by caller to ensure consistency between
    // feature encoding and tree training
    MappedColumnVector con_features = args[1].getAs<MappedColumnVector>();

    if (state.empty()) {
        uint32_t n_per_seg = args[2].getAs<uint32_t>();
        uint16_t n_bins = args[3].getAs<uint16_t>();

        state.num_splits = static_cast<uint16_t>(n_bins - 1);
        state.num_features = static_cast<uint16_t>(con_features.size());
        state.buff_size = n_per_seg;
        state.resize();
    }

    state << con_features;

    return state.storage();
}
// ------------------------------------------------------------

AnyType
dst_compute_con_splits_final::run(AnyType &args){
    ConSplitsSample<RootContainer> state = args[0].getAs<ByteString>();
    ConSplitsResult<MutableRootContainer> result =
            defaultAllocator().allocateByteString<
                dbal::FunctionContext, dbal::DoZero, dbal::ThrowBadAlloc>(0);
    result.num_features = state.num_features;
    result.num_splits = state.num_splits;
    result.resize();

    if (state.num_rows <= state.num_splits) {
        std::stringstream error_msg;
        // In message below, add 1 to state.num_splits since the meaning of
        // "splits" for the caller is the number of quantiles, where as
        // "splits" in this function is the number of values dividing the data
        // into quantiles.
        error_msg << "Decision tree error: Number of splits ("
            << state.num_splits + 1
            << ") is larger than the number of records ("
            << state.num_rows << ")";
        throw std::runtime_error(error_msg.str());
    }

    int bin_size = static_cast<int>(state.num_rows / (state.num_splits + 1));
    for (Index i = 0; i < state.num_features; i ++) {
        // sorting the values for each feature
        ColumnVector feature_i_sample = \
                state.sample.row(i).segment(0, state.num_rows);
        std::sort(feature_i_sample.data(),
                  feature_i_sample.data() + feature_i_sample.size());
        // binning
        for (int j = 0; j < state.num_splits; j ++) {
            result.con_splits(i, j) = feature_i_sample(bin_size * (j + 1) - 1);
        }
    }

    return result.storage();
}
// ------------------------------------------------------------

AnyType
dst_compute_entropy_transition::run(AnyType &args){
    int encoded_dep_var = args[1].getAs<int>();
    if (encoded_dep_var < 0) {
        throw std::runtime_error("unexpected negative encoded_dep_var");
    }

    MutableNativeIntegerVector state;
    if (args[0].isNull()) {
        // allocate the state for the first row
        int num_dep_var = args[2].getAs<int>();
        if (num_dep_var <= 0) {
            throw std::runtime_error("unexpected non-positive num_dep_var");
        }
        state.rebind(this->allocateArray<int>(num_dep_var));
    } else {
        // avoid state copying if initialized
        state.rebind(args[0].getAs<MutableArrayHandle<int> >());
    }

    if (encoded_dep_var >= state.size()) {
        std::stringstream err_msg;
        err_msg << "out-of-bound encoded_dep_var=" << encoded_dep_var
            << ", while smaller than " << state.size() << " expected";
        throw std::runtime_error(err_msg.str());
    }

    state(encoded_dep_var) ++;
    return state;
}
// ------------------------------------------------------------

AnyType
dst_compute_entropy_merge::run(AnyType &args){
    if (args[0].isNull()) { return args[1]; }
    if (args[1].isNull()) { return args[0]; }

    MutableNativeIntegerVector state0 =
            args[0].getAs<MutableNativeIntegerVector>();
    NativeIntegerVector state1 = args[1].getAs<NativeIntegerVector>();

    state0 += state1;
    return state0;
}
// ------------------------------------------------------------

namespace {

double
p_log2_p(const double &p) {
    if (p < 0.) { throw std::runtime_error("unexpected negative probability"); }
    if (p == 0.) { return 0.; }
    return p * log2(p);
}

}
// ------------------------------------------------------------

AnyType
dst_compute_entropy_final::run(AnyType &args){
    MappedIntegerVector state = args[0].getAs<MappedIntegerVector>();
    double sum_of_dep_counts = static_cast<double>(state.sum());
    ColumnVector probs = state.cast<double>() / sum_of_dep_counts;
    // usage of unaryExpr with functor:
    // http://eigen.tuxfamily.org/dox/classEigen_1_1MatrixBase.html#a23fc4bf97168dee2516f85edcfd4cfe7
    return -(probs.unaryExpr(std::ptr_fun(p_log2_p))).sum();
}
// ------------------------------------------------------------

inline
bool
cmp_text(text* s1, text* s2) {
    if (VARSIZE_ANY(s1) != VARSIZE_ANY(s2))
        return false;
    else {
        return strncmp(VARDATA_ANY(s1), VARDATA_ANY(s2), VARSIZE_ANY(s1) - VARHDRSZ) == 0;
    }
}
// ------------------------------------------------------------

AnyType
map_catlevel_to_int::run(AnyType &args){
    ArrayHandle<text*> cat_values = args[0].getAs<ArrayHandle<text*> >();
    ArrayHandle<text*> cat_levels = args[1].getAs<ArrayHandle<text*> >();
    ArrayHandle<int> n_levels = args[2].getAs<ArrayHandle<int> >();
    bool null_as_category = args[3].getAs<bool>();

    MutableArrayHandle<int> cat_int = allocateArray<int>(n_levels.size());
    int pos = 0;
    for (size_t i = 0; i < n_levels.size(); i++) {
        // linear search to find a match

        // if cat_values contains any not present in cat_levels, then the mapped
        // integer is -1. If cat_values contains a known cat_level, then the
        // mapped integer is the index of that value in cat_levels.
        int match = -1;
        for (int j = 0; j < n_levels[i]; j++){
            if (cmp_text(cat_values[i], cat_levels[pos + j])) {
                match = j;
                break;
            }
        }

        // If null_as_category is True, then match is set to the last index
        // instead of -1 since the last index is expected to represent NULL.
        if (match == -1 and null_as_category){
            cat_int[i] = n_levels[i] - 1;
        } else {
            cat_int[i] = match;
        }
        pos += static_cast<int>(n_levels[i]);
    }
    return cat_int;
}
// --------------------------------------------------------------

AnyType
get_bin_value_by_index::run(AnyType &args) {
    ConSplitsResult<RootContainer> con_splits_results = args[0].getAs<ByteString>();
    int feature_index = args[1].getAs<int>();
    int bin_index = args[2].getAs<int>();

    // -1 is used to indicate a NaN value
    if (bin_index < 0)
        return std::numeric_limits<double>::quiet_NaN();

    // assuming we use <= to create bins by values in con_splits
    // see TreeAccumulator::operator<<(const tuple_type&)
    if (bin_index < con_splits_results.con_splits.cols()) {
        // covering ranges (-inf,v_0], ..., (v_{n-2}, v_{n-1}]
        return con_splits_results.con_splits(feature_index, bin_index);
    } else {
        // covering range (v_{n-1}, +inf)
        return con_splits_results.con_splits(feature_index, bin_index-1) + 1.;
    }
}
// --------------------------------------------------------------

AnyType
get_bin_index_by_value::run(AnyType &args) {
    double bin_value = args[0].getAs<double>();
    // we return a -1 index is the value is NaN
    if (std::isnan(bin_value)) { return -1; }

    ConSplitsResult<RootContainer> con_splits_results = args[1].getAs<ByteString>();
    if (con_splits_results.con_splits.cols() <= 0) { return Null(); }

    int feature_index = args[2].getAs<int>();
    // assuming we use <= to create bins by values in con_splits
    // and each row in con_splits is sorted in ascending order
    // see TreeAccumulator::operator<<(const tuple_type&)
    // and dst_compute_con_splits_final::run(AnyType &)
    // each v_i covering ranges (-inf,v_0], ..., (v_{n-2}, v_{n-1}]
    int result = -1;
    int low = 0;
    int high = static_cast<int>(con_splits_results.con_splits.cols()) - 1;
    if (bin_value > con_splits_results.con_splits(feature_index, high)) {
        // covering range (v_{n-1}, +inf)
        result = high + 1;
    }
    while (low < high) {
        int mid = (low + high) / 2;
        if (bin_value <= con_splits_results.con_splits(feature_index, mid)) {
            high = mid;
        } else {
            low = mid + 1;
        }
    }
    result = high;

#ifndef NDEBUG
    // test code for binary search (mimic std::lower_bound())
    if (low != high) {
        dbout << "bin_value: " << bin_value
            << "\ncon_splits_results.con_splits.row(feature_index): " << con_splits_results.con_splits.row(feature_index)
            << "\nlow: " << low << ", high: " << high << std::endl;
        throw std::runtime_error("low_bound() has bug!");
    }

    for (int i = 0; i < con_splits_results.con_splits.cols(); i ++) {
        if (bin_value <= con_splits_results.con_splits(feature_index, i)) {
            if (i != result) {
                dbout << "bin_value: " << bin_value
                    << "\ncon_splits_results.con_splits.row(feature_index): " << con_splits_results.con_splits.row(feature_index)
                    << "\nresult: " << result << ", i: " << i << std::endl;
                throw std::runtime_error("low_bound() has bug!");
            }
            return i;
        }
    }
#endif

    return result;
}
// --------------------------------------------------------------

AnyType
get_bin_indices_by_values::run(AnyType &args) {
    // Null-handling is done by declaring it as strict
    MappedColumnVector bin_values = args[0].getAs<MappedColumnVector>();
    ConSplitsResult<RootContainer> con_splits_results = args[1].getAs<ByteString>();

    if (con_splits_results.con_splits.cols() <= 0) { return Null(); }

    MutableNativeIntegerVector bin_indices(
            this->allocateArray<int>(bin_values.size()));

    // assuming we use <= to create bins by values in con_splits
    // and each row in con_splits is sorted in ascending order
    // see TreeAccumulator::operator<<(const tuple_type&)
    // and dst_compute_con_splits_final::run(AnyType &)
    // each v_i covering ranges (-inf,v_0], ..., (v_{n-2}, v_{n-1}]
    for (Index i = 0; i < bin_values.size(); i ++) {
        int low = 0;
        int high = static_cast<int>(con_splits_results.con_splits.cols()) - 1;
        if (std::isnan(bin_values(i))) {
            // we return a -1 index is the value is NaN
            bin_indices(i) = -1;
        } else if (bin_values(i) > con_splits_results.con_splits(i, high)) {
            // covering range (v_{n-1}, +inf)
            bin_indices(i) = high + 1;
        } else {
            // binary search
            while (low < high) {
                int mid = (low + high) / 2;
                if (bin_values(i) <= con_splits_results.con_splits(i, mid)) {
                    high = mid;
                } else {
                    low = mid + 1;
                }
            }
            bin_indices(i) = high;
        }
    }

    return bin_indices;
}

} // namespace recursive_partitioning
} // namespace modules
} // namespace madlib
