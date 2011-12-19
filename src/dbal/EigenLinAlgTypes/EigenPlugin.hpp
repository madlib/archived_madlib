/* ----------------------------------------------------------------------- *//**
 *
 * @file EigenPlugin.hpp
 *
 *//* ----------------------------------------------------------------------- */

// Workaround for Doxygen: Ignore if not included by EigenLinAlgTypes.hpp
#ifdef MADLIB_EIGEN_LINALGTYPES_HPP

inline bool is_finite() const {
    for (Index j = 0; j < size(); ++j)
        if (!boost::math::isfinite(this->coeff(j)))
            return false;
    return true;
}

#endif // MADLIB_EIGEN_LINALGTYPES_HPP (workaround for Doxygen)
