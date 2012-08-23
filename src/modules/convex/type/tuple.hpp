/* ----------------------------------------------------------------------- *//**
 *
 * @file tuple.hpp
 *
 * Tuple classes are defined so that algorithm code can be shared
 * for different kinds of tuples.
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_CONVEX_TYPE_TUPLE_HPP_
#define MADLIB_MODULES_CONVEX_TYPE_TUPLE_HPP_

#include "independent_variables.hpp"
#include "dependent_variable.hpp"

namespace madlib {

namespace modules {

namespace convex {

template <class IndependentVariables, class DependentVariable>
struct ExampleTuple {
    typedef IndependentVariables independent_variables_type;
    typedef DependentVariable dependent_variable_type;

    // id may not be necessary all the time, keep it for now
    // since harm is not observed
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
    ~ExampleTuple() { }
};

typedef ExampleTuple<MatrixIndex, double> LMFTuple;

} // namespace convex

} // namespace modules

} // namespace madlib

#endif

