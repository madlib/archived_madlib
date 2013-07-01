
#include <dbconnector/dbconnector.hpp>
#include <boost/math/distributions.hpp>
#include <modules/prob/student.hpp>
#include <modules/prob/boost.hpp>
#include <limits>

#include "clustered_errors.hpp"
#include "clustered_errors_state.hpp"

using namespace madlib::dbal::eigen_integration;

namespace madlib {
namespace modules {
namespace regress {

// ------------------------------------------------------------------------

typedef ClusteredState<RootContainer> IClusteredState;
typedef ClusteredState<MutableRootContainer> MutableClusteredState;

// ------------------------------------------------------------------------

// function used by linear and logistic transitions
AnyType __clustered_common_transition (AnyType& args, bool bool_y,
                                       void (*func)(
                                           MutableClusteredState&,
                                           const MappedColumnVector&,
                                           const double& y))
{
    MutableClusteredState state = args[0].getAs<MutableByteString>();
    double y;
    if (bool_y)
        y = args[1].getAs<bool>() ? 1. : -1;
    else
        y = args[1].getAs<double>();
    const MappedColumnVector& x = args[2].getAs<MappedColumnVector>();

    if (!std::isfinite(y))
        throw std::domain_error("Dependent variables are not finite.");
    else if (x.size() > std::numeric_limits<uint16_t>::max())
        throw std::domain_error("Number of independent variables cannot be "
                                "larger than 65535.");
    
    if (state.numRows == 0) {
        state.widthOfX = static_cast<uint16_t>(x.size());
        state.resize();
        const MappedColumnVector& coef = args[3].getAs<MappedColumnVector>();
        state.coef = coef;
        state.meat_half.setZero();
    }

    // dimension check
    if (state.widthOfX != static_cast<uint16_t>(x.size()))
        throw std::runtime_error("Inconsistent numbers of independent "
                                 "variables.");

    state.numRows++;

    (*func)(state, x, y);
    return state.storage();
}

// ------------------------------------------------------------------------

// function used by linear and logistic merges
AnyType __clustered_common_merge (AnyType& args)
{
    MutableClusteredState state1 = args[0].getAs<MutableByteString>();
    IClusteredState state2 = args[1].getAs<ByteString>();
    
    if (state1.numRows == 0)
        return state2.storage();
    else if (state2.numRows == 0)
        return state1.storage();
  
    state1.numRows += state2.numRows;
    state1.bread += state2.bread;
    state1.meat_half += state2.meat_half;

    return state1.storage();
}

// ------------------------------------------------------------------------

// function used by linear and logistic finals
AnyType __clustered_common_final (AnyType& args)
{
    IClusteredState state = args[0].getAs<ByteString>();

    if (state.numRows == 0) return Null();

    Allocator& allocator = defaultAllocator();
    
    SymmetricPositiveDefiniteEigenDecomposition<Matrix> decomposition(
        state.bread, EigenvaluesOnly, ComputePseudoInverse);

    int k = static_cast<int>(state.widthOfX);
    
    Matrix meat(k, k);
    meat = trans(state.meat_half) * state.meat_half;

    MutableNativeColumnVector meatvec;
    MutableNativeColumnVector breadvec;
    meatvec.rebind(allocator.allocateArray<double>(k*k));
    breadvec.rebind(allocator.allocateArray<double>(k*k));
    int count = 0;
    for (int i = 0; i < k; i++)
        for (int j = 0; j < k; j++) {
            meatvec(count) = meat(i,j);
            breadvec(count) = state.bread(i,j);
            count++;
        }

    AnyType tuple;

    tuple << meatvec << breadvec;

    return tuple;
}

// ------------------------------------------------------------------------

// Compute the stats from coef and errs
AnyType clustered_compute_stats (AnyType& args,
                                 void (*func)(
                                     MutableNativeColumnVector&,
                                     MutableNativeColumnVector&,
                                     int, int))
{
    const MappedColumnVector& coef = args[0].getAs<MappedColumnVector>();
    const MappedColumnVector& meatvec = args[1].getAs<MappedColumnVector>();
    const MappedColumnVector& breadvec = args[2].getAs<MappedColumnVector>();
    int mcluster = args[3].getAs<int>();
    int numRows = args[4].getAs<int>();
    int k = coef.size();
    Matrix bread(k,k);
    Matrix meat(k,k);
    int count = 0;

    for (int i = 0; i < k; i++)
        for (int j = 0; j < k; j++) {
            meat(i,j) = meatvec(count);
            bread(i,j) = breadvec(count);
            count++;
        }

    if (mcluster == 1)
        throw std::domain_error ("Clustered variance error: Number of cluster cannot be smaller than 2!");
    double dfc = (mcluster / (mcluster - 1.)) * ((numRows - 1.) / (numRows - k));
        
    SymmetricPositiveDefiniteEigenDecomposition<Matrix> decomposition(
        bread, EigenvaluesOnly, ComputePseudoInverse);
    Matrix inverse_of_bread = decomposition.pseudoInverse();
    Matrix cov(k, k);
    cov = inverse_of_bread * meat * inverse_of_bread;

    MutableNativeColumnVector errs;
    MutableNativeColumnVector stats;
    MutableNativeColumnVector pValues;
    Allocator& allocator = defaultAllocator();
   
    errs.rebind(allocator.allocateArray<double>(k));
    stats.rebind(allocator.allocateArray<double>(k));
    pValues.rebind(allocator.allocateArray<double>(k));
    for (int i = 0; i < k; i++)
    {
        if (inverse_of_bread(i,i) < 0)
            errs(i) = 0;
        else
            errs(i) = std::sqrt(cov(i,i) * dfc);
        
        if (coef(i) == 0 && errs(i) == 0)
            stats(i) = 0;
        else
            stats(i) = coef(i) / errs(i);
    }

    if (numRows > k)
        (*func)(pValues, stats, numRows, k);

    AnyType tuple;

    tuple << coef << errs << stats
          << (numRows > k
              ? pValues
              : Null());
    return tuple;
}

// ------------------------------------------------------------------------

// compute t-stats
void __compute_t_stats (MutableNativeColumnVector& pValues,
                        MutableNativeColumnVector& stats,
                        int numRows, int k)
{
    for (int i = 0; i < k; i++)
        pValues(i) = 2. * prob::cdf(
            boost::math::complement(
                prob::students_t(static_cast<double>(numRows - k)),
                std::fabs(stats(i))));
}

// ------------------------------------------------------------------------

// compute t-stats
void __compute_z_stats (MutableNativeColumnVector& pValues,
                        MutableNativeColumnVector& stats,
                        int numRows, int k)
{
    (void)numRows;
    for (int i = 0; i < k; i++)
        pValues(i) = 2. * prob::cdf(
            boost::math::complement(prob::normal(), std::fabs(stats(i))));
}

// ------------------------------------------------------------------------
// linear clustered
// ------------------------------------------------------------------------

void __linear_trans_compute (MutableClusteredState& state,
                             const MappedColumnVector& x, const double& y)
{
    // On Redhat, I have to use the next a few lines instead of
    // state.meat_half += (y - trans(state.coef) * x) * x;
    double sm = 0;
    for (int i = 0; i < state.widthOfX; i++) sm += state.coef(i) * x(i);
    sm = y - sm;
    for (int i = 0; i < state.widthOfX; i++)
        state.meat_half(0,i) += sm * x(i);

    state.bread += x * trans(x);
}

// ------------------------------------------------------------------------

AnyType __clustered_err_lin_transition::run (AnyType& args)
{
    return __clustered_common_transition(args, false, __linear_trans_compute);
}

// ------------------------------------------------------------------------

AnyType __clustered_err_lin_merge::run (AnyType& args)
{
    return __clustered_common_merge(args);
}

// ------------------------------------------------------------------------

AnyType __clustered_err_lin_final::run (AnyType& args)
{
    return __clustered_common_final(args);
}

// ------------------------------------------------------------------------

AnyType clustered_lin_compute_stats::run (AnyType& args)
{
    return clustered_compute_stats(args, __compute_t_stats);
}

// ------------------------------------------------------------------------
// Logistic clustered standard errors
// ------------------------------------------------------------------------ 

inline double sigma(double x) {
	return 1. / (1. + std::exp(-x));
}

void __logistic_trans_compute (MutableClusteredState& state,
                               const MappedColumnVector& x, const double& y)
{
    double sm = 0;
    for (int i = 0; i < state.widthOfX; i++) sm += state.coef(i) * x(i);

    double sgn = y > 0 ? -1 : 1;
    double t1 = sigma(sgn * sm);
    double t2 = sigma(-sgn * sm);
    
    for (int i = 0; i < state.widthOfX; i++)
        state.meat_half(0,i) += t1 * sgn * x(i);

    state.bread += (t1 * t2) * (x * trans(x));
}

// ------------------------------------------------------------------------

AnyType __clustered_err_log_transition::run (AnyType& args)
{
    return __clustered_common_transition(args, true, __logistic_trans_compute);
}

// ------------------------------------------------------------------------

AnyType __clustered_err_log_merge::run (AnyType& args)
{
    return __clustered_common_merge(args);
}

// ------------------------------------------------------------------------

AnyType __clustered_err_log_final::run (AnyType& args)
{
    return __clustered_common_final(args);
}

// ------------------------------------------------------------------------

AnyType clustered_log_compute_stats::run (AnyType& args)
{
    return clustered_compute_stats(args, __compute_z_stats);
}

}
}
}
