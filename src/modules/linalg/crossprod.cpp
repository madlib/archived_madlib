#include <dbconnector/dbconnector.hpp>

#include "crossprod.hpp"

namespace madlib {
namespace modules {
namespace linalg {

using namespace dbal::eigen_integration;

// ----------------------------------------------------------------------

AnyType __pivotalr_crossprod_transition::run (AnyType& args)
{
    ArrayHandle<double> left = args[1].getAs<ArrayHandle<double> >();
    ArrayHandle<double> right = args[2].getAs<ArrayHandle<double> >();
    size_t m = left.size();
    size_t n = right.size();
    MutableArrayHandle<double> state(NULL);

    if (args[0].isNull()) {
        state = this->allocateArray<double, dbal::AggregateContext, dbal::DoZero,
                                    dbal::ThrowBadAlloc>(m * n);
        for (size_t i = 0; i < state.size(); i++) state[i] = 0;
    } else
        state = args[0].getAs<MutableArrayHandle<double> >();

    int count = 0;
    for (size_t i = 0; i < m; i++)
        for (size_t j = 0; j < n; j++)
            state[count++] += left[i] * right[j];

    return state;
}

// ----------------------------------------------------------------------

AnyType __pivotalr_crossprod_merge::run (AnyType& args)
{
    if (args[0].isNull() && args[1].isNull()) return args[0];
    if (args[0].isNull()) return args[1];
    if (args[1].isNull()) return args[0];

    MutableArrayHandle<double> state1 = args[0].getAs<MutableArrayHandle<double> >();
    ArrayHandle<double> state2 = args[1].getAs<ArrayHandle<double> >();
    for (size_t i = 0; i < state1.size(); i++) state1[i] += state2[i];
    return state1;
}

// ----------------------------------------------------------------------

AnyType __pivotalr_crossprod_sym_transition::run (AnyType& args)
{
    ArrayHandle<double> arr = args[1].getAs<ArrayHandle<double> >();
    size_t n = arr.size();
    MutableArrayHandle<double> state(NULL);

    if (args[0].isNull()) {
        state = this->allocateArray<double, dbal::AggregateContext, dbal::DoZero,
                                    dbal::ThrowBadAlloc>(n * (n + 1) / 2);
        for (size_t i = 0; i < state.size(); i++) state[i] = 0;
    } else
        state = args[0].getAs<MutableArrayHandle<double> >();

    int count = 0;
    for (size_t i = 0; i < n; i++)
        for (size_t j = 0; j <= i; j++)
            state[count++] += arr[i] * arr[j];

    return state;
}

}
}
}
