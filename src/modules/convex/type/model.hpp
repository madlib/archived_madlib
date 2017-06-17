/* ----------------------------------------------------------------------- *//**
 *
 * @file model.hpp
 *
 * This file contians classes of coefficients (or model), which usually has
 * fields that maps to transition states for user-defined aggregates.
 * The necessity of these wrappers is to allow classes in algo/ and task/ to
 * have a type that they can template over.
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_CONVEX_TYPE_MODEL_HPP_
#define MADLIB_MODULES_CONVEX_TYPE_MODEL_HPP_

#include <modules/shared/HandleTraits.hpp>

namespace madlib {

namespace modules {

namespace convex {

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
    static inline uint32_t arraySize(const int32_t inRowDim,
            const int32_t inColDim, const int32_t inMaxRank) {
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

// Generalized Linear Models (GLMs): Logistic regression, Linear SVM
typedef HandleTraits<MutableArrayHandle<double> >::ColumnVectorTransparentHandleMap
    GLMModel;

// The necessity of this wrapper is to allow classes in algo/ and task/ to
// have a type that they can template over
template <class Handle>
struct MLPModel {
    typename HandleTraits<Handle>::ReferenceToUInt16 is_classification;
    typename HandleTraits<Handle>::ReferenceToUInt16 activation;
    std::vector<Eigen::Map<Matrix > > u;

    /**
     * @brief Space needed.
     *
     * Extra information besides the values in the matrix, like dimension is
     * necessary for a matrix, so that it can perform operations. These are
     * stored in the HandleMap.
     */
    static inline uint32_t arraySize(const uint16_t &inNumberOfStages,
                                     const double *inNumbersOfUnits) {
        // inNumberOfStages == 0 is not an expected value, but
        // it won't cause exception -- returning 0
        uint32_t size = 0;
        size_t N = inNumberOfStages;
        const double *n = inNumbersOfUnits;
        size_t k;
        for (k = 1; k <= N; k ++) {
            size += (n[k-1] + 1) * (n[k] + 1);
        }
        return 1 +       // is_classification
               1 +       // activation
               size;     // weights (u)
    }

    /**
     * @brief Initialize the model randomly
     */
    void initialize(int is_classification_in, int activation_in) {
        is_classification = is_classification_in;
        activation = activation_in;
        // using madlib::dbconnector::$database::NativeRandomNumberGenerator
        NativeRandomNumberGenerator rng;

        // Scaling factor for weight initialization
        double epsilon = 0.0001;


        double base = rng.min();
        double span = rng.max() - base;

        uint16_t N = u.size(); // assuming nu. of layers >= 1
        uint16_t k, s, j;

        std::vector<uint16_t> n; n.clear(); //nu. of units in each layer

        n.push_back(u[0].rows() - 1);
        for (k = 1; k <= N; k ++) {
            n.push_back(u[k-1].cols() - 1);
        }

        for (k=1; k <= N; k++){
            for (s=0; s <= n[k-1]; s++){
                u[k-1](s,0)=1;
                for (j=1; j <= n[k]; j++){
                    // Generate normal(0,epsilon) value using Box-Muller transform
                    double u1 = (rng()-base)/span;
                    double u2 = (rng()-base)/span;
                    double z = std::sqrt(-2*std::log(u1))*std::cos(2*M_PI*u2);
                    u[k-1](s,j) = epsilon*z;
                }
            }
        }
    }

    uint32_t rebind(const double *is_classification_in,
                    const double *activation_in,
                    const double *data,
                    const uint16_t &inNumberOfStages,
                    const double *inNumbersOfUnits) {
        size_t N = inNumberOfStages;
        const double *n = inNumbersOfUnits;
        size_t k;

        is_classification.rebind(is_classification_in);
        activation.rebind(activation_in);

        uint32_t sizeOfU = 0;
        u.clear();
        for (k = 1; k <= N; k ++) {
            u.push_back(Eigen::Map<Matrix >(
                    const_cast<double*>(data + sizeOfU),
                    n[k-1] + 1, n[k] + 1));
            sizeOfU += (n[k-1] + 1) * (n[k] + 1);
        }

        return sizeOfU;
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
        is_classification = inOtherModel.is_classification;
        activation = inOtherModel.activation;

        return *this;
    }
};

} // namespace convex

} // namespace modules

} // namespace madlib

#endif

