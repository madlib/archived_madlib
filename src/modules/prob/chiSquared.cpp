/* ----------------------------------------------------------------------- *//** 
 *
 * @file chiSquared.cpp
 *
 * @brief Evaluate the chi-squared distribution function using the boost library.
 *
 *//* ----------------------------------------------------------------------- */

#include <modules/prob/chiSquared.hpp>

#include <boost/math/distributions/chi_squared.hpp>


namespace madlib {

  namespace modules {

    namespace prob {

      /**
       * @brief Chi-squared cumulative distribution function: In-database interface
       */
      AnyType chi_squared_cdf(AbstractDBInterface & /* db */, AnyType args) {
        AnyType::iterator   arg(args);
        const double        x  = *arg++;
        const double        df = *arg;
        
        /* We want to ensure df > 0 */
        if (df <= 0)
          throw std::domain_error("Chi Squared distribution undefined for "
                                  "degree of freedom <= 0");

        return boost::math::cdf( boost::math::chi_squared(df), x );
      }


      /**
       * @brief Chi-squared probability density function: In-database interface
       */
      AnyType chi_squared_pdf(AbstractDBInterface & /* db */, AnyType args) {
        AnyType::iterator   arg(args);
        const double        x  = *arg++;
        const double        df = *arg;
  
        /* We want to ensure df > 0 */
        if (df <= 0)
          throw std::domain_error("Chi Squared distribution undefined for "
                                  "degree of freedom <= 0");

        return boost::math::pdf( boost::math::chi_squared(df), x );
      }


      /**
       * @brief Chi-squared quantile function: In-database interface
       */
      AnyType chi_squared_quantile(AbstractDBInterface & /* db */, AnyType args) 
      {
        AnyType::iterator   arg(args);
        const double        p  = *arg++;
        const double        df = *arg;
        
        /* We want to ensure df > 0 */
        if (df <= 0)
          throw std::domain_error("Chi Squared distribution undefined for "
                                  "degree of freedom <= 0");
        if (p < 0 || p > 1)
          throw std::domain_error("Chi Squared probability must be in the "
                                  "range [0,1]");

        return boost::math::quantile( boost::math::chi_squared(df), p );
      }


    } // namespace prob

  } // namespace modules

} // namespace regress
