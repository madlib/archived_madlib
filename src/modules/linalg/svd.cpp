/* ----------------------------------------------------------------------- *//**
 *
 * @file svd.cpp
 *
 * @brief Functions for Singular Value Decomposition
 *
 * @date Jul 10, 2013
 *//* ----------------------------------------------------------------------- */


#include <dbconnector/dbconnector.hpp>
#include <math.h>
#include <iostream>
#include <algorithm>
#include <functional>
#include <numeric>
#include "svd.hpp"
#include <Eigen/SVD>

namespace madlib {

using namespace dbal::eigen_integration;// Use Eigen

namespace modules {
namespace linalg {

using madlib::dbconnector::postgres::madlib_construct_array;

// To get a rank-k approximation of the original matrix if we perform k + s Lanczos
// bidiagonalization steps followed by the SVD of a small matrix B(k+s) then the
// algorithm constructs the best rank-k subspace in an extended subspace
// Span[U(k+s)]. Hence we obtain a better rank-k approximation than the one
// obtained after k steps steps of the standard Lanczos bidiagonalization algorithm.
// There is a memory limit to the number of extended steps and we restrict that to
// fixed number of steps for now.
// Magic number computed using the 1GB memory limit.
// MAX_LANCZOS_STEPS^2 < 10^9 bytes / (8 bytes * 3 matrices)
const size_t MAX_LANCZOS_STEPS = 5000;

// For floating point equality comparisons,
// it is safer to define a small range of values
// that are "zero", rather than use the exact value of 0.
const double ZERO_THRESHOLD = 1e-8;

/* Project the vector v into the vector u (in-place) */
static void __project(MappedColumnVector& u, MutableNativeColumnVector& v){
    double uu = u.dot(u);
    double uv = u.dot(v);

    double coef;
    if (uu <= ZERO_THRESHOLD) { // if u is the zero vector, we have a division by zero problem
        coef = 0;
    } else
        coef = uv / uu;
    v = coef * u;
}


/**
 * @brief This function returns a random normalized unit vector of specified size
 * @param args[0]   The dimension
 * @return          The unit-norm vector
 **/
AnyType svd_unit_vector::run(AnyType & args)
{
    int32_t dim = args[0].getAs<int32_t>();

    if(dim < 1){
        throw std::invalid_argument(
            "invalid argument - Positive integer expected for dimension");
    }

    MutableNativeColumnVector vectorEigen;
    Allocator& allocator = defaultAllocator();
    vectorEigen.rebind(allocator.allocateArray<double>(dim));
    vectorEigen.setRandom();
    vectorEigen = vectorEigen.normalized();

    // AnyType tuple;
    // tuple << vectorEigen;
    // return tuple;

    return vectorEigen;
}

/**
 * @brief This function is the transition function of the aggregator computing the Lanczos vectors
 * @param args[0]   State variable (i.e. A * q_j OR A_trans * p_(j-1))
 * @param args[1]   Matrix row id
 * @param args[2]   Matrix row array
 * @param args[3]   Previous P/Q vector
 * @param args[4]   Row/Column dimension
 **/
AnyType svd_lanczos_sfunc::run(AnyType & args){
    int32_t row_id = args[1].getAs<int32_t>();
    MappedColumnVector row_array = args[2].getAs<MappedColumnVector >();
    MappedColumnVector vec = args[3].getAs<MappedColumnVector >();
    int32_t dim = args[4].getAs<int32_t>();

    if(dim < 1){
        throw std::invalid_argument(
            "invalid argument - Positive integer expected for dimension");
    }

    if(row_id <= 0 || row_id > dim){
        throw std::invalid_argument(
            "invalid argument: row_id is out of range [1, dim]");
    }

    if(row_array.size() != vec.size()){
        throw std::invalid_argument(
            "dimensions mismatch: row_array.size() != vec.size(). "
            "Data contains different sized arrays");
    }

    // FIXME: construct_array functions circumvent the abstraction layer. These
    // should be replaced with appropriate Allocator:: calls.
    MutableArrayHandle<double> state(NULL);
    if(args[0].isNull()){
        state = MutableArrayHandle<double>(
            madlib_construct_array(
                NULL, dim, FLOAT8OID, sizeof(double), true, 'd'));
        for (int i = 0; i < dim; i++)
            state[i] = 0;
    }else{
        state = args[0].getAs<MutableArrayHandle<double> >();
    }

    state[row_id - 1] = row_array.dot(vec);

    return state;
}

/**
 * @brief This function is the merge function of the aggregator computing the Lanczos vectors
 * @param args[0]   State variable 1
 * @param args[1]   State variable 2
 **/
AnyType svd_lanczos_prefunc::run(AnyType & args){
    MutableArrayHandle<double> state1 = args[0].getAs<MutableArrayHandle<double> >();
    ArrayHandle<double> state2 = args[1].getAs<ArrayHandle<double> >();

    if(state1.size() != state2.size()){
        throw std::runtime_error("dimension mismatch: state1.size() != state2.size()");
    }

    for(size_t i = 0; i < state1.size(); i++)
        state1[i] += state2[i];

    return state1;
}

/**
 * @breif This function completes the computation of Lanczoc P vector
 * @param args[0]   Partial P vector from the aggregator
 * @param args[1]   Previous P vector
 * @param args[2]   Previous beta
 **/
AnyType svd_lanczos_pvec::run(AnyType & args){
    MutableNativeColumnVector partial_pvec = args[0].getAs<MutableNativeColumnVector>();

    // When args[1] is NULL, it's special case for computing p_1
    if (!args[1].isNull()){
        MappedColumnVector prev_pvec = args[1].getAs< MappedColumnVector >();
        double beta = args[2].getAs<double>();

        if(partial_pvec.size() != prev_pvec.size()){
            throw std::invalid_argument(
                "dimension mismatch: partial_pvec.size() != prev_pvec.size()");
        }
        partial_pvec = partial_pvec - beta * prev_pvec;
    }

    double norm = partial_pvec.norm();
    partial_pvec.normalize();

    AnyType tuple;
    tuple << norm << partial_pvec;
    return tuple;
}

/**
 * @breif This function completes the computation of Lanczoc Q vector
 * @param args[0]   Partial Q vector from the aggregator
 * @param args[1]   Previous Q vector
 * @param args[2]   Current alpha
 **/
AnyType svd_lanczos_qvec::run(AnyType & args){
    MutableNativeColumnVector partial_qvec = args[0].getAs<MutableNativeColumnVector>();

    MappedColumnVector prev_qvec = args[1].getAs< MappedColumnVector >();
    double alpha = args[2].getAs<double>();

    if(partial_qvec.size() != prev_qvec.size()){
        throw std::invalid_argument(
            "dimension mismatch: partial_qvec.size() != prev_qvec.size()");
    }

    partial_qvec = partial_qvec - alpha * prev_qvec;

    // Different with svd_lanczos_pvec, the Q vector will be furhter orthogonalized
    // and then be normalized in a separate function
    return partial_qvec;
}

/**
 * @brief This function is the transition function of the aggregaror doing the Gram-Schmidt orthogonalization
 * @param args[0]   State variable: sum of projected vectors | vector v
 * @param args[1]   Unorthogonalized vector (v)
 * @param args[2]   Orthogonalized vector (u)
 **/
AnyType svd_gram_schmidt_orthogonalize_sfunc::run(AnyType & args){
	MutableNativeColumnVector v = args[1].getAs<MutableNativeColumnVector >();
	MappedColumnVector u = args[2].getAs<MappedColumnVector >();


    if(u.size() != v.size()){
        throw std::invalid_argument(
            "dimensions mismatch: u.size() != v.size()");
    }

    // FIXME: construct_array functions circumvent the abstraction layer. These
    // should be replaced with appropriate Allocator:: calls.
    MutableArrayHandle<double> state(NULL);
    if(args[0].isNull()){
        state = MutableArrayHandle<double>(
            madlib_construct_array(NULL,
                                   static_cast<int>(u.size()) * 2,
                                   FLOAT8OID, sizeof(double), true, 'd'));

        // Save v into the state variable
        memcpy(state.ptr() + u.size(), v.data(), v.size() * sizeof(double));
    }else{
        state = args[0].getAs<MutableArrayHandle<double> >();
    }

    // In-place projection
    __project(u, v);

    for(int i = 0; i < u.size(); i++){
        state[i] += v[i];
    }

    return state;
}

/**
 * @brief This function is the merge function of the aggregator doing the Gram-Schmidt orthogonalization
 * @param args[0]   State variable 1
 * @param args[1]   State variable 2
 **/
AnyType svd_gram_schmidt_orthogonalize_prefunc::run(AnyType & args){
    MutableArrayHandle<double> state1 = args[0].getAs<MutableArrayHandle<double> >();
    ArrayHandle<double> state2 = args[1].getAs<ArrayHandle<double> >();

    if(state1.size() != state2.size()){
        throw std::runtime_error("dimension mismatch: state1.size() != state2.size()");
    }

    // Note that the second half of the state variable stores the vector v
    for(size_t i = 0; i < state1.size() / 2; i++)
        state1[i] += state2[i];

    return state1;
}

/**
 * @brief This function is the final function of the aggregator doing the Gram-Schmidt orthogonalization
 * @param args[0]   State variable
 **/
AnyType svd_gram_schmidt_orthogonalize_ffunc::run(AnyType & args){
    ArrayHandle<double> state = args[0].getAs<ArrayHandle<double> >();

    MutableNativeColumnVector u;
    Allocator& allocator = defaultAllocator();
    u.rebind(allocator.allocateArray<double>(state.size() / 2));

    for(int i = 0; i < u.size(); i++){
        u[i] = state[u.size() + i] - state[i];
    }

    double norm = u.norm();
    u.normalize();

    AnyType tuple;
    tuple << norm << u;

    return tuple;
}

// -- SVD Decomposition for Bidiagonal Matrix --------------------------------
/**
 * @brief This function is the transition function of the aggregator computing the SVD
 * of a sparse bidiagonal matrix
 * @param args[0]   State variable (in-memory dense matrix)
 * @param args[1]   Dimension of the matrix (i.e. k)
 * @param args[2]   Row ID (i.e. row_id)
 * @param args[3]   Column ID (i.e. col_id)
 * @param args[4]   Value
 **/
AnyType svd_decompose_bidiagonal_sfunc::run(AnyType & args){
    if(args[1].isNull() || args[2].isNull()
        || args[3].isNull() || args[4].isNull())
        return args[0];

    int32_t k = args[1].getAs<int32_t>();
    int32_t row_id = args[2].getAs<int32_t>();
    int32_t col_id = args[3].getAs<int32_t>();
    double value = args[4].getAs<double>();

    if(k < 0){
        throw std::invalid_argument(
            "SVD error: k should be a positive integer");
    }
    if(k > (int)MAX_LANCZOS_STEPS){
        throw std::invalid_argument(
            "SVD error: k is too large, try with a value in the range of [1, 6000]");
    }
    if(row_id <= 0 || row_id > k){
        throw std::invalid_argument(
            "SVD error: row_id should be in the range of [1, k]");
    }
    if(col_id <= 0 || col_id > k){
        throw std::invalid_argument(
            "invalid parameter: col_id should be in the range of [1, k]");
    }

    // FIXME: construct_array functions circumvent the abstraction layer. These
    // should be replaced with appropriate Allocator:: calls.
    MutableArrayHandle<double> state(NULL);
    if(args[0].isNull()){
        state = MutableArrayHandle<double>(
            madlib_construct_array(NULL, k * k, FLOAT8OID, sizeof(double), true, 'd'));
    } else {
        state = args[0].getAs<MutableArrayHandle<double> >();
    }

    state[(row_id - 1) * k + col_id - 1] = value;
    return state;
}

/**
 * @brief This function is the merge function of the aggregator computing the SVD
 * of a sparse bidiagonal matrix
 * @param args[0]   State variable 1
 * @param args[1]   State varaible 2
 **/
AnyType svd_decompose_bidiagonal_prefunc::run(AnyType & args){
    MutableArrayHandle<double> state1 = args[0].getAs<MutableArrayHandle<double> >();
    ArrayHandle<double> state2 = args[1].getAs<ArrayHandle<double> >();

    if(state1.size() != state2.size()){
        throw std::runtime_error("dimension mismatch: state1.size() != state2.size()");
    }

    for(size_t i = 0; i < state1.size(); i++){
        state1[i] += state2[i];
    }

    return state1;
}

/**
 * @brief Take the final matrix and run it by Eigen JacobiSVD to get the left and right
    decompositions along with the eigen values
 **/
AnyType svd_decompose_bidiagonal_ffunc::run(AnyType & args){
    MappedColumnVector state = args[0].getAs<MappedColumnVector>();
    size_t k = static_cast<size_t>(sqrt(static_cast<double>(state.size())));

    // Note that Eigen Matrix deserializes the vector in the column order
    // Thus transpose() is needed after resize()
    Matrix b = state;
    b.resize(k, k);
    b.transposeInPlace();
    Eigen::JacobiSVD<Matrix> svd(b, Eigen::ComputeThinU | Eigen::ComputeThinV);

    // Note that AnyType serializes the Matrix Object in the column order
    // Thus transpose() is needed before output
    Matrix u = svd.matrixU().transpose();
    Matrix v = svd.matrixV().transpose();
    Matrix s = svd.singularValues();

    AnyType tuple;
    tuple << u << v << s;
    return tuple;
}

AnyType svd_decompose_bidiag::run(AnyType & args){

    // <row_id, col_id, value> triple indicate the values of a bidiagonal matrix
    ArrayHandle<int32_t> row_id = args[0].getAs<ArrayHandle<int32_t> >();
    ArrayHandle<int32_t> col_id = args[1].getAs<ArrayHandle<int32_t> >();
    MappedColumnVector value = args[2].getAs<MappedColumnVector>();

    // since row_id, col_id start indexing from 1, the max element indicates the
    // dimension of of the bidiagonal matrix
    int32_t row_dim = *std::max_element(row_id.ptr(), row_id.ptr() + row_id.size());
    int32_t col_dim = *std::max_element(col_id.ptr(), col_id.ptr() + col_id.size());

    Matrix b = Matrix::Zero(row_dim, col_dim);
    for(size_t i = 0; i < row_id.size(); i++){
        // we use -1 since row_id and col_id start from 1
        b(row_id[i] - 1, col_id[i] - 1) = value[i];
    }

    Eigen::JacobiSVD<Matrix> svd(b, Eigen::ComputeThinU | Eigen::ComputeThinV);

    // Note that AnyType serializes the Matrix Object in the column order
    // Thus transpose() is needed before output
    Matrix u = svd.matrixU().transpose();
    Matrix v = svd.matrixV().transpose();
    Matrix s = svd.singularValues();

    AnyType tuple;
    tuple << u << v << s;
    return tuple;
}

/**
 * @brief This function is the transition function of the aggregator computing the Lanczos vectors
 * @param args[0]   State variable (i.e. A * q_j OR A_trans * p_(j-1))
 * @param args[1]   Matrix block row id
 * @param args[2]   Matrix block col id
 * @param args[3]   Matrix block
 * @param args[4]   Previous P/Q vector
 * @param args[5]   Row/Column dimension
 **/
AnyType svd_block_lanczos_sfunc::run(AnyType & args){
    int32_t row_id = args[1].getAs<int32_t>();
    int32_t col_id = args[2].getAs<int32_t>();
    MappedMatrix block = args[3].getAs<MappedMatrix>();
    MappedColumnVector vec = args[4].getAs<MappedColumnVector >();
    int32_t dim = args[5].getAs<int32_t>();

    if(row_id <= 0){
        throw std::invalid_argument(
            "SVD error: row_id should be in the range of [1, dim]");
    }
    if(col_id <= 0){
        throw std::invalid_argument(
            "invalid parameter: col_id should be in the range of [1, dim]");
    }

    // FIXME: construct_array functions circumvent the abstraction layer. These
    // should be replaced with appropriate Allocator:: calls.
    MutableArrayHandle<double> state(NULL);
    if(args[0].isNull()){
        state = MutableArrayHandle<double>(
            madlib_construct_array(
                NULL, dim, FLOAT8OID, sizeof(double), true, 'd'));
    }else{
        state = args[0].getAs<MutableArrayHandle<double> >();
    }

    // Note that block is constructed in the column-major order
    size_t row_size = block.cols();
    size_t col_size = block.rows();

    Matrix v = block.transpose() * vec.segment((col_id - 1) * col_size, col_size);
    for(int32_t i = 0; i < v.rows(); i++)
        state[(row_id - 1) * row_size + i] += v.col(0)[i];

    return state;
}

/**
 * @brief This function is the transition function of the aggregator computing the Lanczos vectors
 * @param args[0]   State variable (i.e. A * q_j OR A_trans * p_(j-1))
 * @param args[1]   Row ID
 * @param args[2]   Column ID
 * @param args[3]   Value
 * @param args[4]   Previous P/Q vector
 * @param args[5]   Row/Column dimension
 **/
AnyType svd_sparse_lanczos_sfunc::run(AnyType & args){
    int32_t row_id = args[1].getAs<int32_t>();
    int32_t col_id = args[2].getAs<int32_t>();
    double value = args[3].getAs<double>();

    MappedColumnVector vec = args[4].getAs<MappedColumnVector >();
    int32_t dim = args[5].getAs<int32_t>();

    // FIXME: construct_array functions circumvent the abstraction layer. These
    // should be replaced with appropriate Allocator:: calls.
    MutableArrayHandle<double> state(NULL);
    if(args[0].isNull()){
        state = MutableArrayHandle<double>(
            madlib_construct_array(
                NULL, dim, FLOAT8OID, sizeof(double), true, 'd'));
    }else{
        state = args[0].getAs<MutableArrayHandle<double> >();
    }

    state[row_id - 1] += value * vec[col_id - 1];
    return state;
}

/*
 *  @brief In-memory multiplication of a vector with a matrix
 *  @param vec  a 1 x r vector
 *  @param mat  a r x n matrix
 *  @param k    a positive number < n
 *  @note first cut mat to r x k, then return vec * mat
 */
AnyType svd_vec_mult_matrix::run(AnyType & args){
    MappedColumnVector vec = args[0].getAs<MappedColumnVector>();
    MappedMatrix mat = args[1].getAs<MappedMatrix>();
    int32_t k = args[2].getAs<int32_t>();

    // Any integer is ok
    if(k <= 0 || k > mat.rows()){
        k = static_cast<int32_t>(mat.rows());
    }

    // Note mat is constructed in the column-first order
    // which means that mat is actually transposed
    if(vec.size() != mat.cols()){
        throw std::invalid_argument(
            "dimensions mismatch: vec.size() != matrix.rows()");
    };

    // trans(vec) * trans(mat) = mat * vec
    Matrix r = mat.topRows(k) * vec;
    ColumnVector v = r.col(0);
    return v;
}

typedef struct __sr_ctx{
    ColumnVector vec;
    Matrix mat;
    int32_t max_call;
    int32_t cur_call;
    int32_t row_id;
    int32_t k;
} sr_ctx;

/**
 * @param arg[0]    Column vector
 * @param arg[1]    Matrix (l x l)
 * @param args[2]   Column ID
 # @param arg[3]    k (Sub-matrix: l * k)
 **/
void * svd_vec_trans_mult_matrix::SRF_init(AnyType &args){
    sr_ctx * ctx = new sr_ctx;
    ctx->vec = args[0].getAs<MappedColumnVector>();
    ctx->mat = args[1].getAs<MappedMatrix>().transpose();
    ctx->row_id = args[2].getAs<int32_t>();
    ctx->k = args[3].getAs<int32_t>();

    if(ctx->row_id <= 0 || ctx->row_id > ctx->mat.rows()){
        elog(ERROR,
            "invalid parameter - row_id should be in the range of [1, mat.rows()]");
    }

    if(ctx->k > ctx->mat.cols()){
        elog(ERROR,
            "invalid parameter - k should be in the range of [0, mat.cols()]");
    }

    ctx->max_call = static_cast<int32_t>(ctx->vec.size());
    ctx->cur_call = 0;

    return ctx;
}

AnyType svd_vec_trans_mult_matrix::SRF_next(void *user_fctx, bool *is_last_call){
    sr_ctx * ctx = (sr_ctx *) user_fctx;
    if (ctx->max_call == 0) {
        *is_last_call = true;
        return Null();
    }

    ColumnVector res = ctx->vec[ctx->cur_call] *
                            ctx->mat.row(ctx->row_id - 1).segment(0, ctx->k);
    AnyType tuple;
    tuple << ctx->cur_call << res;

    ctx->cur_call++;
    ctx->max_call--;
    *is_last_call = false;

    return tuple;
}

} //namespace linalg
} // namespace modules
} //namespace madlib
