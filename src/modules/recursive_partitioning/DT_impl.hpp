/* ----------------------------------------------------------------------- *//**
 *
 * @file Decision_Tree_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_RP_DT_IMPL_HPP
#define MADLIB_MODULES_RP_DT_IMPL_HPP

#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <iterator>

#include <limits>  // std::numeric_limits

#include <dbconnector/dbconnector.hpp>
#include "DT_proto.hpp"

namespace madlib {

// Use Eigen
using namespace dbal::eigen_integration;

namespace modules {

namespace recursive_partitioning {

namespace {

string
escape_quotes(const string &before) {
    // From http://stackoverflow.com/questions/1162619/fastest-quote-escaping-implementation
    string after;
    after.reserve(before.length() + 4);

    for (string::size_type i = 0; i < before.length(); ++i) {
        switch (before[i]) {
            case '"':
            case '\\':
                after += '\\';
                // Fall through.
            default:
                after += before[i];
        }
    }
    return after;
}
// ------------------------------------------------------------

double
computeEntropy(const double &p) {
    if (p < 0.) { throw std::runtime_error("unexpected negative probability"); }
    if (p == 0.) { return 0.; }
    return -p * log2(p);
}
// ------------------------------------------------------------

// Extract a string from ArrayHandle<text*>
inline
string
get_text(ArrayHandle<text*> &strs, size_t i) {
    return std::string(VARDATA_ANY(strs[i]), VARSIZE_ANY(strs[i]) - VARHDRSZ);
}

} // anonymous namespace

// ------------------------------------------------------------------------
// Definitions for class Decision Tree
// ------------------------------------------------------------------------

//TODO: Check if this default constructor works
template <class Container>
inline
DecisionTree<Container>::DecisionTree():
        Base(defaultAllocator().allocateByteString<
                dbal::FunctionContext, dbal::DoZero, dbal::ThrowBadAlloc>(0)) {
    this->initialize();
}
// ------------------------------------------------------------

template <class Container>
inline
DecisionTree<Container>::DecisionTree(
        Init_type& inInitialization): Base(inInitialization) {
    this->initialize();
}
// ------------------------------------------------------------

template <class Container>
inline
void
DecisionTree<Container>::bind(ByteStream_type& inStream) {
    std::stringstream help;
    inStream >> tree_depth
             >> is_regression
             >> impurity_type
             >> n_y_labels;

    size_t n_nodes = 0;
    size_t n_labels = 0;
    if (!tree_depth.isNull()) {
        n_nodes = static_cast<size_t>(pow(2, tree_depth) - 1);

        // for classification n_labels = n_y_labels + 1 since the last element
        // is the count of actual (unweighted) tuples landing on a node
        if (is_regression)
            n_labels = static_cast<size_t>(n_y_labels);
        else
            n_labels = static_cast<size_t>(n_y_labels + 1);
    }

    inStream
        >> feature_indices.rebind(n_nodes)
        >> feature_thresholds.rebind(n_nodes)
        >> is_categorical.rebind(n_nodes)
        >> predictions.rebind(n_nodes, n_labels)
        ;
}
// ------------------------------------------------------------

template <class Container>
inline
DecisionTree<Container>&
DecisionTree<Container>::rebind(const uint16_t in_tree_depth,
                                const uint16_t in_y_labels) {
    tree_depth = in_tree_depth;
    n_y_labels = in_y_labels;
    this->resize();
    return *this;
}
// ------------------------------------------------------------

template <class Container>
inline
DecisionTree<Container>&
DecisionTree<Container>::incrementInPlace() {
    // back up current tree
    size_t n_orig_nodes = static_cast<size_t>(pow(2, tree_depth) - 1);
    DecisionTree<Container> orig = DecisionTree<Container>();
    orig.tree_depth = tree_depth;
    orig.resize();
    orig.copy(*this);

    // increment one level
    tree_depth ++;
    this->resize();

    // restore from backup
    is_regression = orig.is_regression;
    impurity_type = orig.impurity_type;
    feature_indices.segment(0, n_orig_nodes) = orig.feature_indices;
    feature_thresholds.segment(0, n_orig_nodes) = orig.feature_thresholds;
    is_categorical.segment(0, n_orig_nodes) = orig.is_categorical;
    for (Index i = 0; i < orig.predictions.rows(); i++){
        // resize adds rows at the end of predictions
        predictions.row(i) = orig.predictions.row(i);
    }
    // mark all newly allocated leaves as non-existing nodes, they will be
    //  categorized as leaf nodes by the parent during expansion
    size_t n_new_leaves = n_orig_nodes + 1;
    feature_indices.segment(n_orig_nodes, n_new_leaves).setConstant(NON_EXISTING);
    feature_thresholds.segment(n_orig_nodes, n_new_leaves).setConstant(0);
    is_categorical.segment(n_orig_nodes, n_new_leaves).setConstant(0);
    for (size_t i = n_orig_nodes; i < n_orig_nodes + n_new_leaves; i++){
        predictions.row(i).setConstant(0);
    }

    return *this;
}
// ------------------------------------------------------------

template <class Container>
inline
Index
DecisionTree<Container>::search(MappedIntegerVector cat_features,
                                MappedColumnVector con_features) const {
    Index current = 0;
    int feature_index = feature_indices(current);
    while (feature_index != IN_PROCESS_LEAF &&
            feature_index != FINISHED_LEAF) {
        assert(feature_index != NON_EXISTING);
        bool is_split_true = false;
        if (is_categorical(current) != 0) {
            is_split_true = (cat_features(feature_index)
                             <= feature_thresholds(current));
        } else {
            is_split_true = (con_features(feature_index)
                             <= feature_thresholds(current));
        }
        /*       (i)
               /     \
           (2i+1)  (2i+2)
         */
        current = is_split_true ? trueChild(current) : falseChild(current);
        feature_index = feature_indices(current);
    }
    return current;
}
// ------------------------------------------------------------

template <class Container>
inline
ColumnVector
DecisionTree<Container>::predict(MappedIntegerVector cat_features,
                                 MappedColumnVector con_features) const {
    Index leaf_index = search(cat_features, con_features);
    return statPredict(predictions.row(leaf_index));
}
// ------------------------------------------------------------

template <class Container>
inline
double
DecisionTree<Container>::predict_response(
        MappedIntegerVector cat_features,
        MappedColumnVector con_features) const {
    ColumnVector curr_prediction = predict(cat_features, con_features);
    if (is_regression){
        return curr_prediction(0);
    } else {
        Index max_label;
        curr_prediction.maxCoeff(&max_label);
        return static_cast<double>(max_label);
    }
}
// ------------------------------------------------------------

template <class Container>
inline
double
DecisionTree<Container>::predict_response(Index leaf_index) const {
    ColumnVector curr_prediction = statPredict(predictions.row(leaf_index));
    if (is_regression){
        return curr_prediction(0);
    } else {
        Index max_label;
        curr_prediction.maxCoeff(&max_label);
        return static_cast<double>(max_label);
    }
}
// ------------------------------------------------------------

template <class Container>
inline
double
DecisionTree<Container>::impurity(const ColumnVector &stats) const {
    if (is_regression){
        // only mean-squared error metric is supported
        // variance is a measure of the mean-squared distance to all points
        return stats(2) / stats(0) - pow(stats(1) / stats(0), 2);
    } else {
        ColumnVector proportions = statPredict(stats);
        if (impurity_type == GINI){
            return 1 - proportions.cwiseProduct(proportions).sum();
        } else if (impurity_type == ENTROPY){
            return proportions.unaryExpr(std::ptr_fun(computeEntropy)).sum();
        } else if (impurity_type == MISCLASS){
            return 1 - proportions.maxCoeff();
        } else
            throw std::runtime_error("No impurity function set for a classification tree");
    }
}
// ------------------------------------------------------------

template <class Container>
inline
double
DecisionTree<Container>::impurityGain(const ColumnVector &combined_stats,
                                      const uint16_t &stats_per_split) const {

    double true_count = statWeightedCount(combined_stats.segment(0, stats_per_split));
    double false_count = statWeightedCount(combined_stats.segment(stats_per_split, stats_per_split));
    double total_count = true_count + false_count;

    if (true_count == 0 || false_count == 0) {
        // no gain if all fall into one side
        return 0.;
    }
    double true_weight = static_cast<double>(true_count) / total_count;
    double false_weight = static_cast<double>(false_count) / total_count;
    ColumnVector stats_sum = combined_stats.segment(0, stats_per_split) +
            combined_stats.segment(stats_per_split, stats_per_split);
    return (impurity(stats_sum) -
            true_weight * impurity(combined_stats.segment(0, stats_per_split)) -
            false_weight *
                impurity(combined_stats.segment(stats_per_split, stats_per_split))
            );
}

// ------------------------------------------------------------

template <class Container>
template <class Accumulator>
inline
bool
DecisionTree<Container>::expand(const Accumulator &state,
                                const MappedMatrix &con_splits,
                                const uint16_t &min_split,
                                const uint16_t &min_bucket,
                                const uint16_t &max_depth) {

    uint16_t num_non_leaf_nodes = static_cast<uint16_t>(state.num_leaf_nodes - 1);
    bool children_not_allocated = true;
    bool all_leaf_small = true;
    bool all_leaf_pure = true;

    const uint16_t &sps = state.stats_per_split;  // short form for brevity
    for (Index i=0; i < state.num_leaf_nodes; i++) {
        Index current = num_non_leaf_nodes + i;
        if (feature_indices(current) == IN_PROCESS_LEAF) {
            // if a leaf node exists, compute the gain in impurity for each split
            // pick split  with maximum gain and update node with split value
            int max_feat = -1;
            Index max_bin = -1;
            bool max_is_cat = false;
            double max_impurity_gain = -std::numeric_limits<double>::infinity();
            ColumnVector max_stats;

            // go through all categorical stats
            int cumsum = 0;
            for (int f=0; f < state.num_cat_features; ++f){ // each feature
                for (int v=0; cumsum < state.cat_levels_cumsum(f); ++v, ++cumsum){
                    // each value of feature
                    Index fv_index = state.indexCatStats(f, v, true);
                    double gain = impurityGain(
                        state.cat_stats.row(i).segment(fv_index, sps * 2), sps);
                    if (gain > max_impurity_gain){
                        max_impurity_gain = gain;
                        max_feat = f;
                        max_bin = v;
                        max_is_cat = true;
                        max_stats = state.cat_stats.row(i).segment(fv_index,
                                                                   sps * 2);
                    }
                }
            }

            // go through all continuous stats
            for (int f=0; f < state.num_con_features; ++f){  // each feature
                for (Index b=0; b < state.num_bins; ++b){
                    // each bin of feature
                    Index fb_index = state.indexConStats(f, b, true);
                    double gain = impurityGain(
                        state.con_stats.row(i).segment(fb_index, sps * 2), sps);
                    if (gain > max_impurity_gain){
                        max_impurity_gain = gain;
                        max_feat = f;
                        max_bin = b;
                        max_is_cat = false;
                        max_stats = state.con_stats.row(i).segment(fb_index,
                                                                   sps * 2);
                    }
                }
            }

            // create and update child nodes if splitting current
            if (max_impurity_gain > 0 &&
                    shouldSplit(max_stats, min_split, min_bucket, sps)) {

                if (children_not_allocated) {
                    // allocate the memory for child nodes if not allocated already
                    incrementInPlace();
                    children_not_allocated = false;
                }

                // current node
                feature_indices(current) = static_cast<int>(max_feat);
                is_categorical(current) = max_is_cat ? 1 : 0;
                if (max_is_cat) {
                    feature_thresholds(current) =
                            static_cast<double>(max_bin);
                } else {
                    feature_thresholds(current) = con_splits(max_feat, max_bin);
                }

                // update prediction for children
                if (current == 0){
                    // since prediction is updated by parent node, we need special
                    // handling for root node (index = 0)
                    predictions.row(current) =
                        max_stats.segment(0, sps) + max_stats.segment(sps, sps);
                }
                feature_indices(trueChild(current)) = IN_PROCESS_LEAF;
                predictions.row(trueChild(current)) = max_stats.segment(0, sps);
                feature_indices(falseChild(current)) = IN_PROCESS_LEAF;
                predictions.row(falseChild(current)) = max_stats.segment(sps, sps);

                if (all_leaf_pure){
                    // verify if current's children are pure
                    all_leaf_pure =
                        isChildPure(max_stats.segment(0, sps)) &&
                        isChildPure(max_stats.segment(sps, sps));
                }

                if (all_leaf_small){
                    // verify if current's children are too small to split further
                    uint64_t true_count = statCount(max_stats.segment(0, sps));
                    uint64_t false_count = statCount(max_stats.segment(sps, sps));
                    all_leaf_small = (true_count < min_split && false_count < min_split);
                }
            } else {
                feature_indices(current) = FINISHED_LEAF;
                if (current == 0){
                    // prediction is updated by the parent, need special
                    // handling for the root node
                    if (state.num_cat_features > 0){
                        predictions.row(0) =
                            state.cat_stats.row(0).segment(0, sps) +
                            state.cat_stats.row(0).segment(sps, sps);
                    }else if (state.num_con_features > 0){
                        predictions.row(0) =
                            state.con_stats.row(0).segment(0, sps) +
                            state.con_stats.row(0).segment(sps, sps);
                    } else{
                    }
                }
            }
        } // if leaf exists
    } // for each leaf

    // return true if tree expansion is finished
    //      we check (tree_depth = max_depth + 1) since internally
    //      tree_depth starts from 1 though max_depth expects root node depth as 0
    bool training_finished = (children_not_allocated ||
                              tree_depth >= (max_depth + 1) ||
                              all_leaf_small || all_leaf_pure);
    if (training_finished){
        // label any remaining IN_PROCESS_LEAF as FINISHED_LEAF
        for (Index i=0; i < feature_indices.size(); i++) {
            if (feature_indices(i) == IN_PROCESS_LEAF)
                feature_indices(i) = FINISHED_LEAF;
        }
    }
    return training_finished;
}
// -------------------------------------------------------------------------

template <class Container>
inline
ColumnVector
DecisionTree<Container>::statPredict(const ColumnVector &stats) const {

    // stats is assumed to be of size = stats_per_split
    if (is_regression){
        // regression stat -> (0) = num of tuples, (1) = sum of responses
        // we return the average response as prediction
        return ColumnVector(stats.segment(1, 1) / stats(0));
    } else {
        // classification stat ->  (i) = num of tuples for class i
        // we return the proportion of each label
        ColumnVector statsCopy(stats);
        return statsCopy.head(n_y_labels) / static_cast<double>(stats.head(n_y_labels).sum());
    }

}
// -------------------------------------------------------------------------

/**
 * @brief Return the number of tuples accounted in a 'stats' vector
 */
template <class Container>
inline
uint64_t
DecisionTree<Container>::statCount(const ColumnVector &stats) const{

    // stats is assumed to be of size = stats_per_split
    // for both regression and classification, the last element is the number
    // of tuples landing on this node.
    return static_cast<uint64_t>(stats.tail(1)(0));  // tail(n) returns a slice with last n elements
}
// -------------------------------------------------------------------------

/**
 * @brief Return the number of weighted tuples accounted in a 'stats' vector
 */
template <class Container>
inline
double
DecisionTree<Container>::statWeightedCount(const ColumnVector &stats) const{
    // stats is assumed to be of size = stats_per_split
    if (is_regression)
        return stats(0);
    else
        return stats.head(n_y_labels).sum();
}
// -------------------------------------------------------------------------

/**
 * @brief Return the number of tuples that landed on given node
 */
template <class Container>
inline
uint64_t
DecisionTree<Container>::nodeCount(const Index node_index) const{
    return statCount(predictions.row(node_index));
}


/**
 * @brief Return the number of tuples (normalized using weights) that landed on given node
 */
template <class Container>
inline
double
DecisionTree<Container>::nodeWeightedCount(const Index node_index) const{
    return statWeightedCount(predictions.row(node_index));
}


/*
 * Compute misclassification for prediction from a node in a classification tree.
 * For regression, a zero value is returned.
 * For classification, the difference between sum of weighted count and the max coefficient.
 *
 * @param node_index: Index of node for which to compute misclassification
 */
template <class Container>
inline
double
DecisionTree<Container>::computeMisclassification(Index node_index) const {
    if (is_regression) {
        return  0;
    } else {
        return predictions.row(node_index).head(n_y_labels).sum() -
                predictions.row(node_index).head(n_y_labels).maxCoeff();
    }
}
// -------------------------------------------------------------------------


/*
 * Compute risk for a node of the tree.
 * For regression, risk is the variance of the reponse at that node.
 * For classification, risk is the number of misclassifications at that node.
 *
 * @param node_index: Index of node for which to compute risk
 */
template <class Container>
inline
double
DecisionTree<Container>::computeRisk(const Index node_index) const {
    if (is_regression) {
        double wt_tot = predictions.row(node_index)(0);
        double y_avg = predictions.row(node_index)(1);
        double y2_avg = predictions.row(node_index)(2);
        return y2_avg - y_avg * y_avg / wt_tot;
    } else {
        return computeMisclassification(node_index);
    }
}


/**
 * @brief Return if a child node is pure
 */
template <class Container>
inline
bool
DecisionTree<Container>::isChildPure(const ColumnVector &stats) const{
    // stats is assumed to be of size = stats_per_split
    double epsilon = 1e-5;
    if (is_regression){
        // child is pure if variance is extremely small compared to mean
        double mean = stats(1) / stats(0);
        double variance = stats(2) / stats(0) - std::pow(mean, 2);
        return variance < epsilon * mean * mean;
    } else {
        // child is pure if all are of same class
        return (statPredict(stats) / stats.head(n_y_labels).maxCoeff()).sum() < 100 * epsilon;
    }

}
// -------------------------------------------------------------------------

template <class Container>
inline
bool
DecisionTree<Container>::shouldSplit(const ColumnVector &combined_stats,
                                      const uint16_t &min_split,
                                      const uint16_t &min_bucket,
                                      const uint16_t &stats_per_split) const {

    // combined_stats is assumed to be of size = stats_per_split
    // we always want at least 1 tuple going into a child node. Hence the
    // minimum value for min_bucket is 1
    uint64_t thresh_min_bucket = (min_bucket == 0) ? 1u : min_bucket;
    uint64_t true_count = statCount(combined_stats.segment(0, stats_per_split));
    uint64_t false_count = statCount(combined_stats.segment(stats_per_split, stats_per_split));
    return ((true_count + false_count) >= min_split &&
            true_count >= thresh_min_bucket &&
            false_count >= thresh_min_bucket);
}
// ------------------------------------------------------------------------

/**
 * @brief Display the decision tree in dot format
 */
template <class Container>
inline
string
DecisionTree<Container>::displayLeafNode(
            Index id,
            ArrayHandle<text*> &dep_levels,
            const std::string & id_prefix){
    std::stringstream predict_str;
    if (static_cast<bool>(is_regression)){
        predict_str << predict_response(id);
    }
    else{
        std::string dep_value = get_text(dep_levels, static_cast<int>(predict_response(id)));
        predict_str << escape_quotes(dep_value);
    }

    std::stringstream display_str;
    display_str << "\"" << id_prefix << id << "\" [label=\"" << predict_str.str();

    // // uncomment below if distribution of rows is required in leaf node
    // display_str << "\\n[";
    // if (is_regression)
    //      display_str << statCount(predictions.row(id)) << ", "
    //                  << statPredict(predictions.row(id));
    // else
    //     display_str << predictions.row(id);
    // display_str << "]";
    display_str << "\",shape=box]" << ";";
    return display_str.str();
}

/*
    @brief Display the decision tree in dot format
*/
template <class Container>
inline
string
DecisionTree<Container>::displayInternalNode(
            Index id,
            ArrayHandle<text*> &cat_features_str,
            ArrayHandle<text*> &con_features_str,
            ArrayHandle<text*> &cat_levels_text,
            ArrayHandle<int> &cat_n_levels,
            const std::string & id_prefix
            ){

    string feature_name;
    std::stringstream label_str;
    if (is_categorical(id) == 0) {
        feature_name = get_text(con_features_str, feature_indices(id));
        label_str << escape_quotes(feature_name) << "<=" << feature_thresholds(id);
    } else {
        feature_name = get_text(cat_features_str, feature_indices(id));
        label_str << escape_quotes(feature_name) << "<="
                   << getCatLabels(feature_indices(id),
                                   static_cast<Index>(feature_thresholds(id)),
                                   cat_levels_text, cat_n_levels);
    }

    std::stringstream display_str;
    display_str << "\"" << id_prefix << id << "\" [label=\"" << label_str.str();
    // // uncomment below if distribution of rows is required in internal node
    // display_str << "\\n[";
    // if (is_regression)
    //      display_str << statCount(predictions.row(id)) << ", "
    //                  << statPredict(predictions.row(id));
    // else
    //     display_str << predictions.row(id);
    // display_str << "]";
    display_str <<"\", shape=ellipse]" << ";";
   return display_str.str();
}
// -------------------------------------------------------------------------

/**
 * @brief Display the decision tree in dot format
 */
template <class Container>
inline
string
DecisionTree<Container>::display(
        ArrayHandle<text*> &cat_features_str,
        ArrayHandle<text*> &con_features_str,
        ArrayHandle<text*> &cat_levels_text,
        ArrayHandle<int> &cat_n_levels,
        ArrayHandle<text*> &dependent_levels,
        const std::string &id_prefix) {

    std::stringstream display_string;
    if (feature_indices(0) == FINISHED_LEAF){
        display_string << displayLeafNode(0, dependent_levels, id_prefix)
                       << std::endl;
    }
    else{
        for(Index index = 0; index < feature_indices.size() / 2; index++) {
            if (feature_indices(index) != NON_EXISTING &&
                    feature_indices(index) != IN_PROCESS_LEAF &&
                    feature_indices(index) != FINISHED_LEAF) {

                display_string << displayInternalNode(
                        index, cat_features_str, con_features_str,
                        cat_levels_text, cat_n_levels, id_prefix) << std::endl;

                // Display the children
                Index tc = trueChild(index);
                if (feature_indices(tc) != NON_EXISTING) {
                    display_string << "\"" << id_prefix << index << "\" -> "
                                   << "\"" << id_prefix << tc << "\"";

                     // edge going left is "true" node
                    display_string << "[label=\"yes\"];" << std::endl;

                    if (feature_indices(tc) == IN_PROCESS_LEAF ||
                        feature_indices(tc) == FINISHED_LEAF)
                        display_string
                            << displayLeafNode(tc, dependent_levels, id_prefix)
                            << std::endl;
                }

                Index fc = falseChild(index);
                if (feature_indices(fc) != NON_EXISTING) {
                    display_string << "\"" << id_prefix << index << "\" -> "
                                   << "\"" << id_prefix << fc << "\"";

                    // root edge going right is "false" node
                    display_string << "[label=\"no\"];" << std::endl;

                    if (feature_indices(fc) == IN_PROCESS_LEAF ||
                        feature_indices(fc) == FINISHED_LEAF)
                        display_string
                            << displayLeafNode(fc, dependent_levels, id_prefix)
                            << std::endl;
                }
            }
        } // end of for loop
    }
    return display_string.str();
}
// -------------------------------------------------------------------------

template <class Container>
inline
string
DecisionTree<Container>::print(
        Index current,
        ArrayHandle<text*> &cat_features_str,
        ArrayHandle<text*> &con_features_str,
        ArrayHandle<text*> &cat_levels_text,
        ArrayHandle<int> &cat_n_levels,
        ArrayHandle<text*> &dep_levels,
        uint16_t recursion_depth){

    if (feature_indices(current) == NON_EXISTING){
        return "";
    }
    std::stringstream print_string;

    // print current node + prediction
    print_string << "(" << current << ")";
    print_string << "[ ";
    if (is_regression){
        print_string << nodeWeightedCount(current) << ", "
                     << statPredict(predictions.row(current));
    }
    else{
        print_string << predictions.row(current).head(n_y_labels);
    }
    print_string << "]  ";

    if (feature_indices(current) >= 0){
        string feature_name;
        std::stringstream label_str;
        if (is_categorical(current) == 0) {
            feature_name = get_text(con_features_str, feature_indices(current));
            label_str << feature_name << "<=" << feature_thresholds(current);
        } else {
            feature_name = get_text(cat_features_str, feature_indices(current));
            label_str << feature_name << "<="
                       << getCatLabels(feature_indices(current),
                                       static_cast<Index>(feature_thresholds(current)),
                                       cat_levels_text, cat_n_levels);
        }
        print_string << label_str.str() << std::endl;

        std::string indentation(recursion_depth * 3, ' ');
        print_string
            << indentation
            << print(trueChild(current), cat_features_str,
                     con_features_str, cat_levels_text,
                     cat_n_levels, dep_levels, static_cast<uint16_t>(recursion_depth + 1));

        print_string
            << indentation
            << print(falseChild(current), cat_features_str,
                     con_features_str, cat_levels_text,
                     cat_n_levels, dep_levels, static_cast<uint16_t>(recursion_depth + 1));
    } else {
        print_string << "*";
        if (!is_regression){
            std::string dep_value = get_text(dep_levels,
                                             static_cast<int>(predict_response(current)));
            print_string << " --> " << dep_value;
        }
        print_string << std::endl;
    }
    return print_string.str();
}
// -------------------------------------------------------------------------

template <class Container>
inline
string
DecisionTree<Container>::getCatLabels(Index cat_index,
                                      Index cat_value,
                                      ArrayHandle<text*> &cat_levels_text,
                                      ArrayHandle<int> &cat_n_levels) {
    size_t to_skip = 0;
    for (Index i=0; i < cat_index; i++) {
        to_skip += cat_n_levels[i];
    }
    std::stringstream cat_levels;
    size_t start_index;
    cat_levels << "{";
    for (start_index = to_skip;
            start_index < to_skip + cat_value &&
            start_index < cat_levels_text.size();
            start_index++) {
        cat_levels << get_text(cat_levels_text, start_index) << ",";
    }
    cat_levels << get_text(cat_levels_text, start_index) << "}";
    return cat_levels.str();
}
// -------------------------------------------------------------------------

// ------------------------------------------------------------------------
// Definitions for class TreeAccumulator
// ------------------------------------------------------------------------
template <class Container, class DTree>
inline
TreeAccumulator<Container, DTree>::TreeAccumulator(
        Init_type& inInitialization): Base(inInitialization){
    this->initialize();
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
template <class Container, class DTree>
inline
void
TreeAccumulator<Container, DTree>::bind(ByteStream_type& inStream) {
    // update with actual parameters
    inStream >> num_rows
             >> terminated
             >> num_bins
             >> num_cat_features
             >> num_con_features
             >> total_num_cat_levels
             >> num_leaf_nodes
             >> stats_per_split ;

    uint16_t n_bins = 0;
    uint16_t n_cat = 0;
    uint16_t n_con = 0;
    uint32_t tot_levels = 0;
    uint16_t n_leafs = 0;
    uint16_t n_stats = 0;

    if (!num_rows.isNull()){
        n_bins = num_bins;
        n_cat = num_cat_features;
        n_con = num_con_features;
        tot_levels = total_num_cat_levels;
        n_leafs = num_leaf_nodes;
        n_stats = stats_per_split;
    }

    inStream
        >> cat_levels_cumsum.rebind(n_cat)
        >> cat_stats.rebind(n_leafs, tot_levels * n_stats * 2)
        >> con_stats.rebind(n_leafs, n_con * n_bins * n_stats * 2)
        ;
}
// -------------------------------------------------------------------------

/**
 * @brief Rebind all elements of the state when dimensionality elements are
 *  available
 *
 */
template <class Container, class DTree>
inline
void
TreeAccumulator<Container, DTree>::rebind(
        uint16_t n_bins, uint16_t n_cat_feat,
        uint16_t n_con_feat, uint32_t n_total_levels,
        uint16_t tree_depth, uint16_t n_stats) {

    num_bins = n_bins;
    num_cat_features = n_cat_feat;
    num_con_features = n_con_feat;
    total_num_cat_levels = n_total_levels;
    num_leaf_nodes = static_cast<uint16_t>(pow(2, tree_depth - 1));
    stats_per_split = n_stats;
    this->resize();
}
// -------------------------------------------------------------------------

/**
 * @brief Update the accumulation state by feeding a tuple
 */
template <class Container, class DTree>
inline
TreeAccumulator<Container, DTree>&
TreeAccumulator<Container, DTree>::operator<<(const tuple_type& inTuple) {

    tree_type dt = std::get<0>(inTuple);
    const MappedIntegerVector& cat_features = std::get<1>(inTuple);
    const MappedColumnVector& con_features = std::get<2>(inTuple);
    const double& response = std::get<3>(inTuple);
    const double& weight = std::get<4>(inTuple);
    const MappedIntegerVector& cat_levels = std::get<5>(inTuple);
    const MappedMatrix& con_splits = std::get<6>(inTuple);

    // The following checks were introduced with MADLIB-138. It still seems
    // useful to have clear error messages in case of infinite input values.
    if (!terminated){
        if (!std::isfinite(response)) {
            warning("Decision tree response variable values are not finite.");
        } else if (!dbal::eigen_integration::isfinite(con_features)){
            warning("Decision tree continuous feature values are not finite.");
        } else if ((cat_features.size() + con_features.size()) >
                        std::numeric_limits<uint16_t>::max()) {
            warning("Number of independent variables cannot be larger than 65535.");
        } else if (num_cat_features != static_cast<uint16_t>(cat_features.size())) {
            warning("Inconsistent numbers of categorical independent variables.");
        } else if (num_con_features != static_cast<uint16_t>(con_features.size())) {
            warning("Inconsistent numbers of continuous independent variables.");
        } else{
            uint16_t num_non_leaf_nodes = static_cast<uint16_t>(num_leaf_nodes - 1);
            Index dt_search_index = dt.search(cat_features, con_features);
            if (dt.feature_indices(dt_search_index) != dt.FINISHED_LEAF) {
                Index row_index = dt_search_index - num_non_leaf_nodes;

                for (Index i=0; i < num_cat_features; ++i){
                    for (int j=0; j < cat_levels(i); ++j){
                        Index col_index = indexCatStats(
                                i, j, (cat_features(i) <= j));
                        updateStats(static_cast<bool>(dt.is_regression), true,
                                row_index, col_index, response, weight);
                    }
                }
                for (Index i=0; i < num_con_features; ++i){
                    for (Index j=0; j < num_bins; ++j){
                        Index col_index = indexConStats(i, j,
                                (con_features(i) <= con_splits(i, j)));
                        updateStats(static_cast<bool>(dt.is_regression), false,
                                row_index, col_index, response, weight);
                    }
                }
            }
            num_rows++;
            return *this;
        }
        // error case for current group
        terminated = true;
    }
    return *this;
}
// -------------------------------------------------------------------------

/**
 * @brief Merge with another accumulation state
 */
template <class Container, class DTree>
template <class C, class DT>
inline
TreeAccumulator<Container, DTree>&
TreeAccumulator<Container, DTree>::operator<<(
        const TreeAccumulator<C, DT>& inOther) {
    // assuming that (*this) is not empty. This check needs to happen before
    // this function is called.
    if (!inOther.empty()) {
        if ((num_bins != inOther.num_bins) ||
               (num_cat_features != inOther.num_cat_features) ||
               (num_con_features != inOther.num_con_features)) {
            warning("Inconsistent states during merge.");
            terminated = true;
        } else {
            cat_stats += inOther.cat_stats;
            con_stats += inOther.con_stats;
        }
    }
    return *this;
}
// -------------------------------------------------------------------------

/**
 * @brief Update the leaf node statistics for current row in given feature/bin
 */
template <class Container, class DTree>
inline
void
TreeAccumulator<Container, DTree>::updateStats(bool is_regression,
                                               bool  is_cat,
                                               Index row_index,
                                               Index stats_index,
                                               const double &response,
                                               const double &weight) {
    ColumnVector stats(stats_per_split);
    stats.fill(0);
    if (is_regression){
        double w_response = weight * response;
        stats << weight, w_response, w_response * response, 1;
    } else {
        stats(static_cast<uint16_t>(response)) = weight;
        stats.tail(1)(0) = 1;
    }

    if (is_cat) {
        cat_stats.row(row_index).segment(stats_index, stats_per_split) += stats;
    } else {
        con_stats.row(row_index).segment(stats_index, stats_per_split) += stats;
    }
}
// -------------------------------------------------------------------------

template <class Container, class DTree>
inline
Index
TreeAccumulator<Container, DTree>::indexConStats(Index feature_index,
                                                 Index bin_index,
                                                 bool  is_split_true) const {
    assert(feature_index < num_con_features);
    assert(bin_index < num_bins);
    return computeSubIndex(feature_index * num_bins, bin_index, is_split_true);
}
// -------------------------------------------------------------------------

template <class Container, class DTree>
inline
Index
TreeAccumulator<Container, DTree>::indexCatStats(Index feature_index,
                                                 int   cat_value,
                                                 bool  is_split_true) const {
    // cat_stats is a matrix
    //   size = (num_leaf_nodes) x (total_num_cat_levels * stats_per_split * 2)
    assert(feature_index < num_cat_features);
    unsigned int cat_cumsum_value = (feature_index == 0) ? 0 : cat_levels_cumsum(feature_index - 1);
    return computeSubIndex(static_cast<Index>(cat_cumsum_value),
                           static_cast<Index>(cat_value),
                           is_split_true);
}
// -------------------------------------------------------------------------

template <class Container, class DTree>
inline
Index
TreeAccumulator<Container, DTree>::computeSubIndex(Index start_index,
                                                   Index relative_index,
                                                   bool is_split_true) const {
    Index col_index = static_cast<Index>(stats_per_split * 2 *
                                         (start_index + relative_index));
    return is_split_true ? col_index : col_index + stats_per_split;
}
//-------------------------------------------------------------------------

} // namespace recursive_partitioning
} // namespace modules
} // namespace madlib

#endif // defined(MADLIB_MODULES_RP_DT_IMPL_HPP)

