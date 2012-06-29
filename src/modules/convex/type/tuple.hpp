/* ----------------------------------------------------------------------- *//**
 *
 * @file tuple.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_CONVEX_TYPE_TUPLE_HPP_
#define MADLIB_CONVEX_TYPE_TUPLE_HPP_

namespace madlib {

namespace modules {

namespace convex {

template <class IndependentVariables, class DependentVariable>
struct ExampleTuple {
    typedef IndependentVariables independent_variables_type;
    typedef DependentVariable dependent_variable_type;

    int id;
    independent_variables_type indVar;
    dependent_variable_type depVar;

    ExampleTuple() { id = 0; }
    ExampleTuple(const ExampleTuple &rhs) {
        id = rhs.id;
        indVar = rhs.indVar;
        depVar = rhs.depVar;
    }
    ExampleTuple& operator=(const ExampleTuple &rhs) {
        if (this != &rhs) {
            id = rhs.id;
            indVar = rhs.indVar;
            depVar = rhs.depVar;
        }
        return *this;
    }
    ~ExampleTuple() {}
};

typedef ExampleTuple<MatrixIndex, double> LMFTuple;

} // namespace convex

} // namespace modules

} // namespace madlib

#endif

