
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

AnyType __clustered_err_lin_transition::run (AnyType& args)
{
    MutableClusteredState state = args[0].getAs<MutableByteString>();
    const double& y = args[1].getAs<double>();
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
        state.ols_coef = coef;
        state.meat_half.setZero();
    }

    // dimension check
    if (state.widthOfX != static_cast<uint16_t>(x.size()))
        throw std::runtime_error("Inconsistent numbers of independent "
                                 "variables.");

    state.numRows++;

    // On Redhat, I have to use the next a few lines instead of
    // state.meat_half += (y - trans(state.ols_coef) * x) * x;
    double sm = 0;
    for (int i = 0; i < state.widthOfX; i++) sm += state.ols_coef(i) * x(i);
    sm = y - sm;
    for (int i = 0; i < state.widthOfX; i++) state.meat_half(0,i) += sm * x(i);

    state.bread += x * trans(x);

    return state.storage();
}

// ------------------------------------------------------------------------

AnyType __clustered_err_lin_merge::run (AnyType& args)
{
    //elog(INFO, "+++++++++++++++++++++++++");
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

AnyType __clustered_err_lin_final::run (AnyType& args)
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
AnyType clustered_compute_lin_stats::run (AnyType& args)
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
    MutableNativeColumnVector tStats;
    MutableNativeColumnVector pValues;
    Allocator& allocator = defaultAllocator();
   
    errs.rebind(allocator.allocateArray<double>(k));
    tStats.rebind(allocator.allocateArray<double>(k));
    pValues.rebind(allocator.allocateArray<double>(k));
    for (int i = 0; i < k; i++)
    {
        if (inverse_of_bread(i,i) < 0)
            errs(i) = 0;
        else
            errs(i) = std::sqrt(cov(i,i) * dfc);
        
        if (coef(i) == 0 && errs(i) == 0)
            tStats(i) = 0;
        else
            tStats(i) = coef(i) / errs(i);
    }

    if (numRows > k)
        for (int i = 0; i < k; i++)
            pValues(i) = 2. * prob::cdf(
                boost::math::complement(
                    prob::students_t(
                        static_cast<double>(numRows - k)
                                     ),
                    std::fabs(tStats(i))
                                        ));

    AnyType tuple;

    tuple << coef << errs << tStats
          << (numRows > k
              ? pValues
              : Null());
    return tuple;
}

}
}
}
