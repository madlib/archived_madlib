/* ----------------------------------------------------------------------- *//**
 *
 * @file matrix_op.cpp
 *
 * @date May 8, 2013
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>
#include <modules/shared/HandleTraits.hpp>

#include <math.h>
#include <iostream>
#include <algorithm>
#include <functional>
#include <numeric>
#include "matrix_op.hpp"

namespace madlib {
namespace modules {
namespace linalg {

using madlib::dbconnector::postgres::madlib_get_typlenbyvalalign;
using madlib::dbconnector::postgres::madlib_construct_array;
using madlib::dbconnector::postgres::madlib_construct_md_array;

// Use Eigen
using namespace dbal::eigen_integration;


typedef struct __type_info{
    Oid oid;
    int16_t len;
    bool    byval;
    char    align;

    __type_info(Oid oid):oid(oid)
    {
        madlib_get_typlenbyvalalign(oid, &len, &byval, &align);
    }
} type_info;
static type_info FLOAT8TI(FLOAT8OID);
static type_info INT4TI(INT4OID);

AnyType matrix_densify_sfunc::run(AnyType & args)
{
    int32_t col_dim = args[1].getAs<int32_t>();
    int32_t col = args[2].getAs<int32_t>();
    double val = args[3].getAs<double>();

    if(col_dim < 1){
        throw std::invalid_argument(
            "invalid argument - col_dim should be positive");
    }

    if(col >= col_dim){
        throw std::invalid_argument(
            "invalid argument - col should be in the range of [0, col_dim)");
    }

    MutableArrayHandle<double> state(NULL);
    if (args[0].isNull()){
        state =  madlib_construct_array(
            NULL, col_dim, FLOAT8TI.oid, FLOAT8TI.len, FLOAT8TI.byval,
            FLOAT8TI.align);
    }else{
        state = args[0].getAs<MutableArrayHandle<double> >();
    }

    *(state.ptr() + col) = val;

    return state;
}

AnyType matrix_mem_sum_sfunc::run(AnyType & args)
{
    ArrayHandle<double> m = args[1].getAs<ArrayHandle<double> >();

    if (m.dims() !=2){
        throw std::invalid_argument(
            "invalid argument - 2-d array expected");
    }

    int row_m = m.sizeOfDim(0);
    int col_m = m.sizeOfDim(1);

    MutableArrayHandle<double> state(NULL);
    if (args[0].isNull()){
        int dims[2] = {row_m, col_m};
        int lbs[2] = {1, 1};
        state =  madlib_construct_md_array(
            NULL, NULL, 2, dims, lbs, FLOAT8TI.oid,
            FLOAT8TI.len, FLOAT8TI.byval, FLOAT8TI.align);
    }else{
        state = args[0].getAs<MutableArrayHandle<double> >();
    }

    for(int i = 0; i < row_m; i++){
        for(int j = 0; j < col_m; j++){
            *(state.ptr() + i * col_m + j) += *(m.ptr() + i * col_m + j);
        }
    }

    return state;
}

AnyType matrix_blockize_sfunc::run(AnyType & args)
{
    if(args[1].isNull())
        return args[0];

    int32_t row_id = args[1].getAs<int32_t>();
    ArrayHandle<double> row_vec  = args[2].getAs<ArrayHandle<double> >();
    int32_t csize = row_vec.sizeOfDim(0);
    int32_t rsize = args[3].getAs<int32_t>();
    if(rsize < 1){
        throw std::invalid_argument(
            "invalid argument - block size should be positive");
    }

    MutableArrayHandle<double> state(NULL);
    if (args[0].isNull()){
        int dims[2] = {rsize, csize};
        int lbs[2] = {1, 1};
        state = madlib_construct_md_array(
                NULL, NULL, 2, dims, lbs, FLOAT8TI.oid,
                FLOAT8TI.len, FLOAT8TI.byval, FLOAT8TI.align);
    }else{
        state = args[0].getAs<MutableArrayHandle<double> >();
    }

    memcpy(
        state.ptr() + (row_id % rsize) * csize, row_vec.ptr(),
            csize * sizeof(double));

    return state;
}

AnyType matrix_mem_mult::run(AnyType & args)
{
    ArrayHandle<double> a = args[0].getAs<ArrayHandle<double> >();
    ArrayHandle<double> b = args[1].getAs<ArrayHandle<double> >();
    bool trans_b = args[2].getAs<bool>();

    if (a.dims() != 2 || b.dims() !=2){
        throw std::invalid_argument(
            "invalid argument - 2-d array expected");
    }

    int row_a = a.sizeOfDim(0);
    int col_a = a.sizeOfDim(1);
    int row_b = b.sizeOfDim(0);
    int col_b = b.sizeOfDim(1);

    if ((!trans_b && col_a != row_b) || (trans_b && col_a != col_b)){
        throw std::invalid_argument(
            "invalid argument - dimension mismatch");
    }

    int dims[2] = {row_a, col_b};
    if (trans_b)
        dims[1] = row_b;
    int lbs[2] = {1, 1};
    MutableArrayHandle<double> r = madlib_construct_md_array(
            NULL, NULL, 2, dims, lbs, FLOAT8TI.oid,
            FLOAT8TI.len, FLOAT8TI.byval, FLOAT8TI.align);

    for (int i = 0; i < row_a; i++){
        for(int j = 0; j < col_a; j++){
            for(int k = 0; k < dims[1]; k++){
                *(r.ptr() + i * dims[1] + k) += trans_b ?
                    *(a.ptr() + i * col_a + j) * *(b.ptr() + k * col_b + j) :
                        *(a.ptr() + i * col_a + j) * *(b.ptr() + j * col_b + k);
            }
        }
    }
    return r;
}

// AnyType matrix_vec_mem_mult::run(AnyType &args){
//     // MappedMatrix left_matrix = args[0].getAs<MappedMatrix>();
//     MappedColumnVector right_vec = args[1].getAs<MappedColumnVector>();
//     MappedColumnVector left_vec = args[0].getAs<MappedColumnVector>();
//     return left_vec * right_vec;
// }

AnyType matrix_mem_trans::run(AnyType & args)
{
    ArrayHandle<double> m = args[0].getAs<ArrayHandle<double> >();

    if (m.dims() != 2){
        throw std::invalid_argument(
            "invalid argument - 2-d array expected");
    }

    int row_m = m.sizeOfDim(0);
    int col_m = m.sizeOfDim(1);

    int dims[2] = {col_m, row_m};
    int lbs[2] = {1, 1};
    MutableArrayHandle<double> r = madlib_construct_md_array(
            NULL, NULL, 2, dims, lbs, FLOAT8TI.oid,
            FLOAT8TI.len, FLOAT8TI.byval, FLOAT8TI.align);

    for (int i = 0; i < row_m; i++){
        for(int j = 0; j < col_m; j++){
                *(r.ptr() + j * row_m + i) = *(m.ptr() + i * col_m + j);
        }
    }
    return r;
}

AnyType rand_vector::run(AnyType & args)
{
    int dim = args[0].getAs<int>();
    if (dim < 1) {
        throw std::invalid_argument("invalid argument - dim should be positive");
    }
    MutableArrayHandle<int> r =  madlib_construct_array(
            NULL, dim, INT4TI.oid, INT4TI.len, INT4TI.byval, INT4TI.align);

    for (int i = 0; i < dim; i++){
        *(r.ptr() + i) = (int)(drand48() * 1000);
    }
    return r;
}

AnyType rand_block::run(AnyType & args)
{
    int row_dim = args[0].getAs<int>();
    int col_dim = args[1].getAs<int>();
    if (row_dim < 1 || col_dim < 1) {
        throw std::invalid_argument("invalid argument - row_dim and col_dim \
        should be positive");
    }

    int dims[2] = {row_dim, col_dim};
    int lbs[2] = {1, 1};
    MutableArrayHandle<int> r = madlib_construct_md_array(
            NULL, NULL, 2, dims, lbs, INT4TI.oid,
            INT4TI.len, INT4TI.byval, INT4TI.align);

    for (int i = 0; i < row_dim; i++){
        for(int j = 0; j < col_dim; j++){
                *(r.ptr() + i * col_dim + j) = (int)(drand48() * 1000);
        }
    }
    return r;
}

typedef struct __sr_ctx1{
    const double * inarray;
    int32_t dim;
    int32_t maxcall;
    int32_t size;
    int32_t curcall;
} sr_ctx1;

void * row_split::SRF_init(AnyType &args)
{
    ArrayHandle<double> inarray = args[0].getAs<ArrayHandle<double> >();
    int32_t size = args[1].getAs<int32_t>();

    if (size < 1) {
        // throw std::invalid_argument will cause crash
        elog(ERROR, "invalid argument - the spliting size should be positive");
    }

    sr_ctx1 * ctx = new sr_ctx1;
    ctx->inarray = inarray.ptr();
    ctx->dim = inarray.sizeOfDim(0);
    ctx->size = size;
    ctx->maxcall = static_cast<int32_t>(
        ceil(static_cast<double>(ctx->dim) / size));
    ctx->curcall = 0;

    return ctx;
}

/**
 * @brief The function is used to return the next row by the SRF..
 **/
AnyType row_split::SRF_next(void *user_fctx, bool *is_last_call)
{
    sr_ctx1 * ctx = (sr_ctx1 *) user_fctx;
    if (ctx->maxcall == 0) {
        *is_last_call = true;
        return Null();
    }

    int32_t size = ctx->size;
    if (ctx->maxcall == 1 && ctx->dim % ctx->size != 0) {
        size = ctx->dim % ctx->size;
    }

    MutableArrayHandle<double> outarray(
        madlib_construct_array(
            NULL, size, FLOAT8TI.oid, FLOAT8TI.len, FLOAT8TI.byval,
                FLOAT8TI.align));
    memcpy(
        outarray.ptr(), ctx->inarray + ctx->curcall * ctx->size,
            size * sizeof(double));

    ctx->curcall++;
    ctx->maxcall--;
    *is_last_call = false;

    return outarray;
}

AnyType matrix_unblockize_sfunc::run(AnyType & args)
{
    if (args[1].isNull() || args[2].isNull() || args[3].isNull())
        return args[0];

    int32_t total_col_dim = args[1].getAs<int32_t>();
    int32_t col_id = args[2].getAs<int32_t>();
    ArrayHandle<double> row_vec = args[3].getAs<ArrayHandle<double> >();
    int32_t col_dim = row_vec.sizeOfDim(0);

    if(total_col_dim < 1){
        throw std::invalid_argument(
            "invalid argument - total_col_dim should be positive");
    }

    if(col_id < 0){
        throw std::invalid_argument(
            "invalid argument - col_id should be zero or positive");
    }

    if(col_id >= total_col_dim){
        throw std::invalid_argument(
            "invalid argument - col_id should be in the range of [0, total_col_dim)");
    }

    MutableArrayHandle<double> state(NULL);
    if (args[0].isNull()){
        state =  madlib_construct_array(
            NULL, total_col_dim, FLOAT8TI.oid, FLOAT8TI.len, FLOAT8TI.byval,
            FLOAT8TI.align);
    }else{
        state = args[0].getAs<MutableArrayHandle<double> >();
    }

    memcpy(state.ptr() + col_id, row_vec.ptr(), col_dim * sizeof(double));

    return state;
}

typedef struct __sr_ctx2{
    const double * inarray;
    int32_t maxcall;
    int32_t dim;
    int32_t curcall;
} sr_ctx2;

void * unnest_block::SRF_init(AnyType &args)
{
    ArrayHandle<double> inarray = args[0].getAs<ArrayHandle<double> >();
    if(inarray.dims() != 2)
        throw std::invalid_argument("invalid dimension");

    sr_ctx2 * ctx = new sr_ctx2;
    ctx->inarray = inarray.ptr();
    ctx->maxcall = inarray.sizeOfDim(0);
    ctx->dim = inarray.sizeOfDim(1);
    ctx->curcall = 0;

    return ctx;
}

AnyType unnest_block::SRF_next(void *user_fctx, bool *is_last_call)
{
    sr_ctx2 * ctx = (sr_ctx2 *) user_fctx;
    if (ctx->maxcall == 0) {
        *is_last_call = true;
        return Null();
    }

    MutableArrayHandle<double> outarray(
        madlib_construct_array(
            NULL, ctx->dim, FLOAT8TI.oid, FLOAT8TI.len, FLOAT8TI.byval,
            FLOAT8TI.align));
    memcpy(
        outarray.ptr(), ctx->inarray + ctx->curcall * ctx->dim, ctx->dim *
        sizeof(double));

    ctx->curcall++;
    ctx->maxcall--;
    *is_last_call = false;

    return outarray;
}

}
}
}
