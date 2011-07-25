/* ----------------------------------------------------------------------- *//**
 *
 * @file linear.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_REGRESS_LINEAR_H
#define MADLIB_REGRESS_LINEAR_H

#include <modules/common.hpp>

namespace madlib {

namespace modules {

namespace regress {

/**
 * @brief Linear-regression functions
 */
struct LinearRegression {
    enum What { kCoef, kRSquare, kTStats, kPValues };
    
    class TransitionState;
    
    static AnyType transition(AbstractDBInterface &db, AnyType args);
    static AnyType mergeStates(AbstractDBInterface &db, AnyType args);
    static AnyType final(AbstractDBInterface &db, AnyType args);
};

} // namespace regress

} // namespace modules

} // namespace regress

#endif
