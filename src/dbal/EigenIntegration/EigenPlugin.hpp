/* ----------------------------------------------------------------------- *//**
 *
 * @file EigenPlugin.hpp
 *
 *//* ----------------------------------------------------------------------- */

// Workaround for Doxygen: Ignore if not included by EigenIntegration.hpp
#ifdef MADLIB_DBAL_EIGEN_INTEGRATION_HPP

inline bool is_finite() const {
    for (Index j = 0; j < size(); ++j)
        if (!boost::math::isfinite(this->coeff(j)))
            return false;
    return true;
}

#endif // MADLIB_DBAL_EIGEN_INTEGRATION_HPP (workaround for Doxygen)
