/* ------------------------------------------------------
 *
 * @file random_forest.cpp
 *
 * @brief Random Forest functions
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
#include <boost/random/discrete_distribution.hpp>
#include <boost/random/variate_generator.hpp>

#include "DT_proto.hpp"
#include "DT_impl.hpp"
#include "ConSplits.hpp"

#include <math.h>       /* fabs */

#include "random_forest.hpp"

namespace madlib {

// Use Eigen
using namespace dbal::eigen_integration;

using boost::random::discrete_distribution;
using boost::random::variate_generator;

namespace modules {

namespace recursive_partitioning {

typedef DecisionTree<RootContainer> Tree;

/*
 * Permute each categorical variable and predict
 */
AnyType
rf_cat_imp_score::run(AnyType &args) {
    if (args[0].isNull() || args[7].isNull()) { return Null(); }
    Tree dt = args[0].getAs<ByteString>();
    MutableNativeIntegerVector cat_features;
    NativeColumnVector con_features;
    try {
        if (args[1].isNull()){
            // no cat features
            return Null();
        }
        else {
            MutableNativeIntegerVector xx_cat = args[1].getAs<MutableNativeIntegerVector>();
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
        // not expect to reach here
        // if max_surr = 0, nulls are filtered
        // otherwise, mapped to -1 or NaN
        return Null();
    }

    MappedIntegerVector cat_n_levels = args[3].getAs<MappedIntegerVector>();

    int n_permutations = args[4].getAs<int>();
    double y = args[5].getAs<double>();
    bool is_classification = args[6].getAs<bool>();
    MappedMatrix distributions = args[7].getAs<MappedMatrix>();

    // returning
    MutableNativeColumnVector permuted_predictions(
            this->allocateArray<double>(cat_n_levels.size()));

    // permute each and predict
    NativeRandomNumberGenerator generator;
    for (int p = 0; p < n_permutations; p ++) {
        for (Index i = 0; i < cat_n_levels.size(); i ++) {
            int orig_i = cat_features(i);
            discrete_distribution<> ddist(distributions.col(i).data(),
                    distributions.col(i).data() + cat_n_levels(i) + 1);
            variate_generator<NativeRandomNumberGenerator, discrete_distribution<> >
                    rvt(generator, ddist);

            cat_features(i) = rvt() - 1;

            // calling NativeIntegerVector for a const cast
            // see EigenIntegration_impl.hpp in ports for details
            double prediction = dt.predict_response(
                NativeIntegerVector(cat_features.memoryHandle()), con_features);
            double score = 0.;
            if (is_classification) {
                score = y - prediction < 1e-3 ? 1. : 0.;
            } else {
                score = - (y - prediction) * (y - prediction);
            }
            permuted_predictions(i) += score;

            cat_features(i) = orig_i;
        }
    }
    permuted_predictions /= n_permutations;
    return permuted_predictions;
}
// ------------------------------------------------------------


/*
 * Permute each continuous variable and predict
 */
AnyType
rf_con_imp_score::run(AnyType &args) {
    if (args[0].isNull() || args[7].isNull()) { return Null(); }
    Tree dt = args[0].getAs<ByteString>();
    NativeIntegerVector cat_features;
    MutableNativeColumnVector con_features;
    try {
        if (args[1].isNull()){
            // no cat features
            cat_features.rebind(this->allocateArray<int>(0));
        }
        else {
            NativeIntegerVector xx_cat = args[1].getAs<NativeIntegerVector>();
            cat_features.rebind(xx_cat.memoryHandle(), xx_cat.size());
        }
        if (args[2].isNull()){
            //no con features
            return Null();
        }
        else {
            MutableNativeColumnVector xx_con = args[2].getAs<MutableNativeColumnVector>();
            con_features.rebind(xx_con.memoryHandle(), xx_con.size());
        }
    } catch (const ArrayWithNullException &e) {
        // not expect to reach here
        // if max_surr = 0, nulls are filtered
        // otherwise, mapped to -1 or NaN
        return Null();
    }

    // con_splits size = num_con_features x num_bins
    // When num_con_features = 0, the input will be an empty string that is read
    // as a ByteString
    ConSplitsResult<RootContainer> splits_results = args[3].getAs<ByteString>();

    int n_permutations = args[4].getAs<int>();
    double y = args[5].getAs<double>();
    bool is_classification = args[6].getAs<bool>();
    MappedMatrix distributions = args[7].getAs<MappedMatrix>();

    // returning
    MutableNativeColumnVector permuted_predictions(
            this->allocateArray<double>(con_features.size()));

    // permute each and predict
    NativeRandomNumberGenerator generator;
    for (int p = 0; p < n_permutations; p ++) {
        for (Index i = 0; i < con_features.size(); i ++) {
            double orig_i = con_features(i);
            discrete_distribution<> ddist(distributions.col(i).data(),
                    distributions.col(i).data() + distributions.rows());
            variate_generator<NativeRandomNumberGenerator, discrete_distribution<> >
                    rvt(generator, ddist);

            int outcome = rvt();
            if (outcome == 0) {
                con_features(i) = std::numeric_limits<double>::quiet_NaN();
            } else if (outcome == static_cast<int>(distributions.rows()) - 1) {
                // bin value that is larger than the last separator (last value in con_splits)
                con_features(i) = splits_results.con_splits(i, outcome-2) + 1.;
            } else {
                con_features(i) = splits_results.con_splits(i, outcome-1);
            }

            // calling NativeColumnVector for a const cast
            // see EigenIntegration_impl.hpp in ports for details
            double prediction = dt.predict_response(
                cat_features, NativeColumnVector(con_features.memoryHandle()));
            double score = 0.;
            if (is_classification) {
                score = y - prediction < 1e-3 ? 1. : 0.;
            } else {
                score = - (y - prediction) * (y - prediction);
            }
            permuted_predictions(i) += score;

            con_features(i) = orig_i;
        }
    }
    permuted_predictions /= n_permutations;
    return permuted_predictions;
}
// ------------------------------------------------------------


} // namespace recursive_partitioning
} // namespace modules
} // namespace madlib
