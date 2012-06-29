/* ----------------------------------------------------------------------- *//**
 *
 * @file model.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_CONVEX_TYPE_MODEL_HPP_
#define MADLIB_CONVEX_TYPE_MODEL_HPP_

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
    static inline uint32_t arraySize(const uint16_t inRowDim, 
            const uint16_t inColDim, const uint16_t inMaxRank) {
        return (inRowDim + inColDim) * inMaxRank;
    }

    /**
     * HAYING: broken now thanks to the HandleTraits...
     * Before modifying HandleTraits.hpp, we have to rebind the matrices
     * in a higher level.
     *
     * @brief Rebind to a provided double array.
     */
    //void rebind(const Handle &inHandle, uint16_t inRowDim, uint16_t inColDim,
    //        uint16_t inMaxRank) {
    //    matrixU.rebind(&inHandle[0], inRowDim, inMaxRank);
    //    matrixV.rebind(&inHandle[inRowDim * inMaxRank], inColDim, inMaxRank);
    //}

    /**
     * @brief Initialize the model randomly with a user-provided scale factor
     */
    void initialize(const double &inInitValue) {
        srand(static_cast<unsigned>(time(NULL)));
        int i, j, r;
        for (i = 0; i < matrixU.rows(); i ++) {
            for (r = 0; r < matrixU.cols(); r ++) {
                matrixU(i, r) = (inInitValue * rand()) / RAND_MAX;
            }
        }
        for (j = 0; j < matrixV.rows(); j ++) {
            for (r = 0; r < matrixV.cols(); r ++) {
                matrixV(j, r) = (inInitValue * rand()) / RAND_MAX;
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
        // The semantic of operator= for HandleMap is a bit confusing:
        // If matrixU acts like a "reference type" in C++, it should be 
        // value-copying; if matrixU acts like a "pointer type" in C++,
        // or reference in Java, it is a rebinding
        // HAYING: I can rule out the possibility of the latter after I use
        // this function in higher levels, but I am still wondering whether
        // we can make it more explicit...

        matrixU = inOtherModel.matrixU;
        matrixV = inOtherModel.matrixV;

        return *this;
    }
};

} // namespace convex

} // namespace modules

} // namespace madlib

#endif

