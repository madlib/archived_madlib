/* ----------------------------------------------------------------------- *//**
 *
 * @file DT_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_RP_DT_PROTO_HPP
#define MADLIB_MODULES_RP_DT_PROTO_HPP

#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <iterator>

#include <limits>  // std::numeric_limits

#include <dbconnector/dbconnector.hpp>

namespace madlib {

namespace modules {

namespace recursive_partitioning {

// Use Eigen
using namespace dbal;
using namespace dbal::eigen_integration;
using std::vector;
using std::string;

const uint16_t REGRESS_N_STATS = 4u;

template <class Container>
class DecisionTree: public DynamicStruct<DecisionTree<Container>, Container> {
public:
    typedef DynamicStruct<DecisionTree, Container> Base;
    MADLIB_DYNAMIC_STRUCT_TYPEDEFS;

    enum { MSE, MISCLASS, ENTROPY, GINI };
    enum { IN_PROCESS_LEAF=-1, FINISHED_LEAF=-2, NODE_NON_EXISTING=-3 };
    enum { SURR_IS_MAJORITY=-1, SURR_NON_EXISTING=-2 };

    // functions
    DecisionTree(Init_type& inInitialization);
    DecisionTree();
    void bind(ByteStream_type& inStream);
    DecisionTree& rebind(const uint16_t in_tree_depth,
                         const uint16_t in_n_y_labels,
                         const uint16_t in_max_n_surr,
                         const bool in_is_regression);
    DecisionTree& incrementInPlace();
    DecisionTree& my_tree();
    Index search(MappedIntegerVector cat_features,
                 MappedColumnVector con_features) const;
    uint64_t getMajorityCount(Index node_index) const;
    bool getMajoritySplit(Index node_index) const;
    bool getSurrSplit(Index node_index,
                      MappedIntegerVector cat_features,
                      MappedColumnVector con_features) const;
    ColumnVector predict(MappedIntegerVector cat_features,
                         MappedColumnVector con_features) const;
    double predict_response(MappedIntegerVector cat_features,
                            MappedColumnVector con_features) const;
    double predict_response(Index feature_index) const;

    bool updatePrimarySplit(const Index node_index,
                            const int & max_feat,
                            const double & max_threshold,
                            const bool & max_is_cat,
                            const uint16_t & min_split,
                            const ColumnVector &true_stats,
                            const ColumnVector &false_stats);

    template <class Accumulator>
    bool expand(const Accumulator &state,
                const MappedMatrix &con_splits,
                const uint16_t &min_split,
                const uint16_t &min_bucket,
                const uint16_t &max_depth);

    template <class Accumulator>
    bool expand_by_sampling(const Accumulator &state,
                            const MappedMatrix &con_splits,
                            const uint16_t &min_split,
                            const uint16_t &min_bucket,
                            const uint16_t &max_depth,
                            const int &num_random_features);


    Index parentIndex(Index current) const {
        return current > 0? static_cast<Index>((current-1)/2): 0;
    }
    Index trueChild(Index current) const { return 2 * current + 1; }
    Index falseChild(Index current) const { return 2 * current + 2; }
    double impurity(const ColumnVector & stats) const;
    double impurityGain(const ColumnVector &combined_stats,
                        const uint16_t &stats_per_split) const;
    bool shouldSplit(const ColumnVector & stats,
                     const uint16_t &min_split,
                     const uint16_t &min_bucket,
                     const uint16_t &stats_per_split,
                     const uint16_t &max_depth) const;

    bool shouldSplitWeights(const ColumnVector & stats,
                            const uint16_t &min_split,
                            const uint16_t &min_bucket,
                            const uint16_t &stats_per_split) const;
    template <class Accumulator>
    void pickSurrogates(const Accumulator &state,
                        const MappedMatrix &con_splits);

    ColumnVector statPredict(const ColumnVector &stats) const;
    uint64_t statCount(const ColumnVector &stats) const;
    double statWeightedCount(const ColumnVector &stats) const;
    uint64_t nodeCount(const Index node_index) const;
    double nodeWeightedCount(const Index node_index) const;
    double computeMisclassification(const Index node_index) const;
    double computeRisk(const Index node_index) const;
    bool isChildPure(const ColumnVector &stats) const;
    bool isNull(const double feature_val, const bool is_categorical) const{
        return (is_categorical) ? (feature_val < 0) : std::isnan(feature_val);
    }

    uint16_t recomputeTreeDepth() const;
    string displayLeafNode(Index id, ArrayHandle<text*> &dep_levels, const std::string & id_prefix, bool verbose);
    string displayInternalNode(Index id,
                               ArrayHandle<text*> &cat_features_str,
                               ArrayHandle<text*> &con_features_str,
                               ArrayHandle<text*> &cat_levels_text,
                               ArrayHandle<int> &cat_n_levels,
                               ArrayHandle<text*> &dep_levels,
                               const std::string & id_prefix,
                               bool verbose);
    string display(ArrayHandle<text*>&, ArrayHandle<text*>&, ArrayHandle<text*>&,
                   ArrayHandle<int>&, ArrayHandle<text*>&, const std::string&, bool verbose);
    string getCatLabels(Index, Index, Index, ArrayHandle<text*> &,
                        ArrayHandle<int> &);
    string print_split(bool, bool, Index, double,
                       ArrayHandle<text*> &, ArrayHandle<text*> &,
                       ArrayHandle<text*> &, ArrayHandle<int> &);
    string print(Index, ArrayHandle<text*>&, ArrayHandle<text*>&,
                 ArrayHandle<text*>&, ArrayHandle<int>&,
                 ArrayHandle<text*>&, uint16_t);
    string surr_display(
        ArrayHandle<text*> &cat_features_str,
        ArrayHandle<text*> &con_features_str,
        ArrayHandle<text*> &cat_levels_text,
        ArrayHandle<int> &cat_n_levels);

    int encodeIndex(const int &feature_index,
                      const int &is_categorical,
                      const int &n_cat_features) const;

    // attributes
    // dimension information
    uint16_type tree_depth; // 1 for root-only tree

    uint16_type n_y_labels;

    uint16_type max_n_surr;

    bool_type is_regression; // = 0 for classification, = 1 for regression

    uint16_type impurity_type;  // can be 'mse', 'gini', 'entropy', 'misclass'

    // All vectors below are of size n_nodes ( = 2^{tree_depth} - 1 )
    // Note: in order to make DynamicStruct::DryRun work,
    // non-scalars should be of size 0 when dimension is 0

    // The complete tree is broken into multiple vectors: each vector being the
    // collection of a single variable for all nodes.

    // -1 means leaf, -2 mean non-existing node
    IntegerVector_type feature_indices;
    // elements are of integer type for categorical
    ColumnVector_type feature_thresholds;
    // used as boolean array, 0 means continuous, otherwise categorical
    IntegerVector_type is_categorical;

    // Used to keep count of the number of non-null rows that are split to the
    // left and right children for each internal node.
    // For leaf nodes, the value is 0.
    // This is useful when using surrogates to compute the majority count/split
    // for an internal node. Size = n_nodes x 2
    ColumnVector_type nonnull_split_count;
    // FIXME: we use a ColumnVector (elements of 'double' type) to store
    // big int values, since we dont' yet have a vector type with uint64_t elements.

    // 'surrogate_indices' is of size n_nodes x max_n_surrogates
    // If a particular node has fewer than max_n_surrogates, then
    //   the indices on non-existing will be -1
    IntegerVector_type surr_indices;
    ColumnVector_type surr_thresholds;   // used as integer if classification

    // used as enumerated array to record the status of a surrogate split
    //      0 = default value indicating invalid surrogate split
    //      1 = categorical feature <= threshold
    //     -1 = categorical feature > threshold
    //      2 = continuous feature <= threshold
    //     -2 = continuous feature > threshold
    IntegerVector_type surr_status;
    // used for keeping count of number of rows that are common between primary
    // and surrogate
    IntegerVector_type surr_agreement;

    // 'prediction' is matrix of size n_nodes x n_predictions
    //      where n_predictions = stats_per_split,
    //      stats_per_split is set by the accumulator during training of tree
    Matrix_type predictions;   // used as integer if we do classification
};

// ------------------------------------------------------------------------
// TreeAccumulator is used for collecting statistics during training the nodes
// for a level of the decision tree. The same accumulator is also used for
// computing the statistics for surrogate splits.

template <class Container, class DTree>
class TreeAccumulator
  : public DynamicStruct<TreeAccumulator<Container, DTree>, Container> {
public:
    typedef DynamicStruct<TreeAccumulator, Container> Base;
    MADLIB_DYNAMIC_STRUCT_TYPEDEFS;
    typedef DTree tree_type;
    typedef std::tuple< tree_type,
                        MappedIntegerVector, // categorical feature values
                        MappedColumnVector,  // continuous feature values
                        double,              // response variable
                        double,              // weight
                        MappedIntegerVector, // levels for each categorical feature
                        MappedMatrix         // split values for each continuous feature
                      > tuple_type;

    typedef std::tuple< tree_type,
                        MappedIntegerVector, // categorical feature values
                        MappedColumnVector,  // continuous feature values
                        MappedIntegerVector, // levels for each categorical feature
                        MappedMatrix,        // split values for each continuous feature
                        int                  // duplicated count for each tuple
                                             //   (used in random forest)
                       > surr_tuple_type;

    // functions
    TreeAccumulator(Init_type& inInitialization);
    void bind(ByteStream_type& inStream);
    void rebind(uint16_t n_bins, uint16_t n_cat_feat,
                uint16_t n_con_feat, uint32_t n_total_levels,
                uint16_t tree_depth, uint16_t n_stats, bool weights_as_rows);

    TreeAccumulator& operator<<(const tuple_type& inTuple);
    TreeAccumulator& operator<<(const surr_tuple_type& inTuple);

    template <class C, class DT>
    TreeAccumulator& operator<<(const TreeAccumulator<C, DT>& inOther);
    bool empty() const { return this->n_rows == 0; }

    // cat_features[feature_index] <= cat_value
    Index indexCatStats(Index feature_index, int cat_value, bool is_split_true) const;
    // con_features[feature_index] <= bin_threshold,
    // bin_threshold = con_splits[feature_index, bin_index]
    Index indexConStats(Index feature_index, Index bin_index, bool is_split_true) const;
    Index computeSubIndex(Index start_Index, Index relative_index, bool is_split_true) const;

    void updateNodeStats(bool is_regression, Index row_index,
                         const double response, const double weight);
    // apply the tuple using indices
    void updateStats(bool is_regression, bool is_cat, Index row_index,
                     Index stats_index, const double response,
                     const double weight);

    // apply the tuple using indices
    void updateSurrStats(const bool is_cat, const bool surr_agrees,
                         Index row_index, Index stats_index, const int dup_count);

    // attributes
    uint64_type n_rows;  // number of rows mapped to this node
    // return NULL state if the node is terminated due to an error
    bool_type terminated;

    // dimension information
    uint16_type n_bins;
    uint16_type n_cat_features;
    uint16_type n_con_features;
    // sum of num of levels in each categorical variable
    uint32_type total_n_cat_levels;
    // n_leaf_nodes = 2^{dt.tree_depth-1} for dt.tree_depth > 0
    uint16_type n_leaf_nodes;
    // For regression, stats_per_split = 4, i.e. (w, w*y, w*y^2, 1)
    // For classification, stats_per_split = (number of class labels + 1)
    // i.e. (w_1, w_2, ..., w_c, 1)
    // For surrogates calculation, stats_per_split = 1
    uint16_type stats_per_split;

    // treat weights as duplicated rows (used for random forest)
    bool_type weights_as_rows;

    // training statistics
    // cumulative sum of the levels of categorical variables
    // with first element as 0. This is helpful to compute the index into
    // cat_stats for each categorical variable
    // size = n_cat_features last
    // element = (total_n_cat_levels - last element of cat_levels)
    IntegerVector_type cat_levels_cumsum; // used as integer array

    // con_stats and cat_stats are matrices that contain the statistics used
    // during training.
    // cat_stats is a matrix of size:
    // (n_leaf_nodes) x (total_n_cat_levels * stats_per_split * 2)
    Matrix_type cat_stats;
    // con_stats is a matrix:
    // (n_leaf_nodes) x (n_con_features * n_bins * stats_per_split * 2)
    Matrix_type con_stats;

    // node_stats is used to keep a statistic of all the rows that land on a
    // node and are used to determine the prediction made by a node.
    // In the absence of any NULL value, these stats can be deduced from
    // cat_stats/con_stats. In the presence of NULL value, the stats could be
    // different.
    Matrix_type node_stats;
};
// ------------------------------------------------------------------------


} // namespace recursive_partitioning

} // namespace modules

} // namespace madlib

#endif // defined(MADLIB_MODULES_RP_DT_PROTO_HPP)
