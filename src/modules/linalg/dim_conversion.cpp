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

using namespace madlib::dbal::eigen_integration;

AnyType
get_row_from_2d_array::run(AnyType & args) {
    MappedMatrix input = args[0].getAs<MappedMatrix>();
    int index = args[1].getAs<int>() - 1; // database index starts from 1
    if (index < 0 or index >= input.cols()) {
        std::stringstream err_msg;
        err_msg << "Out-of-bound index: " << index << " >= " << input.cols();
        throw std::runtime_error(err_msg.str());
    }
    MutableNativeColumnVector ret(this->allocateArray<double>(input.rows()));
    ret = input.col(static_cast<Index>(index));

    return ret;
}

}
}
}
