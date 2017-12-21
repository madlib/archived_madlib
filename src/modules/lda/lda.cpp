/* ----------------------------------------------------------------------- *//**
 *
 * @file lda.cpp
 *
 * @brief Functions for Latent Dirichlet Allocation
 *
 * @date Dec 13, 2012
 *//* ----------------------------------------------------------------------- */


#include <dbconnector/dbconnector.hpp>
#include <math.h>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <functional>
#include <numeric>
#include "lda.hpp"

namespace madlib {
namespace modules {
namespace lda {

using namespace dbal::eigen_integration;
using madlib::dbconnector::postgres::madlib_construct_array;
using madlib::dbconnector::postgres::madlib_construct_md_array;

/**
 * @brief This function samples a new topic for a word in a document based on
 * the topic counts computed on the rest of the corpus. This is the core
 * function in the Gibbs sampling inference algorithm for LDA.
 * @param topic_num     The number of topics
 * @param topic         The current topic assignment to a word
 * @param count_d_z     The document topic counts
 * @param count_w_z     The word topic counts
 * @param count_z       The corpus topic counts
 * @param alpha         The Dirichlet parameter for the per-doc topic
 *                      multinomial
 * @param beta          The Dirichlet parameter for the per-topic word
 *                      multinomial
 * @return retopic      The new topic assignment to the word
 * @note The topic ranges from 0 to topic_num - 1.
 *
 * @note For the sake of performance, this function will not check the validity
 * of parameters. The caller will ensure that the three pointers all have non-null
 * values and the lengths are the actual lengths of the arrays. And this
 * function is local to this file only, so this function cannot be maliciously
 * called by intruders.
 **/
static int32_t __lda_gibbs_sample(
    int32_t topic_num, int32_t topic, const int32_t * count_d_z, const int32_t * count_w_z,
    const int64_t * count_z, double alpha, double beta)
{
    /* The cumulative probability distribution of the topics */
    double * topic_prs = new double[topic_num];
    if(!topic_prs)
        throw std::runtime_error("out of memory");

    /* Calculate topic (unnormalised) probabilities */
    double total_unpr = 0;
    for (int32_t i = 0; i < topic_num; i++) {
        int32_t nwz = count_w_z[i];
        int32_t ndz = count_d_z[i];
        int64_t nz = count_z[i];

        /* Adjust the counts to exclude current word's contribution */
        if (i == topic) {
            nwz--;
            ndz--;
            nz--;
        }

        /* Compute the probability */
        // Note that ndz, nwz, nz are non-negative, and topic_num, alpha, and
        // beta are positive, so the division by zero will not occure here.
        double unpr =
            (ndz + alpha) * (static_cast<double>(nwz) + beta)
                / (static_cast<double>(nz) + topic_num * beta);
        total_unpr += unpr;
        topic_prs[i] = total_unpr;
    }

    /* Normalise the probabilities */
    // Note that the division by zero will not occure here, so no need to check
    // whether total_unpr is zero
    for (int32_t i = 0; i < topic_num; i++)
        topic_prs[i] /= total_unpr;

    /* Draw a topic at random */
    double r = drand48();
    int32_t retopic = 0;
    while (true) {
        if (retopic == topic_num - 1 || r < topic_prs[retopic])
            break;
        retopic++;
    }

    delete[] topic_prs;
    return retopic;
}

/**
 * @brief Get the min value of an array - for parameter checking
 * @return      The min value
 * @note The caller will ensure that ah is always non-null.
 **/
template<class T> static T __min(
    ArrayHandle<T> ah, size_t start, size_t len){
    const T * array = ah.ptr() + start;
    return *std::min_element(array, array + len);
}
template<class T> static T __min(ArrayHandle<T> ah){
    return __min(ah, 0, ah.size());
}

/**
 * @brief Get the max value of an array - for parameter checking
 * @return      The max value
 * @note The caller will ensure that ah is always non-null.
 **/
template<class T> static T __max(
    ArrayHandle<T> ah, size_t start, size_t len){
    const T * array = ah.ptr() + start;
    return *std::max_element(array, array + len);
}
template<class T> static T __max(ArrayHandle<T> ah){
    return __max(ah, 0, ah.size());
}

/**
 * @brief Get the sum of an array - for parameter checking
 * @return      The sum
 * @note The caller will ensure that ah is always non-null.
 **/
static int32_t __sum(ArrayHandle<int32_t> ah){
    const int32_t * array = ah.ptr();
    size_t size = ah.size();
    return std::accumulate(array, array + size, static_cast<int32_t>(0));
}

/**
 * @brief This function learns the topics of words in a document and is the
 * main step of a Gibbs sampling iteration. The word topic counts and
 * corpus topic counts are passed to this function in the first call and
 * then transfered to the rest calls through args.mSysInfo->user_fctx for
 * efficiency.
 * @param args[0]   The unique words in the documents
 * @param args[1]   The counts of each unique words
 * @param args[2]   The topic counts and topic assignments in the document
 * @param args[3]   The model (word topic counts and corpus topic
 *                  counts)
 * @param args[4]   The Dirichlet parameter for per-document topic
 *                  multinomial, i.e. alpha
 * @param args[5]   The Dirichlet parameter for per-topic word
 *                  multinomial, i.e. beta
 * @param args[6]   The size of vocabulary
 * @param args[7]   The number of topics
 * @param args[8]   The number of iterations (=1:training, >1:prediction)
 * @return          The updated topic counts and topic assignments for
 *                  the document
 **/
AnyType lda_gibbs_sample::run(AnyType & args)
{
    ArrayHandle<int32_t> words = args[0].getAs<ArrayHandle<int32_t> >();
    ArrayHandle<int32_t> counts = args[1].getAs<ArrayHandle<int32_t> >();
    MutableArrayHandle<int32_t> doc_topic = args[2].getAs<MutableArrayHandle<int32_t> >();
    double alpha = args[4].getAs<double>();
    double beta = args[5].getAs<double>();
    int32_t voc_size = args[6].getAs<int32_t>();
    int32_t topic_num = args[7].getAs<int32_t>();
    int32_t iter_num = args[8].getAs<int32_t>();
    size_t model64_size =
        static_cast<size_t>(voc_size * (topic_num + 1) + 1) * sizeof(int32_t) / sizeof(int64_t);

    if(alpha <= 0)
        throw std::invalid_argument("invalid argument - alpha");
    if(beta <= 0)
        throw std::invalid_argument("invalid argument - beta");
    if(voc_size <= 0)
        throw std::invalid_argument(
            "invalid argument - voc_size");
    if(topic_num <= 0)
        throw std::invalid_argument(
            "invalid argument - topic_num");
    if(iter_num <= 0)
        throw std::invalid_argument(
            "invalid argument - iter_num");

    if(words.size() != counts.size())
        throw std::invalid_argument(
            "dimensions mismatch: words.size() != counts.size()");
    if(__min(words) < 0 || __max(words) >= voc_size)
        throw std::invalid_argument(
            "invalid values in words");
    if(__min(counts) <= 0)
        throw std::invalid_argument(
            "invalid values in counts");

    int32_t word_count = __sum(counts);
    if(doc_topic.size() != (size_t)(word_count + topic_num))
        throw std::invalid_argument(
            "invalid dimension - doc_topic.size() != word_count + topic_num");
    if(__min(doc_topic, 0, topic_num) < 0)
        throw std::invalid_argument("invalid values in topic_count");
    if(
        __min(doc_topic, topic_num, word_count) < 0 ||
        __max(doc_topic, topic_num, word_count) >= topic_num)
        throw std::invalid_argument( "invalid values in topic_assignment");

    if (!args.getUserFuncContext()) {
        ArrayHandle<int64_t> model64 = args[3].getAs<ArrayHandle<int64_t> >();
        if (model64.size() != model64_size) {
            std::stringstream ss;
            ss << "invalid dimension: model64.size() = " << model64.size();
            throw std::invalid_argument(ss.str());
        }
        if (__min(model64) < 0) {
            throw std::invalid_argument("invalid topic counts in model");
        }

        int32_t *context =
            static_cast<int32_t *>(
                MemoryContextAllocZero(
                    args.getCacheMemoryContext(),
                    model64.size() * sizeof(int64_t)
                        + topic_num * sizeof(int64_t)));
        memcpy(context, model64.ptr(), model64.size() * sizeof(int64_t));
        int32_t *model = context;

        int64_t *running_topic_counts = reinterpret_cast<int64_t *>(
                context + model64_size * sizeof(int64_t) / sizeof(int32_t));
        for (int i = 0; i < voc_size; i ++) {
            for (int j = 0; j < topic_num; j ++) {
                running_topic_counts[j] += model[i * (topic_num + 1) + j];
            }
        }

        args.setUserFuncContext(context);
    }

    int32_t *context = static_cast<int32_t *>(args.getUserFuncContext());
    if (context == NULL) {
        throw std::runtime_error("args.mSysInfo->user_fctx is null");
    }
    int32_t *model = context;
    int64_t *running_topic_counts = reinterpret_cast<int64_t *>(
            context + model64_size * sizeof(int64_t) / sizeof(int32_t));

    int32_t unique_word_count = static_cast<int32_t>(words.size());
    for(int32_t it = 0; it < iter_num; it++){
        int32_t word_index = topic_num;
        for(int32_t i = 0; i < unique_word_count; i++) {
            int32_t wordid = words[i];
            for(int32_t j = 0; j < counts[i]; j++){
                int32_t topic = doc_topic[word_index];
                int32_t retopic = __lda_gibbs_sample(
                    topic_num, topic, doc_topic.ptr(),
                    model + wordid * (topic_num + 1),
                    running_topic_counts, alpha, beta);
                doc_topic[word_index] = retopic;
                doc_topic[topic]--;
                doc_topic[retopic]++;

                if(iter_num == 1) {
                    if (model[wordid * (topic_num + 1) + retopic] <= 2e9) {
                        running_topic_counts[topic] --;
                        running_topic_counts[retopic] ++;
                        model[wordid * (topic_num + 1) + topic]--;
                        model[wordid * (topic_num + 1) + retopic]++;
                    } else {
                        model[wordid * (topic_num + 1) + topic_num] = 1;
                    }
                }
                word_index++;
            }
        }
    }

    return doc_topic;
}
/**
 * @brief This function assigns topics to words in a document randomly and
 * returns the topic counts and topic assignments.
 * @param args[0]   The word count in the documents
 * @param args[1]   The number of topics
 * @result          The topic counts and topic assignments
 *                  (length = topic_num + word_count)
 **/
AnyType lda_random_assign::run(AnyType & args)
{
    int32_t word_count = args[0].getAs<int32_t>();
    int32_t topic_num = args[1].getAs<int32_t>();

    if(word_count < 1)
        throw std::invalid_argument( "invalid argument - word_count");
    if(topic_num < 1)
        throw std::invalid_argument( "invalid argument - topic_num");

    // FIXME: construct_array functions circumvent the abstraction layer. These
    // should be replaced with appropriate Allocator:: calls.
    MutableArrayHandle<int32_t> doc_topic(
        madlib_construct_array(
            NULL, topic_num + word_count, INT4OID, sizeof(int32_t), true, 'i'));

    for(int32_t i = 0; i < word_count; i++){
        int32_t topic = static_cast<int32_t>(random() % topic_num);
        doc_topic[topic] += 1;
        doc_topic[topic_num + i] = topic;
    }

    return doc_topic;
}

/**
 * @brief This function is the sfunc for the aggregator computing the topic
 * counts. It scans the topic assignments in a document and updates the word
 * topic counts.
 * @param args[0]   The state variable, current topic counts
 * @param args[1]   The unique words in the document
 * @param args[2]   The counts of each unique word in the document
 * @param args[3]   The topic assignments in the document
 * @param args[4]   The size of vocabulary
 * @param args[5]   The number of topics
 * @return          The updated state
 **/
AnyType lda_count_topic_sfunc::run(AnyType & args)
{
    if(args[4].isNull() || args[5].isNull())
        throw std::invalid_argument("null parameter - voc_size and/or \
        topic_num is null");

    if(args[1].isNull() || args[2].isNull() || args[3].isNull())
        return args[0];

    int32_t voc_size = args[4].getAs<int32_t>();
    int32_t topic_num = args[5].getAs<int32_t>();
    if(voc_size <= 0)
        throw std::invalid_argument(
            "invalid argument - voc_size");
    if(topic_num <= 0)
        throw std::invalid_argument(
            "invalid argument - topic_num");

    ArrayHandle<int32_t> words = args[1].getAs<ArrayHandle<int32_t> >();
    ArrayHandle<int32_t> counts = args[2].getAs<ArrayHandle<int32_t> >();
    ArrayHandle<int32_t> topic_assignment = args[3].getAs<ArrayHandle<int32_t> >();
    if(words.size() != counts.size())
        throw std::invalid_argument(
            "dimensions mismatch - words.size() != counts.size()");
    if(__min(words) < 0 || __max(words) >= voc_size)
        throw std::invalid_argument(
            "invalid values in words");
    if(__min(counts) <= 0)
        throw std::invalid_argument(
            "invalid values in counts");
    if(__min(topic_assignment) < 0 || __max(topic_assignment) >= topic_num)
        throw std::invalid_argument("invalid values in topics");
    if((size_t)__sum(counts) != topic_assignment.size())
        throw std::invalid_argument(
            "dimension mismatch - sum(counts) != topic_assignment.size()");

    // FIXME: construct_array functions circumvent the abstraction layer. These
    // should be replaced with appropriate Allocator:: calls.
    MutableArrayHandle<int64_t> state(NULL);
    int32_t *model;
    if(args[0].isNull()) {
        // to store a voc_size x (topic_num+1) integer matrix in
        // bigint[] (the +1 is for a flag of ceiling the count),
        // we need padding if the size is odd.
        // 1. when voc_size * (topic_num + 1) is (2n+1), gives (n+1)
        // 2. when voc_size * (topic_num + 1) is (2n), gives (n)
        int dims[1] = {static_cast<int>( (voc_size * (topic_num + 1) + 1) * sizeof(int32_t) / sizeof(int64_t) )};
        int lbs[1] = {1};
        state = madlib_construct_md_array(
            NULL, NULL, 1, dims, lbs, INT8OID, sizeof(int64_t), true, 'd');
        // the reason we use bigint[] because integer[] has limit on number of
        // elements and thus cannot be larger than 500MB
        model = reinterpret_cast<int32_t *>(state.ptr());
    } else {
        state = args[0].getAs<MutableArrayHandle<int64_t> >();
        model = reinterpret_cast<int32_t *>(state.ptr());
    }

    int32_t unique_word_count = static_cast<int32_t>(words.size());
    int32_t word_index = 0;
    for(int32_t i = 0; i < unique_word_count; i++){
        int32_t wordid = words[i];
        for(int32_t j = 0; j < counts[i]; j++){
            int32_t topic = topic_assignment[word_index];
            if (model[wordid * (topic_num + 1) + topic] <= 2e9) {
                model[wordid * (topic_num + 1) + topic]++;
            } else {
                model[wordid * (topic_num + 1) + topic_num] = 1;
            }
            word_index++;
        }
    }
    return state;
}

/**
 * @brief This function is the prefunc for the aggregator computing the
 * topic counts.
 * @param args[0]   The state variable, local topic counts
 * @param args[1]   The state variable, local topic counts
 * @return          The merged state, element-wise sum of two local states
 **/
AnyType lda_count_topic_prefunc::run(AnyType & args)
{
    MutableArrayHandle<int64_t> state1 = args[0].getAs<MutableArrayHandle<int64_t> >();
    ArrayHandle<int64_t> state2 = args[1].getAs<ArrayHandle<int64_t> >();

    if(state1.size() != state2.size())
        throw std::invalid_argument("invalid dimension");

    int32_t *model1 = reinterpret_cast<int32_t *>(state1.ptr());
    const int32_t *model2 = reinterpret_cast<const int32_t *>(state2.ptr());
    for(size_t i = 0; i < state1.size() * sizeof(int64_t) / sizeof(int32_t); i++) {
        model1[i] += model2[i];
    }

    return state1;
}

/**
 * @brief This function transposes a matrix represented by a 2-D array
 * @param args[0]   The input matrix
 * return           The transposed matrix
 **/
AnyType lda_transpose::run(AnyType & args)
{
    ArrayHandle<int64_t> matrix = args[0].getAs<ArrayHandle<int64_t> >();
    if(matrix.dims() != 2)
        throw std::domain_error("invalid dimension");

    int32_t row_num = static_cast<int32_t>(matrix.sizeOfDim(0));
    int32_t col_num = static_cast<int32_t>(matrix.sizeOfDim(1));

    // FIXME: construct_array functions circumvent the abstraction layer. These
    // should be replaced with appropriate Allocator:: calls.
    int dims[2] = {col_num, row_num};
    int lbs[2] = {1, 1};
    MutableArrayHandle<int64_t> transposed(
        madlib_construct_md_array(
            NULL, NULL, 2, dims, lbs, INT8OID, sizeof(int64_t), true, 'd'));

    for(int32_t i = 0; i < row_num; i++){
        int32_t index = i * col_num;
        for(int32_t j = 0; j < col_num; j++){
               transposed[j * row_num + i] = matrix[index];
               index++;
        }
    }

    return transposed;
}

/**
 * @brief This structure defines the states used by the following SRF.
 **/
typedef struct __sr_ctx{
    const int32_t * inarray;
    int32_t maxcall;
    int32_t dim;
    int32_t curcall;
} sr_ctx;

void * lda_unnest_transpose::SRF_init(AnyType &args)
{
    ArrayHandle<int64_t> inarray64 = args[0].getAs<ArrayHandle<int64_t> >();

    sr_ctx * ctx = new sr_ctx;
    ctx->inarray = reinterpret_cast<const int32_t *>(inarray64.ptr());
    ctx->maxcall = args[2].getAs<int32_t>();
    ctx->dim = args[1].getAs<int32_t>();
    ctx->curcall = 0;

    return ctx;
}

/**
 * @brief The function is used to return the next row by the SRF..
 **/
AnyType lda_unnest_transpose::SRF_next(void *user_fctx, bool *is_last_call)
{
    sr_ctx * ctx = (sr_ctx *) user_fctx;
    if (ctx->maxcall == ctx->curcall) {
        *is_last_call = true;
        return Null();
    }

    // FIXME: construct_array functions circumvent the abstraction layer. These
    // should be replaced with appropriate Allocator:: calls.
    MutableArrayHandle<int32_t> outarray(
        madlib_construct_array(
            NULL, ctx->dim, INT4OID, sizeof(int32_t), true, 'i'));

    for (int i = 0; i < ctx->dim; i ++) {
        outarray[i] = ctx->inarray[(ctx->maxcall + 1) * i + ctx->curcall];
    }

    ctx->curcall++;
    *is_last_call = false;

    return outarray;
}
// -----------------------------------------------------------------------

void * lda_unnest::SRF_init(AnyType &args)
{
    ArrayHandle<int64_t> inarray64 = args[0].getAs<ArrayHandle<int64_t> >();

    sr_ctx * ctx = new sr_ctx;
    ctx->inarray = reinterpret_cast<const int32_t *>(inarray64.ptr());
    ctx->maxcall = args[1].getAs<int32_t>();
    ctx->dim = args[2].getAs<int32_t>();
    ctx->curcall = 0;

    return ctx;
}

/**
 * @brief The function is used to return the next row by the SRF..
 **/
AnyType lda_unnest::SRF_next(void *user_fctx, bool *is_last_call)
{
    sr_ctx * ctx = (sr_ctx *) user_fctx;
    if (ctx->maxcall == ctx->curcall) {
        *is_last_call = true;
        return Null();
    }

    // FIXME: construct_array functions circumvent the abstraction layer. These
    // should be replaced with appropriate Allocator:: calls.
    MutableArrayHandle<int32_t> outarray(
        madlib_construct_array(
            NULL, ctx->dim, INT4OID, sizeof(int32_t), true, 'i'));

    for (int i = 0; i < ctx->dim; i ++) {
        outarray[i] = ctx->inarray[ctx->curcall * (ctx->dim + 1) + i];
    }

    ctx->curcall++;
    *is_last_call = false;

    return outarray;
}

/**
 * @brief This function is the sfunc of an aggregator computing the
 * perplexity.
 * @param args[0]   The current state
 * @param args[1]   The unique words in the documents
 * @param args[2]   The counts of each unique words
 * @param args[3]   The topic counts in the document
 * @param args[4]   The model (word topic counts and corpus topic
 *                  counts)
 * @param args[5]   The Dirichlet parameter for per-document topic
 *                  multinomial, i.e. alpha
 * @param args[6]   The Dirichlet parameter for per-topic word
 *                  multinomial, i.e. beta
 * @param args[7]   The size of vocabulary
 * @param args[8]   The number of topics
 * @return          The updated state
 **/
AnyType lda_perplexity_sfunc::run(AnyType & args){
    ArrayHandle<int32_t> words = args[1].getAs<ArrayHandle<int32_t> >();
    ArrayHandle<int32_t> counts = args[2].getAs<ArrayHandle<int32_t> >();
    ArrayHandle<int32_t> doc_topic_counts = args[3].getAs<ArrayHandle<int32_t> >();
    double alpha = args[5].getAs<double>();
    double beta = args[6].getAs<double>();
    int32_t voc_size = args[7].getAs<int32_t>();
    int32_t topic_num = args[8].getAs<int32_t>();
    size_t model64_size = static_cast<size_t>(voc_size * (topic_num + 1) + 1) * sizeof(int32_t) / sizeof(int64_t);

    if(alpha <= 0)
        throw std::invalid_argument("invalid argument - alpha");
    if(beta <= 0)
        throw std::invalid_argument("invalid argument - beta");
    if(voc_size <= 0)
        throw std::invalid_argument(
            "invalid argument - voc_size");
    if(topic_num <= 0)
        throw std::invalid_argument(
            "invalid argument - topic_num");

    if(words.size() != counts.size())
        throw std::invalid_argument(
            "dimensions mismatch: words.size() != counts.size()");
    if(__min(words) < 0 || __max(words) >= voc_size)
        throw std::invalid_argument(
            "invalid values in words");
    if(__min(counts) <= 0)
        throw std::invalid_argument(
            "invalid values in counts");

    if(doc_topic_counts.size() != (size_t)(topic_num))
        throw std::invalid_argument(
            "invalid dimension - doc_topic_counts.size() != topic_num");
    if(__min(doc_topic_counts, 0, topic_num) < 0)
        throw std::invalid_argument("invalid values in doc_topic_counts");

    MutableArrayHandle<int64_t> state(NULL);
    if (args[0].isNull()) {
        ArrayHandle<int64_t> model64 = args[4].getAs<ArrayHandle<int64_t> >();

        if (model64.size() != model64_size) {
            std::stringstream ss;
            ss << "invalid dimension: model64.size() = " << model64.size();
            throw std::invalid_argument(ss.str());
        }
        if(__min(model64) < 0) {
            throw std::invalid_argument("invalid topic counts in model");
        }

        // FIXME: construct_array functions circumvent the abstraction layer. These
        // should be replaced with appropriate Allocator:: calls.
        state =  madlib_construct_array(NULL,
                                        static_cast<int>(model64.size()) +
                                            topic_num +
                                            static_cast<int>(sizeof(double) / sizeof(int64_t)),
                                        INT8OID, sizeof(int64_t), true, 'd');

        memcpy(state.ptr(), model64.ptr(), model64.size() * sizeof(int64_t));
        int32_t *_model = reinterpret_cast<int32_t *>(state.ptr());
        int64_t *_total_topic_counts = reinterpret_cast<int64_t *>(state.ptr() + model64.size());
        for (int i = 0; i < voc_size; i ++) {
            for (int j = 0; j < topic_num; j ++) {
                _total_topic_counts[j] += _model[i * (topic_num + 1) + j];
            }
        }
    } else {
        state = args[0].getAs<MutableArrayHandle<int64_t> >();
    }

    int32_t *model = reinterpret_cast<int32_t *>(state.ptr());
    int64_t *total_topic_counts = reinterpret_cast<int64_t *>(state.ptr() + model64_size);
    double *perp = reinterpret_cast<double *>(state.ptr() + state.size() - 1);

    int32_t n_d = 0;
    for(size_t i = 0; i < words.size(); i++){
        n_d += counts[i];
    }

    for(size_t i = 0; i < words.size(); i++){
        int32_t w = words[i];
        int32_t n_dw = counts[i];

        double sum_p = 0.0;
        for(int32_t z = 0; z < topic_num; z++){
                int32_t n_dz = doc_topic_counts[z];
                int32_t n_wz = model[w * (topic_num + 1) + z];
                int64_t n_z = total_topic_counts[z];
                sum_p += (static_cast<double>(n_wz) + beta) * (n_dz + alpha)
                            / (static_cast<double>(n_z) + voc_size * beta);
        }
        sum_p /= (n_d + topic_num * alpha);

        *perp += n_dw * log(sum_p);
    }

    return state;
}

/**
 * @brief This function is the prefunc of an aggregator computing the
 * perplexity.
 * @param args[0]   The local state
 * @param args[1]   The local state
 * @return          The merged state
 **/
AnyType lda_perplexity_prefunc::run(AnyType & args){
    MutableArrayHandle<int64_t> state1 = args[0].getAs<MutableArrayHandle<int64_t> >();
    ArrayHandle<int64_t> state2 = args[1].getAs<ArrayHandle<int64_t> >();
    double * perp1 = reinterpret_cast<double *>(state1.ptr() + state1.size() - 1);
    const double * perp2 = reinterpret_cast<const double *>(state2.ptr() + state2.size() - 1);
    *perp1 += *perp2;
    return state1;
}

/**
 * @brief This function is the finalfunc of an aggregator computing the
 * perplexity.
 * @param args[0]   The global state
 * @return          The perplexity
 **/
AnyType lda_perplexity_ffunc::run(AnyType & args){
    ArrayHandle<int64_t> state = args[0].getAs<ArrayHandle<int64_t> >();
    const double * perp = reinterpret_cast<const double *>(state.ptr() + state.size() - 1);
    return *perp;
}

AnyType
lda_check_count_ceiling::run(AnyType &args) {
    ArrayHandle<int64_t> model64 = args[0].getAs<ArrayHandle<int64_t> >();
    int voc_size = args[1].getAs<int32_t>();
    int topic_num = args[2].getAs<int32_t>();

    int example_words_hit_ceiling[10];
    int count = 0;
    const int32_t *model = reinterpret_cast<const int32_t *>(model64.ptr());
    for (int wordid = 0; wordid < voc_size; wordid ++) {
        int flag = model[wordid * (topic_num + 1) + topic_num];
        if (flag != 0) {
            example_words_hit_ceiling[count ++] = wordid;
        }
    }

    if (count == 0) { return Null(); }

    MutableNativeIntegerVector ret(this->allocateArray<int>(count));
    memcpy(ret.data(), example_words_hit_ceiling, count * sizeof(int));
    return ret;
}

AnyType l1_norm_with_smoothing::run(AnyType & args){
    MutableArrayHandle<double> arr = args[0].getAs<MutableArrayHandle<double> >();
    double smooth = args[1].getAs<double>();
    smooth = fabs(smooth);

    double sum = 0.0;
    for(size_t i = 0; i < arr.size(); i++)
        sum += fabs(arr[i]);
    sum += smooth * static_cast<double>(arr.size());

    double inverse_sum = 0.0;
    if (sum != 0.0)
        inverse_sum = 1.0 / sum;
    for(size_t i = 0; i < arr.size(); i++)
        arr[i] = (arr[i] + smooth) * inverse_sum;
    return arr;
}

AnyType lda_parse_model::run(AnyType & args){
    ArrayHandle<int64_t> state = args[0].getAs<ArrayHandle<int64_t> >();
    int32_t voc_size = args[1].getAs<int32_t>();
    int32_t topic_num = args[2].getAs<int32_t>();

    const int32_t *model = reinterpret_cast<const int32_t *>(state.ptr());

    // FIXME: construct_array functions circumvent the abstraction layer. These
    // should be replaced with appropriate Allocator:: calls.
    int dims[2] = {voc_size/2, topic_num};
    int lbs[2] = {1, 1};
    MutableArrayHandle<int32_t> model_part1(
        madlib_construct_md_array(
            NULL, NULL, 2, dims, lbs, INT4OID, sizeof(int32_t), true, 'i'));

    for(int32_t i = 0; i < voc_size/2; i++){
        for(int32_t j = 0; j < topic_num; j++){
               model_part1[i * topic_num + j] = model[i * (topic_num+1) + j];
        }
    }

    // FIXME: construct_array functions circumvent the abstraction layer. These
    // should be replaced with appropriate Allocator:: calls.
    int dims2[2] = {voc_size - voc_size/2, topic_num};
    MutableArrayHandle<int32_t> model_part2(
        madlib_construct_md_array(
            NULL, NULL, 2, dims2, lbs, INT4OID, sizeof(int32_t), true, 'i'));

    for(int32_t i = voc_size/2; i < voc_size; i++){
        for(int32_t j = 0; j < topic_num; j++){
               model_part2[(i-voc_size/2) * topic_num + j] = model[i * (topic_num+1) + j];
        }
    }

    //int dims3[1] = {topic_num};
    //int lbs3[1] = {1};

    MutableNativeColumnVector total_topic_counts(allocateArray<double>(topic_num));

    for (int i = 0; i < voc_size; i ++) {
        for (int j = 0; j < topic_num; j ++) {
            total_topic_counts[j] += static_cast<double>(model[i * (topic_num + 1) + j]);
        }
    }

    AnyType tuple;
    tuple << model_part1
          << model_part2
          << total_topic_counts;

    return tuple;
}

}
}
}
