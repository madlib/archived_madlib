/* ----------------------------------------------------------------------- *//**
 *
 * @file logistic.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_REGRESS_LOGISTIC_H
#define MADLIB_REGRESS_LOGISTIC_H

#include <modules/common.hpp>

namespace madlib {

namespace modules {

namespace regress {

/**
 * @brief Functions for logistic regression, using the conjugate-gradient method
 */
struct LogisticRegressionCG {
    class State;
    
    static AnyType transition(AbstractDBInterface &db, AnyType args);
    static AnyType mergeStates(AbstractDBInterface &db, AnyType args);
    static AnyType final(AbstractDBInterface &db, AnyType args);
    
    static AnyType distance(AbstractDBInterface &db, AnyType args);
    static AnyType result(AbstractDBInterface &db, AnyType args);
};

/**
 * @brief Functions for logistic regression, using the
 *        iteratively-reweighted-least-squares method
 */
struct LogisticRegressionIRLS {
    class State;
    
    static AnyType transition(AbstractDBInterface &db, AnyType args);
    static AnyType mergeStates(AbstractDBInterface &db, AnyType args);
    static AnyType final(AbstractDBInterface &db, AnyType args);
    
    static AnyType distance(AbstractDBInterface &db, AnyType args);
    static AnyType result(AbstractDBInterface &db, AnyType args);
};

} // namespace regress

} // namespace modules

} // namespace regress

#endif
