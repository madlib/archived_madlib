
#include <dbconnector/dbconnector.hpp>
#include <boost/math/distributions.hpp>
#include <modules/prob/student.hpp>
#include <modules/prob/boost.hpp>
#include <limits>
#include <string>

#include "clustered_errors.hpp"
#include "clustered_errors_state.hpp"

using namespace madlib::dbal::eigen_integration;

using std::string;

namespace madlib {
namespace modules {
namespace regress {

// ------------------------------------------------------------------------

typedef ClusteredState<RootContainer> IClusteredState;
typedef ClusteredState<MutableRootContainer> MutableClusteredState;

// ------------------------------------------------------------------------

// function used by linear and logistic transitions
AnyType __clustered_common_transition (AnyType& args, string regressionType,
                                       void (*func)(
                                           MutableClusteredState&,
                                           const MappedColumnVector&,
                                           const double& y))
{
    // Early return because of an exception has been "thrown" 
    // (actually "warning") in the previous invocations
    if (args[0].isNull())
        return Null();
    MutableClusteredState state = args[0].getAs<MutableByteString>();

   if (args[1].isNull() || args[2].isNull()) {
        return args[0];
    }

    // Get x as a vector of double
    MappedColumnVector x;
    try{
        // an exception is raised in the backend if args[2] contains nulls
        MappedColumnVector xx = args[2].getAs<MappedColumnVector>();
        // x is a const reference, we can only rebind to change its pointer
        x.rebind(xx.memoryHandle(), xx.size());
    } catch (const ArrayWithNullException &e) {
        return args[0];
    }

    double y;
    if (regressionType == "log") //Logistic regression
        y = args[1].getAs<bool>() ? 1. : -1;
    else if(regressionType == "lin") // Linear regression
        y = args[1].getAs<double>();
    else if(regressionType == "mlog")//Multi-logistic regression
    	y = args[1].getAs<int>();

    // const MappedColumnVector& x = args[2].getAs<MappedColumnVector>();

    if (!std::isfinite(y)) {
        //throw std::domain_error("Dependent variables are not finite.");
        warning("Dependent variables are not finite.");
        return Null();
    } else if (x.size() > std::numeric_limits<uint16_t>::max()) {
        //throw std::domain_error("Number of independent variables cannot be "
        //                        "larger than 65535.");
        warning("Number of independent variables cannot be larger than 65535.");
        return Null();
    }

    if (state.numRows == 0) {
        if(regressionType == "mlog")
        {
            if (args[4].isNull() || args[5].isNull()) {
                return args[0];
            }
        	state.numCategories = static_cast<uint16_t>(args[4].getAs<int>());
        	state.refCategory = static_cast<uint16_t>(args[5].getAs<int>());
        } else {
        	state.numCategories = 2;
        	state.refCategory = 0;
        }

        state.widthOfX = static_cast<uint16_t>(x.size() * (state.numCategories-1));
        state.resize();

        if(regressionType == "mlog") {
	            MappedMatrix coefMat = args[3].getAs<MappedMatrix>();
                Matrix mat = coefMat;
                mat.transposeInPlace();
                mat.resize(coefMat.size(), 1);
                state.coef = mat;
        } else {
            const MappedColumnVector& coef = args[3].getAs<MappedColumnVector>();
            state.coef = coef;
        }
        state.meat_half.setZero();
    }

    // dimension check
    if (state.widthOfX != static_cast<uint16_t>(x.size() * (state.numCategories-1))) {
        //throw std::runtime_error("Inconsistent numbers of independent "
        //                         "variables.");
        warning("Inconsistent numbers of independent variables.");
        return Null();
    }
    state.numRows++;
    (*func)(state, x, y);
    return state.storage();
}

// ------------------------------------------------------------------------

// function used by linear and logistic merges
AnyType __clustered_common_merge (AnyType& args)
{
    // In case the aggregator should be terminated because
    // an exception has been "thrown" in the transition function
    if(args[0].isNull() || args[1].isNull())
        return Null();
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
    // In case the aggregator should be terminated because
    // an exception has been "thrown" in the transition function
    if (args[0].isNull())
        return Null(); 
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
                                     int, int), bool ismlogr)
{
    //const MappedColumnVector& coef = args[0].getAs<MappedColumnVector>();
    ColumnVector coef;
    if (ismlogr){
	    MappedMatrix coefMat = args[0].getAs<MappedMatrix>();
        Matrix mat = coefMat;
        mat.transposeInPlace();
        mat.resize(coefMat.size(), 1);
        coef = mat;
    }else{
        coef = args[0].getAs<MappedColumnVector>();
    }
    const MappedColumnVector& meatvec = args[1].getAs<MappedColumnVector>();
    const MappedColumnVector& breadvec = args[2].getAs<MappedColumnVector>();
    int mcluster = args[3].getAs<int>();
    int numRows = args[4].getAs<int>();
    int k = static_cast<int>(coef.size());
    Matrix bread(k,k);
    Matrix meat(k,k);
    int count = 0;

    for (int i = 0; i < k; i++)
        for (int j = 0; j < k; j++)
        {
            meat(i,j) = meatvec(count);
            bread(i,j) = breadvec(count);
            count++;
        }

    if (mcluster == 1) {
        //throw std::domain_error ("Clustered variance error: Number of clusters cannot be smaller than 2!");
        warning("Clustered variance error: Number of clusters cannot be smaller than 2!");
        return Null();
    }
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
    return __clustered_common_transition(args, "lin", __linear_trans_compute);
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
    return clustered_compute_stats(args, __compute_t_stats, false);
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
    return __clustered_common_transition(args, "log", __logistic_trans_compute);
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
    return clustered_compute_stats(args, __compute_z_stats, false);
}


// ------------------------------------------------------------------------
// Multi-Logistic clustered standard errors
// ------------------------------------------------------------------------


void __mlogistic_trans_compute (MutableClusteredState& state,
                               const MappedColumnVector& x, const double& y)
{
	int numCategories = state.numCategories -1;
	ColumnVector yVec(numCategories);
    yVec.fill(0);

    //Pivot around the reference category
    if (y > state.refCategory) {
        yVec((int)y - 1) = 1;
    } else if (y < state.refCategory) {
        yVec((int)y) = 1;
    }

//    if ((int)y != 0) {
 //       yVec(((int)y) - 1) = 1;
 //   }

	/*
    Compute the parameter vector (the 'pi' vector in the documentation)
    for the data point being processed.
    Casting the coefficients into a matrix makes the calculation simple.
    */
    Matrix coef = state.coef;
    coef.resize(numCategories, state.widthOfX/numCategories);

    //Store the intermediate calculations because we'll reuse them in the LLH
    ColumnVector t1 = x; //t1 is vector of size state.widthOfX
    t1 = coef*x;
    /*
        Note: The above 2 lines could have been written as:
        ColumnVector t1 = -coef*x;

        but this creates warnings. These warnings are somehow related to the factor
        that x is an immutable type.
    */

    ColumnVector t2 = t1.array().exp();
    double t3 = 1 + t2.sum();
    ColumnVector pi = t2/t3;
    //The gradient matrix has numCatergories rows and widthOfX columns
    Matrix grad = -yVec * x.transpose() + pi * x.transpose();
    //We cast the gradient into a vector to make the math easier.
    grad.resize(state.widthOfX, 1);
    for (int i = 0; i < state.widthOfX; i++)
    {
		state.meat_half(0,i) += grad(i);
	}

	// Compute the 'a' matrix.
    Matrix a(numCategories,numCategories);
    Matrix piDiag = pi.asDiagonal();
    a = pi * pi.transpose() - piDiag;

    //Start the Hessian calculations
    //Matrix X_transp_AX(numCategories * state.widthOfX, numCategories * state.widthOfX);
 	Matrix X_transp_AX( (int)state.widthOfX, (int)state.widthOfX);
    /*
        Again: The following 3 lines could have been written as
        Matrix XXTrans = x * x.transpose();
        but it creates warnings related to the type of x. Here is an easy fix
    */
    Matrix cv_x = x;
    Matrix XXTrans = trans(cv_x);
    XXTrans = cv_x * XXTrans;

    //Eigen doesn't supported outer-products for matrices, so we have to do our own.
    //This operation is also known as a tensor-product.
    for (int i1 = 0; i1 < (state.widthOfX/numCategories); i1++){
         for (int i2 = 0; i2 < (state.widthOfX/numCategories); i2++){
            int rowOffset = numCategories * i1;
            int colOffset = numCategories * i2;

            X_transp_AX.block(rowOffset, colOffset, numCategories,  numCategories) = XXTrans(i1, i2) * a;
        }
    }

    state.bread += -1*X_transp_AX;
}

// ------------------------------------------------------------------------

AnyType __clustered_err_mlog_transition::run (AnyType& args)
{
    return __clustered_common_transition(args, "mlog", __mlogistic_trans_compute);
}

// ------------------------------------------------------------------------

AnyType __clustered_err_mlog_merge::run (AnyType& args)
{
    return __clustered_common_merge(args);
}

// ------------------------------------------------------------------------

AnyType __clustered_err_mlog_final::run (AnyType& args)
{
    return __clustered_common_final(args);
}

// ------------------------------------------------------------------------

AnyType clustered_mlog_compute_stats::run (AnyType& args)
{
    return clustered_compute_stats(args,  __compute_z_stats, true);
}

}
}
}
