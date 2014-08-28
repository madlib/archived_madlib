/* ------------------------------------------------------
 *
 * @file feature_encoding.cpp
 *
 * @brief Feature encoding functions for Decision Tree models
 *
 *
 */ /* ----------------------------------------------------------------------- */

#include <sstream>
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
dst_compute_con_splits_transition::run(AnyType &args){
    ConSplitsSample<MutableRootContainer> state = args[0].getAs<MutableByteString>();
    if (!state.empty() && state.num_rows >= state.buff_size) {
        return args[0];
    }
    // NULL-handling is done in python to make sure consistency b/w
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
dst_compute_con_splits_merge::run(AnyType &args){
    ConSplitsSample<MutableRootContainer> stateLeft = args[0].getAs<MutableByteString>();
    if (stateLeft.empty()) { return args[1]; }
    ConSplitsSample<RootContainer> stateRight = args[1].getAs<ByteString>();

    stateLeft << stateRight;

    return stateLeft.storage();
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

    // MutableNativeMatrix splits(this->allocateArray<double>(state.num_splits,
    //             state.num_features), state.num_features, state.num_splits);
    ColumnVector to_sort;
    if (static_cast<int>(state.num_rows) < static_cast<int>(state.num_splits) + 1){
        std::stringstream error_msg;
        error_msg << "Decision tree error: Number of splits (" << state.num_splits
                  << ") is larger than the number of records (" << state.num_rows << ")";
        throw std::runtime_error(error_msg.str());
    }

    int bin_size = static_cast<int>(state.num_rows / (state.num_splits + 1));
    for (Index i = 0; i < state.num_features; i ++) {
        to_sort = state.sample.row(i).segment(0, state.num_rows);
        std::sort(to_sort.data(), to_sort.data() + to_sort.size());

        for (int j = 0; j < state.num_splits; j++)
            result.con_splits(i, j) = to_sort(bin_size * (j + 1) - 1);
    }

    // A Composite type is converted to string in Python when passed by the backend,
    //  whereas, a 2D array is not. A 2D array inside a composite type can be
    //  successfully passed in as a string. Hence, we embed the matrix inside a
    //  composite type.

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
    double sum = static_cast<double>(state.sum());
    ColumnVector probilities = state.cast<double>() / sum;
    // usage of unaryExpr with functor:
    // http://eigen.tuxfamily.org/dox/classEigen_1_1MatrixBase.html#a23fc4bf97168dee2516f85edcfd4cfe7
    return -(probilities.unaryExpr(std::ptr_fun(p_log2_p))).sum();
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

    MutableArrayHandle<int> cat_int = allocateArray<int>(n_levels.size());
    int pos = 0;
    for (size_t i = 0; i < n_levels.size(); i++) {
        // linear search to find a match
        int match = 0;
        for (int j = 0; j < n_levels[i]; j++)
            if (cmp_text(cat_values[i], cat_levels[pos + j])) {
                match = j;
                break;
            }
        cat_int[i] = match;
        pos += static_cast<int>(n_levels[i]);
    }
    return cat_int;
}

} // namespace recursive_partitioning
} // namespace modules
} // namespace madlib
