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
    enum { IN_PROCESS_LEAF=-1, FINISHED_LEAF=-2, NON_EXISTING=-3};

    // functions
    DecisionTree(Init_type& inInitialization);
    DecisionTree();
    void bind(ByteStream_type& inStream);
    DecisionTree& rebind(const uint16_t in_tree_depth, const uint16_t n_y_labels);
    DecisionTree& incrementInPlace();
    DecisionTree& my_tree();
    Index search(MappedIntegerVector cat_features,
                 MappedColumnVector con_features) const;
    ColumnVector predict(MappedIntegerVector cat_features,
                         MappedColumnVector con_features) const;
    double predict_response(MappedIntegerVector cat_features,
                            MappedColumnVector con_features) const;
    double predict_response(Index feature_index) const;

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
                       const uint16_t &stats_per_split) const;
    bool shouldSplitWeights(const ColumnVector & stats,
                       const uint16_t &min_split,
                       const uint16_t &min_bucket,
                       const uint16_t &stats_per_split) const;
    ColumnVector statPredict(const ColumnVector &stats) const;
    uint64_t statCount(const ColumnVector &stats) const;
    double statWeightedCount(const ColumnVector &stats) const;
    uint64_t nodeCount(const Index node_index) const;
    double nodeWeightedCount(const Index node_index) const;
    double computeMisclassification(const Index node_index) const;
    double computeRisk(const Index node_index) const;
    bool isChildPure(const ColumnVector &stats) const;

    string print(Index, ArrayHandle<text*>&, ArrayHandle<text*>&,
                 ArrayHandle<text*>&, ArrayHandle<int>&,
                 ArrayHandle<text*>&, uint16_t);

    string displayLeafNode(Index id, ArrayHandle<text*> &dep_levels, const std::string & id_prefix);
    string displayInternalNode(
            Index id,
            ArrayHandle<text*> &cat_features_str,
            ArrayHandle<text*> &con_features_str,
            ArrayHandle<text*> &cat_levels_text,
            ArrayHandle<int> &cat_n_levels,
            const std::string & id_prefix);
    string display(ArrayHandle<text*>&, ArrayHandle<text*>&, ArrayHandle<text*>&,
                   ArrayHandle<int>&, ArrayHandle<text*>&, const std::string&);
    string getCatLabels(Index, Index, ArrayHandle<text*> &,
                        ArrayHandle<int> &);
    // attributes
    // dimension information
    uint16_type tree_depth; // 1 for root-only tree

    bool_type is_regression; // = 0 for classification, = 1 for regression

    uint16_type impurity_type;  // can be 'mse', 'gini', 'entropy', 'misclass'

    uint16_type n_y_labels;

    // All vectors below are of size num_nodes ( = 2^{tree_depth} - 1 )
    // Note: in order to make DynamicStruct::DryRun work,
    // non-scalars should be of size 0 when dimension is 0

    // The complete tree is broken into multiple vectors: each vector being the
    // collection of a single variable of a node.

    // FIXME -1 and -2 should be replaced by enum values
    // -1 means leaf, -2 mean non-existing node
    IntegerVector_type feature_indices;
    // FIXME perhaps we should always store index instead? (simplied expand)
    // elements are of integer type for categorical
    ColumnVector_type feature_thresholds;
    // used as boolean array, 0 means continuous, otherwise categorical
    IntegerVector_type is_categorical;

    // 'prediction' is matrix of size num_nodes x n_predictions
    //  For regression, n_predictions = 1; the average of the response of all tuples
    //  For classification, n_predictions = n_response_labels
    //      this value is only used if a node is a leaf node
    Matrix_type predictions;   // used as integer if we do classification
};

// ------------------------------------------------------------------------

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

    // functions
    TreeAccumulator(Init_type& inInitialization);
    void bind(ByteStream_type& inStream);
    void rebind(uint16_t n_bins, uint16_t n_cat_feat,
                uint16_t n_con_feat, uint32_t n_total_levels,
                uint16_t tree_depth, uint16_t n_stats);

    TreeAccumulator& operator<<(const tuple_type& inTuple);
    template <class C, class DT>
    TreeAccumulator& operator<<(const TreeAccumulator<C, DT>& inOther);
    bool empty() const { return this->num_rows == 0; }

    // cat_features[feature_index] <= cat_value
    Index indexCatStats(Index feature_index, int cat_value, bool is_split_true) const;
    // con_features[feature_index] <= bin_threshold,
    // bin_threshold = con_splits[feature_index, bin_index]
    Index indexConStats(Index feature_index, Index bin_index, bool is_split_true) const;
    Index computeSubIndex(Index start_Index, Index relative_index, bool is_split_true) const;
    // apply the tuple using indices
    void updateStats(bool is_regression, bool is_cat, Index row_index,
                     Index stats_index, const double &response,
                     const double &weight);

    // attributes
    uint64_type num_rows;  // number of rows mapped to this node
    // return NULL state if the node is terminated due to an error
    bool_type terminated;

    // dimension information
    uint16_type num_bins;
    uint16_type num_cat_features;
    uint16_type num_con_features;
    // sum of num of levels in each categorical variable
    uint32_type total_num_cat_levels;
    // num_leaf_nodes = 2^{dt.tree_depth-1} for dt.tree_depth > 0
    uint16_type num_leaf_nodes;
    // For regression, stats_per_split = 3, i.e. (1, y, y^2)
    // For classification, stats_per_split = (number of class labels)
    // i.e. (0, 1, 2, ..., C-1)
    uint16_type stats_per_split;

    // training statistics
    // cumulative sum of the levels of categorical variables
    // with first element as 0. This is helpful to compute the index into
    // cat_stats for each categorical variable
    // size = num_cat_features last
    // element = (total_num_cat_levels - last element of cat_levels)
    IntegerVector_type cat_levels_cumsum; // used as integer array

    // cat_stats is matrix of (num_leaf_nodes) rows,
    // each row is a vector (flattened matrix) of size:
    // (total_num_cat_levels * stats_per_split * 2)
    Matrix_type cat_stats;
    // con_stats and cat_stats are matrices that contain the statistics used
    // during training.
    // con_stats is a matrix:
    // (num_leaf_nodes) x (num_con_features * num_bins * stats_per_split * 2)
    Matrix_type con_stats;
};
// ------------------------------------------------------------------------


} // namespace recursive_partitioning

} // namespace modules

} // namespace madlib

#endif // defined(MADLIB_MODULES_RP_DT_PROTO_HPP)
