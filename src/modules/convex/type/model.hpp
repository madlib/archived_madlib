/* ----------------------------------------------------------------------- *//**
 *
 * @file model.hpp
 *
 * This file contians classes of coefficients (or model), which usually has
 * fields that maps to transition states for user-defined aggregates.
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_CONVEX_TYPE_MODEL_HPP_
#define MADLIB_MODULES_CONVEX_TYPE_MODEL_HPP_

namespace madlib {

namespace modules {

namespace convex {

// The necessity of this wrapper is to allow classes in algo/ and task/ to
// have a type that they can template over
template <class Handle>
struct LMFModel {
    typename HandleTraits<Handle>::MatrixTransparentHandleMap matrixU;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap matrixV;

    /**
     * @brief Space needed.
     *
     * Extra information besides the values in the matrix, like dimension is
     * necessary for a matrix, so that it can perform operations. These are
     * stored in the HandleMap.
     */
    static inline uint32_t modelSize(const uint16_t inRowDim, 
            const uint16_t inColDim, const uint16_t inMaxRank) {
        return (inRowDim + inColDim) * inMaxRank;
    }

    /**
     * @brief Initialize the model randomly with a user-provided scale factor
     */
    void initialize(const double &inScaleFactor) {
        // using madlib::dbconnector::$database::NativeRandomNumberGenerator
        NativeRandomNumberGenerator rng;
        int i, j, rr;
        double base = rng.min();
        double span = rng.max() - base;
        for (rr = 0; rr < matrixU.cols(); rr ++) {
            for (i = 0; i < matrixU.rows(); i ++) {
                matrixU(i, rr) = inScaleFactor * (rng() - base) / span;
            }
        }
        for (rr = 0; rr < matrixV.cols(); rr ++) {
            for (j = 0; j < matrixV.rows(); j ++) {
                matrixV(j, rr) = inScaleFactor * (rng() - base) / span;
            }
        }
    }

    /*
     *  Some operator wrappers for two matrices.
     */
    LMFModel &operator*=(const double &c) {
        matrixU *= c;
        matrixV *= c;

        return *this;
    }

    template<class OtherHandle>
    LMFModel &operator-=(const LMFModel<OtherHandle> &inOtherModel) {
        matrixU -= inOtherModel.matrixU;
        matrixV -= inOtherModel.matrixV;

        return *this;
    }

    template<class OtherHandle>
    LMFModel &operator+=(const LMFModel<OtherHandle> &inOtherModel) {
        matrixU += inOtherModel.matrixU;
        matrixV += inOtherModel.matrixV;

        return *this;
    }

    template<class OtherHandle>
    LMFModel &operator=(const LMFModel<OtherHandle> &inOtherModel) {
        matrixU = inOtherModel.matrixU;
        matrixV = inOtherModel.matrixV;

        return *this;
    }
};

// The necessity of this wrapper is to allow classes in algo/ and task/ to
// have a type that they can template over
template <class Handle>
struct MLPModel {
    std::vector<Eigen::Map<Matrix > > u;

    /**
     * @brief Space needed.
     *
     * Extra information besides the values in the matrix, like dimension is
     * necessary for a matrix, so that it can perform operations. These are
     * stored in the HandleMap.
     */
    static inline uint32_t modelSize(const uint16_t &inNumberOfStages,
            const uint16_t *inNumbersOfUnits) {
        // inNumberOfStages == 0 is not an expected value, but
        // it won't cause exception -- returning 0
        uint32_t size = 0;
        size_t N = inNumberOfStages;
        const uint16_t *n = inNumbersOfUnits;
        size_t k;
        for (k = 1; k <= N; k ++) {
            size += n[k-1] * n[k];
        }

        return size;
    }

    /*
     *  Some operator wrappers for u.
     */
    MLPModel &operator*=(const double &c) {
        size_t k;
        for (k = 1; k <= u.size(); k ++) {
            u[k-1] *= c;
        }

        return *this;
    }

    template<class OtherHandle>
    MLPModel &operator-=(const MLPModel<OtherHandle> &inOtherModel) {
        size_t k;
        for (k = 1; k <= u.size() && k <= inOtherModel.u.size(); k ++) {
            u[k-1] -= inOtherModel.u[k-1];
        }

        return *this;
    }

    template<class OtherHandle>
    MLPModel &operator+=(const MLPModel<OtherHandle> &inOtherModel) {
        size_t k;
        for (k = 1; k <= u.size() && k <= inOtherModel.u.size(); k ++) {
            u[k-1] += inOtherModel.u[k-1];
        }

        return *this;
    }

    template<class OtherHandle>
    MLPModel &operator=(const MLPModel<OtherHandle> &inOtherModel) {
        size_t k;
        for (k = 1; k <= u.size() && k <= inOtherModel.u.size(); k ++) {
            u[k-1] = inOtherModel.u[k-1];
        }

        return *this;
    }
};

} // namespace convex

} // namespace modules

} // namespace madlib

#endif

