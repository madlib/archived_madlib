/* ----------------------------------------------------------------------- *//**
 *
 * @file viterbi.cpp
 *
 * @brief Viterbi Algorithm for inferencing implementation
 *
 * This function implements viterbi algorithm to generate best label sequence
 * given the state emission and state transition matrices.
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>
#include <cmath>
#include "viterbi.hpp"

namespace madlib {
namespace modules {
namespace crf {

using namespace dbal::eigen_integration;
using madlib::dbconnector::postgres::madlib_construct_array;


AnyType vcrf_top1_label::run(AnyType& args) {

    ArrayHandle<double> mArray = args[0].getAs<ArrayHandle<double> >();
    ArrayHandle<double> rArray = args[1].getAs<ArrayHandle<double> >();
    const int32_t numLabels = args[2].getAs<int32_t>();

    if (numLabels == 0)
        throw std::invalid_argument("Number of labels cannot be zero");

    int doc_len = static_cast<int>(rArray.size() / numLabels);

    double* prev_top1_array = new double[numLabels];
    double* curr_top1_array = new double[numLabels];
    double* prev_norm_array = new double[numLabels];
    double* curr_norm_array = new double[numLabels];
    int* path = new int[doc_len*numLabels];

    memset(prev_top1_array, 0, numLabels*sizeof(double));
    memset(prev_norm_array, 0, numLabels*sizeof(double));
    memset(path, 0, doc_len*numLabels*sizeof(int));

    for(int start_pos = 0; start_pos < doc_len; start_pos++) {
        memset(curr_top1_array, 0, numLabels*sizeof(double));
        memset(curr_norm_array, 0, numLabels*sizeof(double));

        if (start_pos == 0) {
            for (int label = 0; label < numLabels; label++) {
                 curr_norm_array[label] = rArray[label] + mArray[label];
                 curr_top1_array[label] = rArray[label] + mArray[label];
            }
        } else {
            for (int curr_label = 0; curr_label < numLabels; curr_label++) {
                for (int prev_label = 0; prev_label < numLabels; prev_label++) {
                    double top1_new_score = prev_top1_array[prev_label]
                                               + rArray[start_pos*numLabels + curr_label]
                                               + mArray[(prev_label+1)*numLabels + curr_label];

                    if (start_pos == doc_len - 1)
                        top1_new_score += mArray[(numLabels+1)*numLabels + curr_label];

                    if (top1_new_score > curr_top1_array[curr_label]) {
                        curr_top1_array[curr_label] = top1_new_score;
                        path[start_pos*numLabels + curr_label] = prev_label;
                    }

                    /* calculate the probability of the best label sequence */
                    double norm_new_score = prev_norm_array[prev_label]
                                               + rArray[start_pos * numLabels + curr_label]
                                               + mArray[(prev_label+1)*numLabels + curr_label];

                    /* last token in a sentence, the end feature should be fired */
                    if (start_pos == doc_len - 1)
                        norm_new_score += mArray[(numLabels+1)*numLabels + curr_label];

                    /* The following wants to do z = log(exp(x)+exp(y)), the faster implementation is
                     *  z=min(x,y) + log(exp(abs(x-y))+1)
                     *  0.5 is for rounding
                     */
                    if (curr_norm_array[curr_label] == 0)
                        curr_norm_array[curr_label] = norm_new_score;
                    else {
                        double x = curr_norm_array[curr_label];
                        double y = norm_new_score;
                        curr_norm_array[curr_label] = std::min(x,y) +
                                static_cast<double>(log(std::exp(std::abs(y-x)/1000.0) +1)*1000.0 + 0.5);
                    }
                }
            }
        }
        for (int label = 0; label < numLabels; label++) {
            prev_top1_array[label] = curr_top1_array[label];
            prev_norm_array[label] = curr_norm_array[label];
        }
    }

    /* find the label of the last token in a sentence */
    double max_score = 0.0;
    int top1_label = 0;
    for(int label = 0; label < numLabels; label++) {
        if(curr_top1_array[label] > max_score) {
            max_score = curr_top1_array[label];
            top1_label = label;
        }
    }

    /* Define the result array with doc_len+1 elements, where the first doc_len
     * elements are used to store the best labels and the last element is used
     * to store the conditional probability of the sequence.
     */

    // FIXME: construct_array functions circumvent the abstraction layer. These
    // should be replaced with appropriate Allocator:: calls.
    MutableArrayHandle<int> result(
        madlib_construct_array(
            NULL, doc_len + 1, INT4OID, sizeof(int32_t), true, 'i'));

    /* trace back to get the labels for the rest tokens in a sentence */
    result[doc_len - 1] = top1_label;
    for (int pos = doc_len - 1; pos >= 1; pos--) {
        top1_label = path[pos * numLabels + top1_label];
        result[pos-1] = top1_label;
    }

    /* compute the sum_i of log(v1[i]/1000), return (e^sum)*1000
     * used in the UDFs which needs marginalization e.g., normalization
     * the following wants to do z=log(exp(x)+exp(y)), the faster implementation is
     * z = min(x,y) + log(exp(abs(x-y))+1)
     */
    double norm_factor = 0.0;
    for (int i = 0; i < numLabels; i++) {
        if (i==0)
            norm_factor = curr_norm_array[0];
        else {
            double x = curr_norm_array[i];
            double y = norm_factor;
            norm_factor = std::min(x,y) + static_cast<double>(log(exp(std::abs(y-x)/1000.0) +1)*1000.0+0.5);
         }
    }

    /* calculate the conditional probability.
     * To convert the probability into integer, firstly,let it multiply 1000000, then later make the product divided by 1000000
     * to get the real conditional probability
     */
    result[doc_len] = static_cast<int>(std::exp((max_score - norm_factor)/1000.0)*1000000);

    delete[] prev_top1_array;
    delete[] curr_top1_array;
    delete[] prev_norm_array;
    delete[] curr_norm_array;
    delete[] path;

    return result;
}

}
}
}
