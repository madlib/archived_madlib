/* ------------------------------------------------------
 *
 * @file decision_tree.cpp
 *
 * @brief Decision Tree models for regression and classification
 *
 *
 */ /* ----------------------------------------------------------------------- */

#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <iterator>

#include <dbconnector/dbconnector.hpp>

#include "DT_proto.hpp"
#include "DT_impl.hpp"
#include "ConSplits.hpp"

#include "decision_tree.hpp"

namespace madlib {

// Use Eigen
using namespace dbal::eigen_integration;

namespace modules {

namespace recursive_partitioning {

enum { NOT_FINISHED=0, FINISHED, TERMINATED };
// ------------------------------------------------------------
// types
// ------------------------------------------------------------
typedef DecisionTree<MutableRootContainer> MutableTree;
typedef DecisionTree<RootContainer> Tree;

// Transition State for collecting statistics
typedef TreeAccumulator<RootContainer, Tree> LevelState;
typedef TreeAccumulator<MutableRootContainer, Tree> MutableLevelState;

// ------------------------------------------------------------
// functions
// ------------------------------------------------------------
AnyType
initialize_decision_tree::run(AnyType & args){
    // elog(INFO, "Initializing tree ...");

    DecisionTree<MutableRootContainer> dt = DecisionTree<MutableRootContainer>();
    bool is_regression_tree = args[0].getAs<bool>();
    std::string impurity_func_str = args[1].getAs<std::string>();
    uint16_t n_y_labels = args[2].getAs<uint16_t>();

    if (is_regression_tree)
        n_y_labels = REGRESS_N_STATS;
    dt.rebind(1u, n_y_labels);
    dt.feature_indices(0) = dt.IN_PROCESS_LEAF;
    dt.feature_thresholds(0) = 0;
    dt.is_categorical(0) = 0;
    dt.predictions.row(0).setConstant(0);

    dt.is_regression = is_regression_tree;
    if (dt.is_regression){
        dt.impurity_type = dt.MSE;     // only MSE defined for regression
    } else {
        if ((impurity_func_str.compare("misclassification") == 0) ||
                (impurity_func_str.compare("misclass") == 0))
            dt.impurity_type = dt.MISCLASS;
        else if ((impurity_func_str.compare("entropy") == 0) ||
                 (impurity_func_str.compare("cross-entropy") == 0))
            dt.impurity_type = dt.ENTROPY;
        else
            dt.impurity_type = dt.GINI;  // default impurity for classification
    }

    return dt.storage();
    } // initialize_decision_tree
// ------------------------------------------------------------

AnyType
compute_leaf_stats_transition::run(AnyType & args){
    MutableLevelState state = args[0].getAs<MutableByteString>();
    LevelState::tree_type dt = args[1].getAs<ByteString>();

    // need to change this according to the calling function
    if (state.terminated || args[4].isNull()) {
        return args[0];
    }
    double response = args[4].getAs<double>();
    double weight = args[5].getAs<double>();
    if (weight < 0)
        throw std::runtime_error("Negative weights present in the data");
    NativeIntegerVector cat_features;
    NativeColumnVector con_features;
    try {
        if (args[2].isNull()){
            cat_features.rebind(this->allocateArray<int>(0));
        }
        else {
            NativeIntegerVector xx_cat = args[2].getAs<NativeIntegerVector>();
            cat_features.rebind(xx_cat.memoryHandle(), xx_cat.size());
        }
        if (args[3].isNull()){
            con_features.rebind(this->allocateArray<double>(0));
        }
        else {
            NativeColumnVector xx_con = args[3].getAs<NativeColumnVector>();
            con_features.rebind(xx_con.memoryHandle(), xx_con.size());
        }
    } catch (const ArrayWithNullException &e) {
        return args[0];
    }

    // cat_levels size = num_cat_features
    NativeIntegerVector cat_levels;
    if (args[6].isNull()){
        cat_levels.rebind(this->allocateArray<int>(0));
    }
    else {
        NativeIntegerVector xx_levels = args[6].getAs<NativeIntegerVector>();
        cat_levels.rebind(xx_levels.memoryHandle(), xx_levels.size());
    }

    // con_splits size = num_con_features x num_bins
    ConSplitsResult<RootContainer> splits_results = args[7].getAs<MutableByteString>();

    // n_response_labels are the number of values the dependent variable takes
    uint16_t n_response_labels = args[8].getAs<uint16_t>();
    if (!dt.is_regression && n_response_labels <= 1){
        // for classification, response should have at least two distinct values
        throw std::runtime_error("Invalid response variable for a classification tree. Should have "
                                  "more than one distinct value");
    }

    if (state.empty()){
        state.rebind(static_cast<uint16_t>(splits_results.con_splits.cols()),
                     static_cast<uint16_t>(cat_features.size()),
                     static_cast<uint16_t>(con_features.size()),
                     static_cast<uint32_t>(cat_levels.sum()),
                     static_cast<uint16_t>(dt.tree_depth),
                     dt.is_regression ? REGRESS_N_STATS : n_response_labels
                    );
        // compute cumulative sum of the levels of the categorical variables
        int current_sum = 0;
        for (Index i=0; i < state.num_cat_features; ++i){
            // We assume that the levels of each categorical variable are sorted
            //  by the entropy for predicting the response. We then create splits
            //  of the form 'A <= t', where A has N levels and t in [0, N-2].
            // This split places all levels <= t on true node and
            //  others on false node. We only check till N-2 since we want at
            //  least 1 level falling to the false node.
            // We keep a variable with just 1 level to ensure alignment,
            //  even though that variable will not be used as a split feature.
            assert(cat_levels(i) > 0);
            current_sum += cat_levels(i);
            state.cat_levels_cumsum(i) = current_sum;
        }
    }

    state << MutableLevelState::tuple_type(dt, cat_features, con_features,
                                           response, weight,
                                           cat_levels,
                                           splits_results.con_splits);
    return state.storage();
} // transition function

// ------------------------------------------------------------
AnyType
compute_leaf_stats_merge::run(AnyType & args){
    MutableLevelState stateLeft = args[0].getAs<MutableByteString>();
    LevelState stateRight = args[1].getAs<ByteString>();
    if (stateLeft.empty()) {
        return stateRight.storage();
    }
    if (!stateRight.empty()) {
        stateLeft << stateRight;
    }

   return stateLeft.storage();
} // merge function


AnyType
dt_apply::run(AnyType & args){
    MutableTree dt = args[0].getAs<MutableByteString>();
    LevelState curr_level = args[1].getAs<ByteString>();

    // 0 = running, 1 = finished training, 2 = terminated prematurely
    uint16_t return_code;

    if (!curr_level.terminated){
        ConSplitsResult<RootContainer> con_splits_results =
            args[2].getAs<ByteString>();

        uint16_t min_split = args[3].getAs<uint16_t>();
        uint16_t min_bucket = args[4].getAs<uint16_t>();
        uint16_t max_depth = args[5].getAs<uint16_t>();

        bool finished = dt.expand(curr_level, con_splits_results.con_splits,
                                  min_split, min_bucket, max_depth);
        return_code = finished ? FINISHED : NOT_FINISHED;
    }
    else{
        return_code = TERMINATED;  // indicates termination due to error
    }
    AnyType output_tuple;
    output_tuple << dt.storage() << return_code;
    return output_tuple;
} // apply function

// ------------------------------------------------------------

/*
    @brief Return the probabilities of classes as prediction
*/
AnyType
predict_dt_prob::run(AnyType &args){
    if (args[0].isNull()){
        return Null();
    }
    Tree dt = args[0].getAs<ByteString>();
    NativeIntegerVector cat_features;
    NativeColumnVector con_features;
    try {
        if (args[1].isNull()){
            cat_features.rebind(this->allocateArray<int>(0));
        }
        else {
            NativeIntegerVector xx_cat = args[1].getAs<NativeIntegerVector>();
            cat_features.rebind(xx_cat.memoryHandle(), xx_cat.size());
        }
        if (args[2].isNull()){
            con_features.rebind(this->allocateArray<double>(0));
        }
        else {
            NativeColumnVector xx_con = args[2].getAs<NativeColumnVector>();
            con_features.rebind(xx_con.memoryHandle(), xx_con.size());
        }
    } catch (const ArrayWithNullException &e) {
        return Null();
    }
    ColumnVector prediction = dt.predict(cat_features, con_features);
    return prediction;
}

/*
    @brief Return the regression prediction or class that corresponds to max prediction
*/
AnyType
predict_dt_response::run(AnyType &args){
    if (args[0].isNull()){
        return Null();
    }
    Tree dt = args[0].getAs<ByteString>();
    NativeIntegerVector cat_features;
    NativeColumnVector con_features;
    try {
        if (args[1].isNull()){
            cat_features.rebind(this->allocateArray<int>(0));
        }
        else {
            NativeIntegerVector xx_cat = args[1].getAs<NativeIntegerVector>();
            cat_features.rebind(xx_cat.memoryHandle(), xx_cat.size());
        }
        if (args[2].isNull()){
            con_features.rebind(this->allocateArray<double>(0));
        }
        else {
            NativeColumnVector xx_con = args[2].getAs<NativeColumnVector>();
            con_features.rebind(xx_con.memoryHandle(), xx_con.size());
        }
    } catch (const ArrayWithNullException &e) {
        return Null();
    }
    return dt.predict_response(cat_features, con_features);
}

AnyType
display_decision_tree::run(AnyType &args) {
    Tree dt = args[0].getAs<ByteString>();
    ArrayHandle<text*> cat_feature_names = args[1].getAs<ArrayHandle<text*> >();
    ArrayHandle<text*> con_feature_names = args[2].getAs<ArrayHandle<text*> >();
    ArrayHandle<text*> cat_levels_text = args[3].getAs<ArrayHandle<text*> >();
    ArrayHandle<int> cat_n_levels = args[4].getAs<ArrayHandle<int> >();
    ArrayHandle<text*> dependent_var_levels = args[5].getAs<ArrayHandle<text*> >();
    std::string id_prefix = args[6].getAs<std::string>();

    string tree_str = dt.display(cat_feature_names, con_feature_names,
                                 cat_levels_text, cat_n_levels,
                                 dependent_var_levels, id_prefix);
    return tree_str;
}

AnyType
print_decision_tree::run(AnyType &args){
    Tree dt = args[0].getAs<ByteString>();
    AnyType tuple;
    tuple << static_cast<uint16_t>(dt.tree_depth - 1)
        << dt.feature_indices
        << dt.feature_thresholds
        << dt.is_categorical
        << dt.predictions;
    return tuple;
}

AnyType
display_text_tree::run(AnyType &args){
    Tree dt = args[0].getAs<ByteString>();
    ArrayHandle<text*> cat_feature_names = args[1].getAs<ArrayHandle<text*> >();
    ArrayHandle<text*> con_feature_names = args[2].getAs<ArrayHandle<text*> >();
    ArrayHandle<text*> cat_levels_text = args[3].getAs<ArrayHandle<text*> >();
    ArrayHandle<int> cat_n_levels = args[4].getAs<ArrayHandle<int> >();

    return dt.print(0, cat_feature_names, con_feature_names, cat_levels_text,
                    cat_n_levels, 1u);
}


} // namespace recursive_partitioning
} // namespace modules
} // namespace madlib
