namespace madlib {
namespace modules {
namespace stats {

// Use Eigen
using namespace dbal::eigen_integration;
// Import names from other MADlib modules
using dbal::NoSolutionFoundException;

using namespace std;
// -------------------------------------------------------------------------

template <class Handle>
class CoxPHState {

    template <class OtherHandle>
    friend class CoxPHState;

  public:
    CoxPHState(const AnyType &inArray)
        : mStorage(inArray.getAs<Handle>()) {

        rebind(static_cast<uint16_t>(mStorage[1]));
    }

    /**
     * @brief Convert to backend representation
     *
     * We define this function so that we can use TransitionState in the argument
     * list and as a return type.   */
    inline operator AnyType() const {
        return mStorage;
    }

    /**
     * @brief Initialize the transition state. Only called for first row.
     *
     * @param inAllocator Allocator for the memory transition state. Must fill
     *     the memory block with zeros.
     * @param inWidthOfX Number of independent variables. The first row of data
     *     determines the size of the transition state. This size is a quadratic
     *     function of inWidthOfX.
     */
    inline void initialize(const Allocator &inAllocator, uint16_t inWidthOfX, const double * inCoef = 0) {
        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
                                             dbal::DoZero, dbal::ThrowBadAlloc>(arraySize(inWidthOfX));
        rebind(inWidthOfX);
        widthOfX = inWidthOfX;
        if(inCoef){
            for(uint16_t i = 0; i < widthOfX; i++)
                coef[i] = inCoef[i];
        }
        this->reset();
    }

    /**
     * @brief We need to support assigning the previous state
     */
    template <class OtherHandle>
    CoxPHState &operator=(
        const CoxPHState<OtherHandle> &inOtherState) {
        for (size_t i = 0; i < mStorage.size(); i++)
            mStorage[i] = inOtherState.mStorage[i];
        return *this;
    }

    /**
     * @brief Merge with another State object by copying the intra-iteration
     *     fields
     */
    template <class OtherHandle>
    CoxPHState &operator+=(
        const CoxPHState<OtherHandle> &inOtherState) {
        if (mStorage.size() != inOtherState.mStorage.size() ||
            widthOfX != inOtherState.widthOfX)
            throw std::logic_error(
                "Internal error: Incompatible transition states");

        numRows += inOtherState.numRows;
        grad += inOtherState.grad;
        S += inOtherState.S;
        H += inOtherState.H;
        logLikelihood += inOtherState.logLikelihood;
        V += inOtherState.V;
        hessian += inOtherState.hessian;

        return *this;
    }

    /**
     * @brief Reset the inter-iteration fields.
     */
    inline void reset() {
        numRows = 0;
        S = 0;
        tdeath = 0;
        y_previous = 0;
        multiplier = 0;
        H.fill(0);
        V.fill(0);
        grad.fill(0);
        hessian.fill(0);
        logLikelihood = 0;

    }

  private:
    static inline size_t arraySize(const uint16_t inWidthOfX) {
        return 7 + 4 * inWidthOfX + 2 * inWidthOfX * inWidthOfX;
    }

    void rebind(uint16_t inWidthOfX) {
        // Inter iteration components
        numRows.rebind(&mStorage[0]);
        widthOfX.rebind(&mStorage[1]);
        multiplier.rebind(&mStorage[2]);
        y_previous.rebind(&mStorage[3]);
        coef.rebind(&mStorage[4], inWidthOfX);

        // Intra iteration components
        S.rebind(&mStorage[4+inWidthOfX]);
        H.rebind(&mStorage[5+inWidthOfX], inWidthOfX);
        grad.rebind(&mStorage[5+2*inWidthOfX],inWidthOfX);
        logLikelihood.rebind(&mStorage[5+3*inWidthOfX]);
        V.rebind(&mStorage[6+3*inWidthOfX],
                 inWidthOfX, inWidthOfX);
        hessian.rebind(&mStorage[6+3*inWidthOfX+inWidthOfX*inWidthOfX],
                       inWidthOfX, inWidthOfX);
        max_coef.rebind(&mStorage[6 + 3 * inWidthOfX + 2 * inWidthOfX * inWidthOfX], inWidthOfX);
        tdeath.rebind(&mStorage[6 + 4 * inWidthOfX + 2 * inWidthOfX * inWidthOfX]);
    }

    Handle mStorage;

  public:
    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
    typename HandleTraits<Handle>::ReferenceToUInt16 widthOfX;
    typename HandleTraits<Handle>::ReferenceToDouble multiplier;
    typename HandleTraits<Handle>::ReferenceToDouble y_previous;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap coef;

    typename HandleTraits<Handle>::ReferenceToDouble S;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap H;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap grad;
    typename HandleTraits<Handle>::ReferenceToDouble logLikelihood;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap V;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap hessian;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap max_coef;
    typename HandleTraits<Handle>::ReferenceToDouble tdeath;
};


} // namespace stats
} // namespace modules
} // namespace madlib
