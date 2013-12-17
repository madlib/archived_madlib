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
#include <algorithm>
#include <functional>
#include <numeric>
#include "utilities.hpp"

namespace madlib {
namespace modules {
namespace utilities{

AnyType array_to_1d::run(AnyType & args)
{
    if (args[0].isNull()) return args[0];
    ArrayHandle<double> in_array = args[0].getAs<ArrayHandle<double> >();
    if (in_array.size() == 0) return args[0];
    if(in_array.dims() != 2){
        throw std::invalid_argument("dimension mismatch - 2 expected");
    }
    MutableArrayHandle<double> out_array = 
        allocateArray<double, dbal::FunctionContext,
            dbal::DoZero, dbal::ThrowBadAlloc>(in_array.size() + 2);
    // The fist two elements encode the dimension info
    out_array[0] = in_array.sizeOfDim(0);
    out_array[1] = in_array.sizeOfDim(1);
    memcpy(out_array.ptr() + 2, in_array.ptr(), sizeof(double) * in_array.size());

    return out_array; 
}

AnyType array_to_2d::run(AnyType & args)
{
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

}
}
}
