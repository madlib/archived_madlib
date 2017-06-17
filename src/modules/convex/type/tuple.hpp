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

#include <dbconnector/dbconnector.hpp>
#include "independent_variables.hpp"

#include "dependent_variable.hpp"

#include <dbconnector/dbconnector.hpp>

namespace madlib {

namespace modules {

namespace convex {

// Use Eigen
using namespace madlib::dbal::eigen_integration;

template <class IndependentVariables, class DependentVariable>
struct ExampleTuple {
    typedef IndependentVariables independent_variables_type;
    typedef DependentVariable dependent_variable_type;

    // id may not be necessary all the time, keep it for now
    // since harm is not observed
    int id;
    independent_variables_type indVar;
    dependent_variable_type depVar;
    double weight;

    ExampleTuple() { id = 0; weight = 1;}
    ExampleTuple(const ExampleTuple &rhs) {
        id = rhs.id;
        indVar = rhs.indVar;
        depVar = rhs.depVar;
        weight = rhs.weight;
    }
    ExampleTuple& operator=(const ExampleTuple &rhs) {
        if (this != &rhs) {
            id = rhs.id;
            indVar = rhs.indVar;
            depVar = rhs.depVar;
            weight = rhs.weight;
        }
        return *this;
    }
    ~ExampleTuple() { }
};

using madlib::dbal::eigen_integration::MappedColumnVector;
// Generalized Linear Models (GLMs): Logistic regression, Linear SVM
typedef ExampleTuple<MappedColumnVector, double> GLMTuple;

// madlib::modules::convex::MatrixIndex
typedef ExampleTuple<MatrixIndex, double> LMFTuple;

typedef ExampleTuple<MappedColumnVector, MappedColumnVector> MLPTuple;

} // namespace convex

} // namespace modules

} // namespace madlib

#endif

