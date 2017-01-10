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
#include <list>
#include <iterator>

#include <dbconnector/dbconnector.hpp>

#include "DT_proto.hpp"
#include "DT_impl.hpp"
#include "ConSplits.hpp"

#include <math.h>       /* fabs */

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
    DecisionTree<MutableRootContainer> dt = DecisionTree<MutableRootContainer>();
    bool is_regression_tree = args[0].getAs<bool>();
    std::string impurity_func_str = args[1].getAs<std::string>();
    uint16_t n_y_labels = args[2].getAs<uint16_t>();
    uint16_t max_n_surr = args[3].getAs<uint16_t>();

    if (is_regression_tree)
        n_y_labels = REGRESS_N_STATS;
    dt.rebind(1u, n_y_labels, max_n_surr, is_regression_tree);
    dt.feature_indices(0) = dt.IN_PROCESS_LEAF;
    dt.feature_thresholds(0) = 0;
    dt.is_categorical(0) = 0;
    if (max_n_surr > 0){
        dt.surr_indices.setConstant(dt.SURR_NON_EXISTING);
        dt.surr_thresholds.setConstant(0);
        dt.surr_status.setConstant(0);
    }
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

////////////////////////////////////////////////////////////////
// Functions to capture leaf stats for picking primary splits //
////////////////////////////////////////////////////////////////

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

    // cat_levels size = n_cat_features
    NativeIntegerVector cat_levels;
    if (args[6].isNull()){
        cat_levels.rebind(this->allocateArray<int>(0));
    }
    else {
        MutableNativeIntegerVector xx_cat = args[6].getAs<MutableNativeIntegerVector>();
        for (Index i = 0; i < xx_cat.size(); i++)
            xx_cat[i] -= 1;   // ignore the last level since a split
                              // like 'var <= last level' would move all rows to
                              // a one side. Such a split will always be ignored
                              // when selecting the best split.
        cat_levels.rebind(xx_cat.memoryHandle(), xx_cat.size());
    }

    // con_splits size = num_con_features x num_bins
    // When num_con_features = 0, the input will be an empty string that is read
    // as a ByteString
    ConSplitsResult<RootContainer> splits_results = args[7].getAs<ByteString>();

    // n_response_labels are the number of values the dependent variable takes
    uint16_t n_response_labels = args[8].getAs<uint16_t>();
    if (!dt.is_regression && n_response_labels <= 1){
        // for classification, response should have at least two distinct values
        throw std::runtime_error("Invalid response variable for a classification"
                                 "tree. Should have "
                                 "more than one distinct value");
    }

    if (state.empty()){
        // For classification, we store for each split the number of weighted
        // tuples for each possible response value and the number of unweighted
        // tuples landing on that node.
        // For regression, REGRESS_N_STATS determines the number of stats per split
        uint16_t stats_per_split = dt.is_regression ?
            REGRESS_N_STATS : static_cast<uint16_t>(n_response_labels + 1);
        const bool weights_as_rows = args[9].getAs<bool>();
        state.rebind(static_cast<uint16_t>(splits_results.con_splits.cols()),
                     static_cast<uint16_t>(cat_features.size()),
                     static_cast<uint16_t>(con_features.size()),
                     static_cast<uint32_t>(cat_levels.sum()),
                     static_cast<uint16_t>(dt.tree_depth),
                     stats_per_split,
                     weights_as_rows
                    );
        // compute cumulative sum of the levels of the categorical variables
        int current_sum = 0;
        for (Index i=0; i < state.n_cat_features; ++i){
            // We assume that the levels of each categorical variable are sorted
            //  by the entropy for predicting the response. We then create splits
            //  of the form 'A <= t', where A has N levels and t in [0, N-2].
            // This split places all levels <= t on true node and
            //  others on false node. We only check till N-2 since we want at
            //  least 1 level falling to the false node.
            // We keep a variable with just 1 level to ensure alignment,
            //  even though that variable will not be used as a split feature.
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
        ConSplitsResult<RootContainer> con_splits_results = args[2].getAs<ByteString>();
        uint16_t min_split = args[3].getAs<uint16_t>();
        uint16_t min_bucket = args[4].getAs<uint16_t>();
        uint16_t max_depth = args[5].getAs<uint16_t>();
        bool subsample = args[6].getAs<bool>();
        int num_random_features = args[7].getAs<int>();

        bool finished = false;
        if (!subsample) {
            finished = dt.expand(curr_level, con_splits_results.con_splits,
                                 min_split, min_bucket, max_depth);
        } else {
            finished = dt.expand_by_sampling(curr_level, con_splits_results.con_splits,
                                             min_split, min_bucket, max_depth,
                                             num_random_features);
        }
        return_code = finished ? FINISHED : NOT_FINISHED;
    }
    else{
        return_code = TERMINATED;  // indicates termination due to error
    }

    AnyType output_tuple;
    output_tuple << dt.storage() << return_code << static_cast<uint16_t>(dt.tree_depth - 1);
    return output_tuple;
} // apply function
// -------------------------------------------------------------------------

///////////////////////////////////////////////
// Functions to capture surrogate statistics //
///////////////////////////////////////////////

AnyType
compute_surr_stats_transition::run(AnyType & args){
    MutableLevelState state = args[0].getAs<MutableByteString>();
    LevelState::tree_type dt = args[1].getAs<ByteString>();

    // need to change this according to the calling function
    if (state.terminated) {
        return args[0];
    }
    NativeIntegerVector cat_features;
    if (args[2].isNull()){
            cat_features.rebind(this->allocateArray<int>(0));
    } else {
        NativeIntegerVector xx_cat = args[2].getAs<NativeIntegerVector>();
        cat_features.rebind(xx_cat.memoryHandle(), xx_cat.size());
    }
    NativeColumnVector con_features;
    if (args[3].isNull()){
        con_features.rebind(this->allocateArray<double>(0));
    } else {
        NativeColumnVector xx_con = args[3].getAs<NativeColumnVector>();
        con_features.rebind(xx_con.memoryHandle(), xx_con.size());
    }

    // cat_levels size = n_cat_features
    NativeIntegerVector cat_levels;
    if (args[4].isNull()){
        cat_levels.rebind(this->allocateArray<int>(0));
    }
    else {
        MutableNativeIntegerVector xx_cat = args[4].getAs<MutableNativeIntegerVector>();
        for (Index i = 0; i < xx_cat.size(); i++)
            xx_cat[i] -= 1;   // ignore the last level
        cat_levels.rebind(xx_cat.memoryHandle(), xx_cat.size());
    }

    // con_splits size = n_con_features x n_bins
    ConSplitsResult<RootContainer> splits_results = args[5].getAs<MutableByteString>();

    // tree_depth = 1 implies a single leaf node in the tree.
    // We compute surrogates only for internal nodes. Hence we need
    // the root be an internal node i.e. we need the tree_depth to be more than 1.
    if (dt.tree_depth > 1){
        if (state.empty()){
            // 1. We need to compute stats for parent of each leaf.
            //      Hence the tree_depth is decremented by 1.
            // 2. We store 2 values for each surrogate split
            //       1st value is for <= split; 2nd value is for > split
            //       (hence stats_per_split = 2)
            state.rebind(static_cast<uint16_t>(splits_results.con_splits.cols()),
                         static_cast<uint16_t>(cat_features.size()),
                         static_cast<uint16_t>(con_features.size()),
                         static_cast<uint32_t>(cat_levels.sum()),
                         static_cast<uint16_t>(dt.tree_depth - 1),
                         2,
                         false // dummy, only used in compute_leaf_stat
                        );
            // compute cumulative sum of the levels of the categorical variables
            int current_sum = 0;
            for (Index i=0; i < state.n_cat_features; ++i){
                current_sum += cat_levels(i);
                state.cat_levels_cumsum(i) = current_sum;
            }
        }

        const int dup_count = args[6].getAs<int>();
        state << MutableLevelState::surr_tuple_type(
            dt, cat_features, con_features, cat_levels, splits_results.con_splits, dup_count);
    }
    return state.storage();
}

AnyType
dt_surr_apply::run(AnyType & args){
    MutableTree dt = args[0].getAs<MutableByteString>();
    LevelState curr_level_surr = args[1].getAs<ByteString>();
    if (!curr_level_surr.terminated && dt.max_n_surr > 0){
        ConSplitsResult<RootContainer> con_splits_results =
            args[2].getAs<ByteString>();
        dt.pickSurrogates(curr_level_surr, con_splits_results.con_splits);
    }
    return dt.storage();
} // apply function
// -------------------------------------------------------------------------

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

// -------------------------------------------------------------------------

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
        // reach here only if surrogates are not used
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
    bool verbose = args[7].getAs<bool>();

    string tree_str = dt.display(cat_feature_names, con_feature_names,
                                 cat_levels_text, cat_n_levels,
                                 dependent_var_levels, id_prefix, verbose);
    return tree_str;
}

AnyType
display_decision_tree_surrogate::run(AnyType &args) {
    Tree dt = args[0].getAs<ByteString>();
    ArrayHandle<text*> cat_feature_names = args[1].getAs<ArrayHandle<text*> >();
    ArrayHandle<text*> con_feature_names = args[2].getAs<ArrayHandle<text*> >();
    ArrayHandle<text*> cat_levels_text = args[3].getAs<ArrayHandle<text*> >();
    ArrayHandle<int> cat_n_levels = args[4].getAs<ArrayHandle<int> >();

    return dt.surr_display(cat_feature_names, con_feature_names,
                           cat_levels_text, cat_n_levels);
}

AnyType
print_decision_tree::run(AnyType &args){
    Tree dt = args[0].getAs<ByteString>();
    AnyType tuple;
    tuple << static_cast<uint16_t>(dt.tree_depth - 1)
        << dt.feature_indices
        << dt.feature_thresholds
        << dt.is_categorical
        << dt.predictions
        << dt.surr_indices
        << dt.surr_thresholds
        << dt.surr_status;
    return tuple;
}

AnyType
display_text_tree::run(AnyType &args){
    Tree dt = args[0].getAs<ByteString>();
    ArrayHandle<text*> cat_feature_names = args[1].getAs<ArrayHandle<text*> >();
    ArrayHandle<text*> con_feature_names = args[2].getAs<ArrayHandle<text*> >();
    ArrayHandle<text*> cat_levels_text = args[3].getAs<ArrayHandle<text*> >();
    ArrayHandle<int> cat_n_levels = args[4].getAs<ArrayHandle<int> >();
    ArrayHandle<text*> dep_levels = args[5].getAs<ArrayHandle<text*> >();

    return dt.print(0, cat_feature_names, con_feature_names, cat_levels_text,
                    cat_n_levels, dep_levels, 1u);
}

// ------------------------------------------------------------
// Prune the tree model using cost-complexity parameter
// ------------------------------------------------------------

// Remove me's subtree and make it a leaf
void mark_subtree_removal_recur(MutableTree &dt, int me) {
    if (me < dt.predictions.rows() &&
            dt.feature_indices(me) != dt.NODE_NON_EXISTING) {
        int left = static_cast<int>(dt.trueChild(static_cast<Index>(me))),
            right = static_cast<int>(dt.falseChild(static_cast<Index>(me)));
        mark_subtree_removal_recur(dt, left);
        mark_subtree_removal_recur(dt, right);
        dt.feature_indices(me) = dt.NODE_NON_EXISTING;
    }
}

void mark_subtree_removal(MutableTree &dt, int me) {
    mark_subtree_removal_recur(dt, me);
    dt.feature_indices(me) = dt.FINISHED_LEAF;
}

// ------------------------------------------------------------

/*
 * Data structure that contains the info for lower sub-trees
 */
struct SubTreeInfo {
    /* id of the root node of the subtree */
    int root_id;

    /* number of node splits */
    int n_split;
    /* current node's own risk */
    double risk;
    /*
     * accumulated risk of sub-tree with the current
     * node being the root
     */
    double sum_risk;
    /* sub-tree's average risk improvement per split */
    double complexity;

    SubTreeInfo * left_child;
    SubTreeInfo * right_child;

    SubTreeInfo(int i, int n, double r, double s, double c):
            root_id(i), n_split(n), risk(r), sum_risk(s), complexity(c){
        left_child = NULL;
        right_child = NULL;
    }
};


// FIXME: Remove after finalzing code
template <class IterableContainer>
string print_debug_list(IterableContainer debug_list){
    std::stringstream debug;
    typename IterableContainer::iterator it;
    for (it = debug_list.begin(); it != debug_list.end(); it++){
        debug << std::setprecision(8) << *it << ", ";
    }
    return debug.str();
}


/*
 * Pruning the tree by setting the pruned nodes'
 * feature_indices value to be NODE_NON_EXISTING.
 *
 * Closely follow rpart's implementation. Please read the
 * source code of rpart/src/partition.c
 */
SubTreeInfo prune_tree(MutableTree &dt,
                       int me,
                       double alpha,
                       double estimated_complexity,
                       std::vector<double> & node_complexities) {
    if (me >= dt.feature_indices.size() || /* out of range */
            dt.feature_indices(me) == dt.NODE_NON_EXISTING)
        return SubTreeInfo(-1, 0, 0, 0, 0);

    double risk = dt.computeRisk(me);

    double adjusted_risk = risk > estimated_complexity ?
                            estimated_complexity : risk;
    if (adjusted_risk <= alpha) {
        /* If the current node's risk is smaller than alpha, then the risk can
         * never decrease more than alpha by splitting. Remove the current
         * node's subtree and make the current node a leaf.
         */
        mark_subtree_removal(dt, me);
        node_complexities[me] = alpha;
        return SubTreeInfo(me, 0, risk, risk, alpha);
    }

    if (dt.feature_indices(me) >= 0) {
        SubTreeInfo left = prune_tree(dt, 2*me+1, alpha,
                                      adjusted_risk - alpha,
                                      node_complexities);
        double left_improve_per_split = (risk - left.sum_risk) / (left.n_split + 1);
        double left_child_improve = risk - left.risk;
        if (left_improve_per_split < left_child_improve)
            left_improve_per_split = left_child_improve;
        adjusted_risk = left_improve_per_split > estimated_complexity ?
                        estimated_complexity : left_improve_per_split;

        SubTreeInfo right = prune_tree(dt, 2*me+2, alpha, adjusted_risk - alpha,
                                       node_complexities);
        /*
         * Closely follow rpart's algorithm, in rpart/src/partition.c
         *
         * If the average improvement of risk per split is larger
         * than the sub-tree's average improvement, the current
         * split is important. And we need to manually increase
         * the value of the current split's improvement, which
         * aims at keeping the current split if possible.
         */
        double left_risk = left.sum_risk;
        double right_risk = right.sum_risk;
        int left_n_split = left.n_split;
        int right_n_split = right.n_split;

        double tempcp = (risk - (left_risk + right_risk)) /
                            (left_n_split + right_n_split + 1);

        if (right.complexity > left.complexity) {
            if (tempcp > left.complexity) {
                left_risk = left.risk;
                left_n_split = 0;

                tempcp = (risk - (left_risk + right_risk)) /
                    (left_n_split + right_n_split + 1);
                if (tempcp > right.complexity) {
                    right_risk = right.risk;
                    right_n_split = 0;
                }
            }
        } else if (tempcp > right.complexity) {
            right_risk = right.risk;
            right_n_split = 0;

            tempcp = (risk - (left_risk + right_risk)) /
                (left_n_split + right_n_split + 1);
            if (tempcp > left.complexity) {
                left_risk = left.risk;
                left_n_split = 0;
            }
        }

        double complexity = (risk - (left_risk + right_risk)) /
                                (left_n_split + right_n_split + 1);
        if (complexity <= alpha) {
            /* Prune this split by removing the subtree */
            mark_subtree_removal(dt, me);
            node_complexities[me] = alpha;
            return SubTreeInfo(me, 0, risk, risk, alpha);
        } else {
            node_complexities[me] = complexity;
            return SubTreeInfo(me, left_n_split + right_n_split + 1,
                               risk, left_risk + right_risk, complexity);
        }
    } // end of if (dt.feature_indices(me) >= 0)
    else {
        // node is a leaf node
        node_complexities[me] = alpha;
        return SubTreeInfo(me, 0, risk, risk, alpha);
    }
}
// ------------------------------------------------------------

void make_cp_list(MutableTree & dt,
                  std::vector<double> & node_complexities,
                  const double & alpha,
                  std::list<double> & cp_list,
                  const double root_risk){

    cp_list.clear();
    cp_list.push_back(node_complexities[0] / root_risk);
    for (uint i = 1; i < node_complexities.size(); i++){
        Index parent_id = dt.parentIndex(i);
        if (dt.feature_indices(i) != dt.NODE_NON_EXISTING &&
                dt.feature_indices(parent_id) != dt.NODE_NON_EXISTING){
            double parent_cp = node_complexities[parent_id];
            if (node_complexities[i] > parent_cp)
                node_complexities[i] = parent_cp;
            double current_cp = node_complexities[i];
            if (current_cp < alpha)
                current_cp = alpha;  // don't explore any cp less than alpha
            if (current_cp < parent_cp){
                // original complexity is scaled by root_risk. But user
                // expects an unscaled cp value
                current_cp /= root_risk;
                std::list<double>::iterator it;
                bool skip_cp = false;
                for (it = cp_list.begin(); !skip_cp && it != cp_list.end(); it++){
                    if (fabs(current_cp - *it) < 1e-4)
                        skip_cp = true;
                    if (current_cp > *it)
                        break;
                }
                if (!skip_cp)
                    cp_list.insert(it, current_cp);
            }
        }
    }
}
// -------------------------------------------------------------------------

AnyType
prune_and_cplist::run(AnyType &args){
    MutableTree dt = args[0].getAs<MutableByteString>();
    double cp = args[1].getAs<double>();
    bool compute_cp_list = args[2].getAs<bool>();

    // We use a scaled version of risk (similar to rpart's definition).
    //    The risk is relative to a tree with no splits (single node tree).
    double root_risk = dt.computeRisk(0);
    double alpha = cp * root_risk;
    std::vector<double> node_complexities(dt.feature_indices.size(), alpha);

    prune_tree(dt, 0, alpha, root_risk, node_complexities);
    // Get the new tree_depth after pruning
    //  Note: externally, tree_depth starts from 0 but DecisionTree assumes
    //  tree_depth starts from 1
    uint16_t pruned_depth = static_cast<uint16_t>(dt.recomputeTreeDepth() - 1);

    AnyType output_tuple;
    if (compute_cp_list){
        std::list<double> cp_list;
        make_cp_list(dt, node_complexities, alpha, cp_list, root_risk);
        // we copy to a vector since we currently only have << operator
        // defined for a vector
        ColumnVector cp_vector(cp_list.size());
        std::list<double>::iterator it = cp_list.begin();
        for (Index i = 0; it != cp_list.end(); it++, i++){
            cp_vector(i) = *it;
        }
        output_tuple << dt.storage() << pruned_depth << cp_vector;
    } else {
        output_tuple << dt.storage() << pruned_depth;
    }
    return output_tuple;
}

// ------------------------------------------------------------

// Helper function for PivotalR
// Convert the result into rpart's frame item in the result

/*
 * Fil a row of the frame matrix using data in tree
 */
void fill_row(MutableNativeMatrix &frame, Tree &dt, int me, int i, int n_cats) {
    frame(i,0) = static_cast<double>(dt.encodeIndex(dt.feature_indices(me),
                dt.is_categorical(me), n_cats));
    frame(i,5) = 1; // complexity is not needed in plotting
    frame(i,6) = 0; // ncompete is not needed in plotting

    // How many surrogate variables have been computed for this split
    int n_surrogates = 0;
    for (int ii = 0; ii < dt.max_n_surr; ii ++) {
        if (dt.surr_indices(me * dt.max_n_surr + ii) >= 0) {
            n_surrogates ++;
        }
    }
    frame(i,7) = n_surrogates;

    if (dt.is_regression) {
        frame(i,1) = dt.predictions(me,3); // n
        frame(i,2) = dt.predictions(me,0); // wt
        frame(i,3) = dt.computeRisk(me); // weighted variance
        frame(i,4) = dt.predictions(me, 1) / dt.predictions(me,0); // yval
    } else {
        double total_records = dt.nodeWeightedCount(0);
        double n_records_innode = static_cast<double>(dt.nodeCount(me));
        double n_records_weighted_innode = dt.nodeWeightedCount(me);
        int n_dep_levels = static_cast<int>(dt.n_y_labels);

        // FIXME use weight sum as the total number
        frame(i,1) = n_records_innode;
        frame(i,2) = n_records_weighted_innode;
        frame(i,3) = dt.computeMisclassification(me);

        Index max_index;
        dt.predictions.row(me).head(dt.n_y_labels).maxCoeff(&max_index);
        // start from 1 to be consistent with R convention
        frame(i,4) = static_cast<double>(max_index + 1);
        frame(i,8) = frame(i,4);
        for (int j = 0; j < n_dep_levels; ++j) {
            frame(i,9 + j) = dt.predictions(me, j);
            frame(i,9 + j + n_dep_levels) =
                dt.predictions(me, j) / n_records_innode;
        }
        frame(i,9 + 2 * n_dep_levels) = n_records_innode / total_records;
    }
}


// ------------------------------------------------------------

/*
 * Recursively transverse the tree in a depth first way,
 * and fill all row of frame at the same time
 */
void transverse_tree(Tree &dt, MutableNativeMatrix &frame,
        int me, int &row, int n_cats) {
    if (me < dt.feature_indices.size() && dt.feature_indices(me) != dt.NODE_NON_EXISTING) {
        fill_row(frame, dt, me, row++, n_cats);
        transverse_tree(dt, frame, static_cast<int>(dt.falseChild(me)), row, n_cats);
        transverse_tree(dt, frame, static_cast<int>(dt.trueChild(me)), row, n_cats);
    }
}

// ------------------------------------------------------------

AnyType convert_to_rpart_format::run(AnyType &args) {
    Tree dt = args[0].getAs<ByteString>();
    int n_cats = args[1].getAs<int>();

    // number of nodes in the tree
    int n_nodes = 0;
    for (int i = 0; i < dt.feature_indices.size(); ++i) {
        if (dt.feature_indices(i) != dt.NODE_NON_EXISTING) n_nodes++;
    }

    // number of columns in rpart frame
    int n_col;
    if (dt.is_regression) {
        n_col = 8;
    } else {
        n_col = 10 + 2 * dt.n_y_labels;
    }

    MutableNativeMatrix frame(this->allocateArray<double>(
                n_col, n_nodes), n_nodes, n_col);

    int row = 0;
    transverse_tree(dt, frame, 0, row, n_cats);

    return frame;
}

// transverse the tree to get the internal nodes' split thresholds
void
transverse_tree_thresh(const Tree &dt, MutableNativeMatrix &thresh,
        int me, int &row, int n_cats) {
    // only for non-leaf nodes
    if (dt.feature_indices(me) >= 0) {
        // primary
        thresh(row,0) = dt.encodeIndex(dt.feature_indices(me),
                                       dt.is_categorical(me),
                                       n_cats);
        thresh(row,1) = dt.feature_thresholds(me);
        row++;

        // surrogates
        for (int ii = 0; ii < dt.max_n_surr; ii ++) {
            int surr_ii = me * dt.max_n_surr + ii;
            if (dt.surr_indices(surr_ii) >= 0) {
                thresh(row,0) = dt.encodeIndex(dt.surr_indices(surr_ii),
                                                dt.surr_status(surr_ii) == 1 || dt.surr_status(surr_ii) == -1,
                                                n_cats);
                thresh(row,1) = dt.surr_thresholds(surr_ii);
                row++;
            }
        }

        transverse_tree_thresh(dt, thresh, static_cast<int>(dt.falseChild(me)), row, n_cats);
        transverse_tree_thresh(dt, thresh, static_cast<int>(dt.trueChild(me)), row, n_cats);
    }
}

// ------------------------------------------------------------

AnyType
get_split_thresholds::run(AnyType &args) {
    Tree dt = args[0].getAs<ByteString>();
    int n_cats = args[1].getAs<int>();
    // number of internal nodes
    int in_nodes = 0;
    // count how many surrogate variables in the whole tree
    int tot_surr_n = 0;
    for (int i = 0; i < dt.feature_indices.size(); ++i) {
        if (dt.feature_indices(i) >= 0) {
            in_nodes++;
            for (int ii = 0; ii < dt.max_n_surr; ii ++) {
                if (dt.surr_indices(i * dt.max_n_surr + ii) >= 0) {
                    tot_surr_n ++;
                }
            }
        }
    }

    MutableNativeMatrix thresh(
            this->allocateArray<double>(2, in_nodes + tot_surr_n),
            in_nodes + tot_surr_n,
            2);
    int row = 0;
    transverse_tree_thresh(dt, thresh, 0, row, n_cats);
    return thresh;
    }

// ------------------------------------------------------------

/*
 * PivotalR: randomForest
 * Fil a row of the frame matrix using data in tree
 */
void fill_one_row(MutableNativeMatrix &frame, Tree &dt, int me, int i,
        int &node_index) {
    int feature_index = dt.feature_indices(me);
    if (feature_index == dt.FINISHED_LEAF) {
        frame(i,0) = 0;
        frame(i,1) = 0;
        frame(i,2) = 0;
        frame(i,4) = -1;
        node_index--;
    } else {
        frame(i,0) = node_index * 2;
        frame(i,1) = node_index * 2 + 1;
        frame(i,4) = 1;
    }
    frame(i,2) = feature_index;
    frame(i,3) = dt.feature_thresholds(me);

    if (dt.is_regression) {
        frame(i,5) = dt.predictions(me,1) / dt.predictions(me,0); // yval
    } else {
        Index max_index;
        dt.predictions.row(me).head(dt.n_y_labels).maxCoeff(&max_index);
        // start from 1 to be consistent with R convention
        frame(i,5) = static_cast<double>(max_index + 1);
    }
}

/*
 * PivotalR: randomForest
 * Convert to R's randomForest format for getTree(..) function
 *
 */
AnyType
convert_to_random_forest_format::run(AnyType &args) {
    Tree dt = args[0].getAs<ByteString>();

    // number of nodes in the tree
    int n_nodes = 0;
    for (int i = 0; i < dt.feature_indices.size(); ++i) {
        if (dt.feature_indices(i) != dt.NODE_NON_EXISTING) n_nodes++;
    }

    // number of columns in randomForest frame

    MutableNativeMatrix frame(this->allocateArray<double>(
                6, n_nodes), n_nodes, 6);

    int row = 0;
    int node_index = 1;
    for (int i = 0; i<n_nodes; i++,node_index++,row++) {
        fill_one_row(frame, dt, i, row, node_index);
    }

    return frame;
}
// ------------------------------------------------------------

} // namespace recursive_partitioning
} // namespace modules
} // namespace madlib
