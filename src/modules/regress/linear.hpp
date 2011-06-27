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
    
    static AnyValue transition(AbstractDBInterface &db, AnyValue args);
    static AnyValue mergeStates(AbstractDBInterface &db, AnyValue args);
    static AnyValue final(AbstractDBInterface &db, AnyValue args);
};

} // namespace regress

} // namespace modules

} // namespace regress

#endif
