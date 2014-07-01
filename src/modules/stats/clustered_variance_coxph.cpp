
/* ----------------------------------------------------------------------
   Clustered Variance estimator for CoxPH model
   ---------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>
#include <modules/shared/HandleTraits.hpp>
#include <modules/prob/boost.hpp>

#include "clustered_variance_coxph.hpp"

namespace madlib {
namespace modules {
namespace stats {

// Use Eigen
using namespace dbal::eigen_integration;
// Import names from other MADlib modules
using dbal::NoSolutionFoundException;

using namespace std;

// ----------------------------------------------------------------------

template <class Handle>
class CLABTransitionState {

    template <class OtherHandle>
    friend class CLABTransitionState;

  public:
    CLABTransitionState(const AnyType &inArray)
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
    inline void initialize(const Allocator &inAllocator, uint16_t inWidthOfX) {
        mStorage = inAllocator.allocateArray<
            double, dbal::AggregateContext, dbal::DoZero, dbal::ThrowBadAlloc>(
                arraySize(inWidthOfX));
        rebind(inWidthOfX);
        widthOfX = inWidthOfX;
        this->reset();
    }

    /**
     * @brief Reset the inter-iteration fields.
     */
    inline void reset() {
        numRows = 0;
        A = 0;
        B.fill(0);
    }

  private:
    static inline size_t arraySize(const uint16_t inWidthOfX) {
        return 3 + inWidthOfX;
    }

    void rebind(uint16_t inWidthOfX) {
        numRows.rebind(&mStorage[0]);
        widthOfX.rebind(&mStorage[1]);
        A.rebind(&mStorage[2]);
        B.rebind(&mStorage[3], inWidthOfX);
    }

    Handle mStorage;

  public:
    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
    typename HandleTraits<Handle>::ReferenceToUInt16 widthOfX;
    typename HandleTraits<Handle>::ReferenceToDouble A;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap B;
};

// ----------------------------------------------------------------------

AnyType coxph_a_b_transition::run(AnyType& args)
{
    CLABTransitionState<MutableArrayHandle<double> > state = args[0];
    
    int widthOfX = args[1].getAs<int>();
    bool status;
    if (args[2].isNull()) {
        // by default we assume that the data is uncensored => status = TRUE
        status = true;
    } else {
        status = args[2].getAs<bool>();
    }
    
    MappedColumnVector H = args[3].getAs<MappedColumnVector>();
    double S = args[4].getAs<double>();

    if (widthOfX > std::numeric_limits<uint16_t>::max())
        throw std::domain_error(
            "Number of independent variables cannot be larger than 65535.");

    if (state.numRows == 0) {
        state.initialize(*this, static_cast<uint16_t>(widthOfX));        
    }

    state.numRows++;

    if (status == 1) {
        state.A += 1. / S;
        state.B += H / (S*S);
    }

    return state;
}

// ----------------------------------------------------------------------
AnyType coxph_a_b_merge::run(AnyType& args)
{
    (void)args;
    throw std::logic_error("The aggregate is used as an aggregate over window. "
                           "The merge function should not be used in this scenario.");
}

// ----------------------------------------------------------------------
AnyType coxph_a_b_final::run(AnyType& args)
{
    CLABTransitionState<MutableArrayHandle<double> > state = args[0];
    if (state.numRows == 0) return Null();

    MutableNativeColumnVector B(
        this->allocateArray<double>(state.widthOfX));
    for (int i = 0; i < state.widthOfX; i++) B(i) = state.B(i);

    AnyType tuple;
    tuple << static_cast<double>(state.A) << B;
    return tuple;
}

// ----------------------------------------------------------------------

AnyType coxph_compute_w::run(AnyType& args)
{
    MappedColumnVector x = args[0].getAs<MappedColumnVector>();

    bool status;
    if (args[1].isNull()) {
        // by default we assume that the data is uncensored => status = TRUE
        status = true;
    } else {
        status = args[1].getAs<bool>();
    }

    MappedColumnVector coef = args[2].getAs<MappedColumnVector>();
    MappedColumnVector H = args[3].getAs<MappedColumnVector>();
    double S = args[4].getAs<double>();
    double A = args[5].getAs<double>();
    MappedColumnVector B = args[6].getAs<MappedColumnVector>();

    if (x.size() > std::numeric_limits<uint16_t>::max())
        throw std::domain_error(
            "Number of independent variables cannot be larger than 65535.");

    MutableNativeColumnVector w(allocateArray<double>(x.size()));
    if (status == 1)
        w += x - H/S;

    double exp_coef_x = std::exp(trans(coef) * x);
    w += exp_coef_x * B - exp_coef_x * x * A;

    return w;
}

// ----------------------------------------------------------------------

AnyType coxph_compute_clustered_stats::run(AnyType& args)
{
    MappedColumnVector coef = args[0].getAs<MappedColumnVector>();
    MappedMatrix hessian = args[1].getAs<MappedMatrix>();
    MappedMatrix matA = args[2].getAs<MappedMatrix>();
    
    // Computing pseudo inverse of a PSD matrix
    SymmetricPositiveDefiniteEigenDecomposition<Matrix> decomposition(
        hessian.transpose(), EigenvaluesOnly, ComputePseudoInverse);
    Matrix inverse_of_hessian = decomposition.pseudoInverse();

    Matrix sandwich(inverse_of_hessian * (matA * matA.transpose()) * inverse_of_hessian);
    ColumnVector sig = sandwich.diagonal();

    MutableNativeColumnVector std_err(
        this->allocateArray<double>(coef.size()));
    MutableNativeColumnVector waldZStats(
        this->allocateArray<double>(coef.size()));
    MutableNativeColumnVector waldPValues(
        this->allocateArray<double>(coef.size()));
    for (int i = 0; i < sig.size(); i++) {
        std_err(i) = sqrt(sig(i));
        waldZStats(i) = coef(i) / std_err(i);
        waldPValues(i) = 2. * prob::cdf(prob::normal(), -std::abs(waldZStats(i)));
    }

    AnyType tuple;
    tuple << std_err << waldZStats << waldPValues;
    return tuple;
}

}
}
}
