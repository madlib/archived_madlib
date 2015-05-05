/* ----------------------------------------------------------------------- *//**
 *
 * @file dim_conversion.cpp
 *
 * @brief Functions for converting b/w 1-D and 2-D arrays
 *
 * @date Dec 17, 2013
 *//* ----------------------------------------------------------------------- */


#include <dbconnector/dbconnector.hpp>
#include <math.h>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <functional>
#include <numeric>
#include "dim_conversion.hpp"

namespace madlib {
namespace modules {
namespace linalg {

using namespace dbal;
using namespace dbal::eigen_integration;

AnyType
array_to_1d::run(AnyType & args) {
    if (args[0].isNull()) { return args[0]; }
    ArrayHandle<double> in_array = args[0].getAs<ArrayHandle<double> >();
    if (in_array.size() == 0) { return args[0]; }
    size_t dims = in_array.dims();
    if(dims == 1) { return args[0]; }
    if(dims != 2){
        std::stringstream err_msg;
        err_msg << "Can only handle 1-D or 2-D, given " << dims;
        throw std::invalid_argument(err_msg.str());
    }
    MutableArrayHandle<double> out_array =
        allocateArray<double, dbal::FunctionContext,
            dbal::DoZero, dbal::ThrowBadAlloc>(in_array.size() + 2);
    // The fist two elements encode the dimension info
    out_array[0] = static_cast<double>(in_array.sizeOfDim(0));
    out_array[1] = static_cast<double>(in_array.sizeOfDim(1));
    memcpy(out_array.ptr() + 2, in_array.ptr(), sizeof(double) * in_array.size());

    return out_array;
}

AnyType
array_to_2d::run(AnyType & args) {
    if (args[0].isNull()) return args[0];
    ArrayHandle<double> in_array = args[0].getAs<ArrayHandle<double> >();
    if (in_array.size() == 0) return args[0];

    size_t dim1 = static_cast<size_t>(in_array[0]);
    size_t dim2 = static_cast<size_t>(in_array[1]);
    if (dim1 * dim2 + 2 != in_array.size()) {
        throw std::runtime_error("dimension mismatch in the encoded input array");
    }

    MutableArrayHandle<double> out_array =
        allocateArray<double, dbal::FunctionContext,
            dbal::DoZero, dbal::ThrowBadAlloc>(
                static_cast<size_t>(in_array[0]), static_cast<size_t>(in_array[1]));
    memcpy(out_array.ptr(), in_array.ptr() + 2, sizeof(double) * (in_array.size() - 2));

    return out_array;
}

AnyType
get_row_from_2d_array::run(AnyType & args) {
    MappedMatrix input = args[0].getAs<MappedMatrix>();
    int index = args[1].getAs<int>() - 1; // database index starts from 1
    if (index < 0 or index >= input.cols()) {
        std::stringstream err_msg;
        err_msg << "Out-of-bound index: " << index + 1 << " not in [1, "
            << input.cols() + 1 << "]";
        throw std::runtime_error(err_msg.str());
    }
    MutableNativeColumnVector ret(this->allocateArray<double>(input.rows()));
    ret = input.col(static_cast<Index>(index));

    return ret;
}

AnyType
get_col_from_2d_array::run(AnyType & args) {
    MappedMatrix input = args[0].getAs<MappedMatrix>();
    int index = args[1].getAs<int>() - 1; // database index starts from 1
    if (index < 0 or index >= input.rows()) {
        std::stringstream err_msg;
        err_msg << "Out-of-bound index: " << index + 1 << " not in [1, "
            << input.rows() + 1 << "]";
        throw std::runtime_error(err_msg.str());
    }
    MutableNativeColumnVector ret(this->allocateArray<double>(input.cols()));
    ret = input.row(static_cast<Index>(index));

    return ret;
}

// ----------------------------------------------------------------------

namespace {

struct Deconstruct2DArrayContext {
    // Assumption: mat is the transpose of the 2-D array in database
    NativeMatrix mat;
    Index curr_col;
};

} // namespace

void*
deconstruct_2d_array::SRF_init(AnyType &args) {
    Deconstruct2DArrayContext *uctx = new Deconstruct2DArrayContext;
    ArrayHandle<double> in_array = args[0].getAs<ArrayHandle<double> >();
    if (in_array.dims() == 2) {
        uctx->mat.rebind(in_array);
    } else if (in_array.dims() < 2) {
        uctx->mat.rebind(in_array, in_array.size(), 1);
    } else {
        throw std::runtime_error("2-D array expected");
    }
    uctx->curr_col = 0;

    return uctx;
}

AnyType
deconstruct_2d_array::SRF_next(void *user_fctx, bool *is_last_call) {
    Deconstruct2DArrayContext *uctx =
            reinterpret_cast<Deconstruct2DArrayContext*>(user_fctx);
    if (uctx->mat.rows() == 0 ||
            uctx->curr_col >= uctx->mat.cols()) {
        *is_last_call = true;
        return Null();
    }

    AnyType tuple;
    tuple << static_cast<int>(uctx->curr_col + 1);
    for (Index i = 0; i < uctx->mat.rows(); i ++) {
        tuple << uctx->mat(i, uctx->curr_col);
    }
    uctx->curr_col ++;

    return tuple;
}

void*
deconstruct_lower_triangle::SRF_init(AnyType &args) {
    Deconstruct2DArrayContext *uctx = new Deconstruct2DArrayContext;
    ArrayHandle<double> in_array = args[0].getAs<ArrayHandle<double> >();
    if (in_array.dims() != 2) {
        throw std::runtime_error("symmetric 2-D array expected");
    } else {
        uctx->mat.rebind(in_array);
    }
    if (uctx->mat.rows() != uctx->mat.cols()) {
        throw std::runtime_error("symmetric 2-D array expected");
    }
    uctx->curr_col = 0;

    return uctx;
}

AnyType
deconstruct_lower_triangle::SRF_next(void *user_fctx, bool *is_last_call) {
    Deconstruct2DArrayContext *uctx =
            reinterpret_cast<Deconstruct2DArrayContext*>(user_fctx);
    if (uctx->mat.rows() == 0 ||
            uctx->curr_col >= uctx->mat.cols()) {
        *is_last_call = true;
        return Null();
    }

    AnyType tuple;
    tuple << static_cast<int>(uctx->curr_col + 1);
    for (Index i = 0; i <= uctx->curr_col; i ++) {
        tuple << uctx->mat(i, uctx->curr_col);
    }
    uctx->curr_col ++;

    return tuple;
}

} // linalg

} // modules

} // madlib
