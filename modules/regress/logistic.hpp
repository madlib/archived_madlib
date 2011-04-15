/* ----------------------------------------------------------------------- *//**
 *
 * @file logistic.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_REGRESS_LOGISTIC_H
#define MADLIB_REGRESS_LOGISTIC_H

#include <madlib/modules/modules.hpp>

namespace madlib {

namespace modules {

namespace regress {

/**
 * @brief Functions for logistic regression, using the conjugate-gradient method
 */
struct LogisticRegressionCG {
    class State;
    
    static AnyValue transition(AbstractDBInterface &db, AnyValue args);
    static AnyValue preliminary(AbstractDBInterface &db, AnyValue args);
    static AnyValue final(AbstractDBInterface &db, AnyValue args);
    
    static AnyValue distance(AbstractDBInterface &db, AnyValue args);
    static AnyValue coef(AbstractDBInterface &db, AnyValue args);
};

/**
 * @brief Functions for logistic regression, using the
 *        iteratively-reweighted-least-squares method
 */
struct LogisticRegressionIRLS {
    class State;
    
    static AnyValue transition(AbstractDBInterface &db, AnyValue args);
    static AnyValue preliminary(AbstractDBInterface &db, AnyValue args);
    static AnyValue final(AbstractDBInterface &db, AnyValue args);
    
    static AnyValue distance(AbstractDBInterface &db, AnyValue args);
    static AnyValue coef(AbstractDBInterface &db, AnyValue args);
};

} // namespace regress

} // namespace modules

} // namespace regress

#endif
