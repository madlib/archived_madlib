/* ----------------------------------------------------------------------- *//**
 *
 * @file arima.cpp
 *
 * @brief ARIMA
 *
 * @date Aug 21, 2013
 *//* ----------------------------------------------------------------------- */


#include <dbconnector/dbconnector.hpp>
#include <math.h>
#include <iostream>
#include <algorithm>
#include <functional>
#include <numeric>
#include <sstream>
#include "arima.hpp"

namespace madlib {
namespace modules {
namespace tsa {

using namespace dbal::eigen_integration;
using madlib::dbconnector::postgres::madlib_construct_array;
using madlib::dbconnector::postgres::madlib_construct_md_array;
using namespace std;


AnyType arima_residual::run (AnyType & args)
{
    int32_t distid = args[0].getAs<int32_t>();
    ArrayHandle<double> tvals = args[1].getAs<ArrayHandle<double> >();
    int p = args[2].getAs<int>();
    int d = args[3].getAs<int>();
    int q = args[4].getAs<int>();
    ArrayHandle<double> phi(NULL);
    if(p > 0)
        phi  = args[5].getAs<ArrayHandle<double> >();
    ArrayHandle<double> theta(NULL);
    if(q > 0)
        theta = args[6].getAs<ArrayHandle<double> >();
    double mean = 0.0;
    if(!args[7].isNull())
        mean = args[7].getAs<double>();
    ArrayHandle<double> prez(NULL);
    if(q > 0)
        prez = args[8].getAs<ArrayHandle<double> >();

    int ret_size = static_cast<int>((distid == 1) ? \
            (tvals.size()+d) : (tvals.size()-p));
    // FIXME: construct_array functions circumvent the abstraction layer. These
    // should be replaced with appropriate Allocator:: calls.
    MutableArrayHandle<double> res(
        madlib_construct_array(
            NULL, ret_size, FLOAT8OID, sizeof(double), true, 'd'));

    if (q < 1) {
        // compute the errors
        for(size_t i = p; i < tvals.size(); i++){
            double err = tvals[i] - mean;
            for(int j = 0; j < p; j++)
                err -= phi[j] * (tvals[i - j - 1] - mean);
            // note that for distid = 1, the first p residuals
            // will always be 0
            res[(distid == 1) ? i+d : (i - p)] = err;
        }
        return res;
    } else {
        // in this way we don't need to update prez explicitly
        double * errs = new double[ret_size + q];
        memset(errs, 0, sizeof(double) * (ret_size+q));
        // how to judge ArrayHandle is Null?
        if(distid != 1)
            for(int i = 0; i < q; i++) errs[i] = prez[i];

        // compute the errors
        for(size_t t = p; t < tvals.size(); t++){
            double err = tvals[t] - mean;

            for(int j = 0; j < p; j++)
                err -= phi[j] * (tvals[t - j - 1] - mean);

            for(int j = 0; j < q; j++)
                if(distid == 1)
                    err -= theta[j] * errs[t+q+d-j-1];
                else
                    err -= theta[j] * errs[t-p+q-j-1];
            errs[(distid == 1) ? (t+q+d) : (t - p + q)] = err;
        }
        memcpy(res.ptr(), errs + q, ret_size * sizeof(double));
        delete[] errs;
    }

    return res;
}

// ----------------------------------------------------------------------

static int * diff_coef (int d)
{
    int * coef = new int[d + 1];
    coef[0] = 1, coef[1] = -1;
    for(int i = 2; i <= d; i++)
        coef[i] = 0;

    for (int i = 1; i < d; i++) {
        coef[1 + i] = - coef[i];
        for(int j = i; j >= 1; j--)
            coef[j] = coef[j] - coef[j - 1];
    }

    return coef;
}

// ----------------------------------------------------------------------

AnyType arima_diff::run (AnyType & args)
{
    ArrayHandle<double> tvals = args[0].getAs<ArrayHandle<double> >();
    uint32_t d  = args[1].getAs<uint32_t>();
    int sz = static_cast<int>(tvals.size() - d);
    // FIXME: construct_array functions circumvent the abstraction layer. These
    // should be replaced with appropriate Allocator:: calls.
    MutableArrayHandle<double> diffs(
        madlib_construct_array(
            NULL, sz, FLOAT8OID, sizeof(double), true, 'd'));
    // get diff coef
    int* coef = diff_coef(d);

    // in-place diff
    for(size_t i = tvals.size() - 1; i >= d; i--){
        diffs[i-d] = 0;
        for(size_t j = 0; j <=d; j++)
            diffs[i-d] += coef[j] * tvals[i - j];
        // tvals[i] = diff;
    }

    // // set the first d elements to zero
    // for(int i = 0; i < d; i++)
    //     tvals[i] = 0;

    delete [] coef;
    return diffs;
}

// ----------------------------------------------------------------------

AnyType arima_adjust::run (AnyType & args)
{
    int distid = args[0].getAs<int>();

    if (distid == 1) return args[1];

    ArrayHandle<double> cur_tvals = args[1].getAs<ArrayHandle<double> >();
    ArrayHandle<double> pre_tvals = args[2].getAs<ArrayHandle<double> >();
    int32_t p  = args[3].getAs<int32_t>();

    // FIXME: construct_array functions circumvent the abstraction layer. These
    // should be replaced with appropriate Allocator:: calls.
    // note that curr_tvals.size() could be different from prez_tvals.size()
    MutableArrayHandle<double> res(
        madlib_construct_array(NULL,
                               static_cast<int>(cur_tvals.size() + p),
                               FLOAT8OID, sizeof(double), true, 'd'));

    // fill in the last p values from the previous tvals
    for(int i = 0; i < p; i++)
        res[i] = pre_tvals[pre_tvals.size() - p + i];
    // copy the current tvals
    for(size_t i = 0; i < cur_tvals.size(); i++)
        res[p + i] = cur_tvals[i];

    return res;
}

// ----------------------------------------------------------------------

AnyType arima_lm_delta::run (AnyType & args)
{
    MappedColumnVector jj = args[0].getAs<MappedColumnVector>();
    MappedColumnVector g = args[1].getAs<MappedColumnVector>();
    double u = args[2].getAs<double>();

    size_t l = g.size();
    Matrix m_jj(jj);
    m_jj.resize(l, l);

    Matrix a = m_jj.diagonal().asDiagonal();
    a = m_jj.transpose() + u * a;

    ColumnVector x = a.lu().solve(g);
    return x;
}

// ----------------------------------------------------------------------

AnyType arima_lm::run (AnyType & args)
{
    int distid = args[0].getAs<int>();
    MutableArrayHandle<double> tvals = args[1].getAs<MutableArrayHandle<double> >();
    int p = args[2].getAs<int>();
    int q = args[3].getAs<int>();
    ArrayHandle<double> phi(NULL);
    if (p > 0)
        phi = args[4].getAs<ArrayHandle<double> >();
    ArrayHandle<double> theta(NULL);
    if (q > 0)
        theta = args[5].getAs<ArrayHandle<double> >();

    bool include_mean = true;
    double mean = 0.0;
    if (args[6].isNull())
        include_mean = false;
    else
        mean = args[6].getAs<double>();

    int l = p + q;
    if (include_mean) l++;

    // FIXME: construct_array functions circumvent the abstraction layer. These
    // should be replaced with appropriate Allocator:: calls.
    MutableArrayHandle<double> prez(
        madlib_construct_array(
            NULL, 0, FLOAT8OID, sizeof(double), true, 'd'));
    MutableArrayHandle<double> prej(
        madlib_construct_array(
            NULL, 0, FLOAT8OID, sizeof(double), true, 'd'));
    if (q > 0) {
        if (distid == 1) {
            prez = madlib_construct_array(
                NULL, q, FLOAT8OID, sizeof(double), true, 'd');
            prej = madlib_construct_array(
                NULL, q * l,  FLOAT8OID, sizeof(double), true, 'd');
        } else {
            prez = args[7].getAs<MutableArrayHandle<double> >();
            prej = args[8].getAs<MutableArrayHandle<double> >();
        }
    }

    // minus the mean
    if (include_mean)
        for(size_t i = 0; i < tvals.size(); i++)
            tvals[i] -= mean;

    double z2 = 0;
    // FIXME: construct_array functions circumvent the abstraction layer. These
    // should be replaced with appropriate Allocator:: calls.
    MutableArrayHandle<double> jj(
        madlib_construct_array(
            NULL, l * l, FLOAT8OID, sizeof(double), true, 'd'));
    MutableArrayHandle<double> jz(
        madlib_construct_array(
            NULL, l, FLOAT8OID, sizeof(double), true, 'd'));

    for (size_t t = p; t < tvals.size(); t++) {
        // compute the error and Jacob
        double * jacob = new double[l];
        for (int i = 0; i < l; i++) jacob[i] = 0;
        double err = 0;

        // compute error
        err = tvals[t];
        for (int i = 0; i < p; i++)
            err -= phi[i] * tvals[t - 1 - i];
        for (int i = 0; i < q; i++)
            err -= theta[i] * prez[q - i - 1];

        // compute J
        // compute the partial derivatives over phi
        for (int i = 0; i < p; i++) {
            jacob[i] = tvals[t - i - 1];
            // recursive part
            for(int j = 0; j < q; j++)
                jacob[i] -= theta[j] * prej[(q - j - 1) * l + i];
        }

        // compute the partial derivatives over theta
        for (int i = 0; i < q; i++) {
            jacob[p + i] = prez[q - i - 1];
            // recursive part
            for(int j = 0; j < q; j++)
                jacob[p + i] -= theta[j] * prej[(q - j - 1) * l + p + i];
        }

        // compute the partial derivatives over mean
        if (include_mean) {
            jacob[p + q] = 1;
            for(int i = 0; i < p; i++)
                jacob[p + q] -= phi[i];
            for(int i = 0; i < q; i++)
                jacob[p + q] -= theta[i] * prej[(q - i - 1) * l + p + q];
        }

        // update Z^2
        z2 += err * err;

        if(q != 0){
            // update PreZ
            for(int i = 0; i < q - 1; i++)
                prez[i] = prez[i + 1];
            prez[q-1] = err;

            // update PreJ
            for(int i = 0; i < l * (q - 1); i++)
                prej[i] = prej[i + l];
            for(int i = 0; i < l; i++)
                prej[l * (q - 1) + i] = jacob[i];
        }

        // update J^T * J
        for(int i = 0; i < l; i++)
            for(int j = 0; j < l; j++)
                jj[i * l + j] += jacob[i] * jacob[j];

        // update jz
        for(int i = 0; i < l; i++)
            jz[i] += jacob[i] * err;

        // delete jacob
        if(jacob) delete[] jacob;
    }

    AnyType tuple;
    tuple << z2 << jj << jz << prez << prej;
    return tuple;
}

// ----------------------------------------------------------------------

AnyType arima_lm_result_sfunc::run (AnyType& args)
{
    ArrayHandle<double> jj = args[1].getAs<ArrayHandle<double> >();
    ArrayHandle<double> jz = args[2].getAs<ArrayHandle<double> >();
    double z2 = args[3].getAs<double>();
    int l = static_cast<int>(jz.size());
    int l2 = l*l;

    MutableArrayHandle<double> state(NULL);

    // FIXME: construct_array functions circumvent the abstraction layer. These
    // should be replaced with appropriate Allocator:: calls.
    if (args[0].isNull()) {
        // state[0:(l*l-1)]         - jj
        // state[(l*l):(l*l+l-1)]   - jz
        // state[l*l+l]             - z2
        // state[l*l+l+1]           - l
        state = madlib_construct_array(NULL, l2 + l + 2,
                                       FLOAT8OID, sizeof(double), true, 'd');

        for (int i = 0; i < l2; i++) state[i] = jj[i];
        for (int i = 0; i < l; i++) state[l2 + i] = jz[i];
        state[l2 + l] = z2;
        state[l2 + l + 1] = l;
    } else {
        state = args[0].getAs<MutableArrayHandle<double> >();
        for (int i = 0; i < l2; i++) state[i] += jj[i];
        for (int i = 0; i < l; i++) state[l2 + i] += jz[i];
        state[l2 + l] += z2;
    }

    return state;
}

// ----------------------------------------------------------------------

AnyType arima_lm_result_pfunc::run (AnyType& args)
{
    if (args[0].isNull() && args[1].isNull()) return args[0];
    if (args[0].isNull()) return args[1].getAs<ArrayHandle<double> >();
    if (args[1].isNull()) return args[0].getAs<ArrayHandle<double> >();

    MutableArrayHandle<double> state1 = args[0].getAs<MutableArrayHandle<double> >();
    ArrayHandle<double> state2 = args[1].getAs<ArrayHandle<double> >();
    for (size_t i = 0; i < state1.size() - 1; i++)
        state1[i] += state2[i];

    return state1;
}

// ----------------------------------------------------------------------

AnyType arima_lm_result_ffunc::run (AnyType& args)
{
    ArrayHandle<double> state = args[0].getAs<ArrayHandle<double> >();
    int l = static_cast<int>(state[state.size() - 1]);

    double z2 = state[state.size() - 2];
    const double* jj = state.ptr();
    const double* jz = state.ptr() + l * l;

    double mx = 0;
    for (int i = 0; i < l; i++) {
        int ll = (l + 1) * i;
        if (jj[ll] > mx)
            mx = jj[ll];
    }

    // FIXME: construct_array functions circumvent the abstraction layer. These
    // should be replaced with appropriate Allocator:: calls.
    MutableArrayHandle<double> arr_jj(
        madlib_construct_array(
            NULL, l*l, FLOAT8OID, sizeof(double), true, 'd'));
    MutableArrayHandle<double> arr_jz(
        madlib_construct_array(
            NULL, l, FLOAT8OID, sizeof(double), true, 'd'));

    memcpy(arr_jj.ptr(), jj, l*l*sizeof(double));
    memcpy(arr_jz.ptr(), jz, l*sizeof(double));

    AnyType tuple;
    tuple << arr_jj << arr_jz << z2 << mx;
    return tuple;
}

// ----------------------------------------------------------------------

static double error(int tid, const double * tvals, int p, int q,
                    const double * phi, const double * theta, const double * prez)
{
    double err = 0.0;
    if(tid > p){
        err = tvals[p];
        for(int i = 0; i < p; i++)
            err -= phi[i] * tvals[p - i - 1];
        for(int i = 0; i < q; i++)
            err -= theta[i] * prez[q - i - 1];
    }

    return err;
}

// ----------------------------------------------------------------------

static double error(int tid, const double * tvals, int p, int q,
                    double * phi, double * theta, const double * prez,
                    double delta, int pos1, int pos2, int sign1, int sign2)
{
    double dmean = 0.0; // change of mean
    // Add delta
    if (pos1 == pos2){
        if (pos1 < p)
            phi[pos1] += sign1 * delta;
        else if (pos1 < p + q)
            theta[pos1-p] += sign1 * delta;
        else
            dmean = sign1 * delta;
    } else {
        if (pos1 < p)
            phi[pos1] += sign1 * delta / 2;
        else if (pos1 < p + q)
            theta[pos1-p] += sign1 * delta / 2;
        else
            dmean = sign1 * delta / 2;

        if (pos2 < p)
            phi[pos2] += sign2 * delta / 2;
        else if (pos2 < p + q)
            theta[pos2-p] += sign2 * delta / 2;
        else
            dmean = sign2 * delta / 2;
    }

    double err = 0.0;
    if (tid > p) {
        err = tvals[p] - dmean;
        for (int i = 0; i < p; i++)
            err -= phi[i] * (tvals[p - i - 1] - dmean);
        for (int i = 0; i < q; i++)
            err -= theta[i] * prez[q - i - 1];
    }

    // Restore the original coefficients
    if (pos1 == pos2) {
        if (pos1 < p)
            phi[pos1] -= sign1 * delta;
        else if (pos1 < p + q)
            theta[pos1-p] -= sign1 * delta;
    } else {
        if (pos1 < p)
            phi[pos1] -= sign1 * delta / 2;
        else if (pos1 < p + q)
            theta[pos1-p] -= sign1 * delta / 2;

        if (pos2 < p)
            phi[pos2] -= sign2 * delta / 2;
        else if (pos2 < p + q)
            theta[pos2-p] -= sign2 * delta / 2;
    }
    return err;
}

// ----------------------------------------------------------------------

static double * update_prez(double * prez, int q, double z)
{
    if (q != 0) {
        for (int i = 1; i < q; i++)
            prez[i - 1] = prez[i];
        prez[q-1] = z;
    }
    return prez;
}

// ----------------------------------------------------------------------

AnyType arima_lm_stat_sfunc::run (AnyType& args)
{
    int distid = args[1].getAs<int>();
    MutableArrayHandle<double> tvals = args[2].getAs<MutableArrayHandle<double> >();
    int p = args[3].getAs<int>();
    int q = args[4].getAs<int>();
    MutableArrayHandle<double> phi = args[5].getAs<MutableArrayHandle<double> >();
    MutableArrayHandle<double> theta = args[6].getAs<MutableArrayHandle<double> >();
    bool include_mean = true;
    double mean = 0.0;
    if (args[7].isNull())
        include_mean = false;
    else
        mean = args[7].getAs<double>();
    double delta = args[8].getAs<double>();

    int l = p + q;
    if(include_mean)
        l++;

    // minus the mean
    if (include_mean)
        for(size_t i = 0; i < tvals.size(); i++)
            tvals[i] -= mean;

    MutableArrayHandle<double> state(NULL);

    // FIXME: construct_array functions circumvent the abstraction layer. These
    // should be replaced with appropriate Allocator:: calls.
    if (args[0].isNull()) {
        // Eqs. (1.2.21, 1.2.22) tells how many Z^2 are needed
        // Also Hessian is symmetric
        int sz = (2 * l * l + 1) * (1 + q) + 4;
        // state[0]                     -- l
        // state[1]                     -- delta
        // state[2]                     -- N
        // state[3:(2*l*l+3)]           -- all the Z^2 needed for numerically differentiation
        // state[(2*l*l+4):((2*l*l+1)*q+(2*l*l+3))]  -- all the PreZ
        // last one   --- p
        state = madlib_construct_array(
            NULL, sz, FLOAT8OID, sizeof(double), true, 'd');
        for (size_t i = 0; i < state.size(); i++) state[i] = 0.;
        state[0] = l;
        state[1] = delta;
        state[2] = 0;
        state[sz-1] = p;
    } else {
        state = args[0].getAs<MutableArrayHandle<double> >();
    }

    // Referring to Eqs. (1.2.21, 1.2.22):
    // If a != b in Eqs. (1.2.21, 1.2.22), we need 4 sum(z^2) for each pair
    // of coef values. [4 * l(l-1)/2 = 2l(l-1)]
    // If a == b, we only need three. One of the three sum(z^2) is for
    // coef without any changes. [ 2l + 1]
    // Also H_{ab} = H_{ba}, so totally we need to measure 2l^2 + 1
    // differernt sum(z^2)'s

    // Comute Z^2 and update the state
    // The one without delta
    int prez_offset = 4 + 2 * l * l;
    for (size_t t = p; t < tvals.size(); t++) {
        int dtid = static_cast<int>((distid == 1) ? (1+t) : (p+1));
        int dtv = static_cast<int>(t - p);
        double * prez = state.ptr() + prez_offset;

        double err = error(dtid, tvals.ptr()+dtv, p, q, phi.ptr(), theta.ptr(), prez);
        state[3] += err * err;
        update_prez(prez, q, err);

        // The others with delta
        int count = 0;
        for (int i = 0; i < l; i++) {
            for (int j = i; j < l; j++) {
                if (i == j) {
                    int sign[][2] = {{1, 1}, {-1, -1}};
                    for(int k = 0; k < 2; k++){
                        prez = state.ptr() + prez_offset + q + (i * 2 + k) * q;
                        err = error(dtid, tvals.ptr()+dtv, p, q, phi.ptr(), theta.ptr(), prez, delta, i, j, sign[k][0], sign[k][1]);
                        state[4 + i * 2 + k] += err * err;
                        update_prez(prez, q, err);
                    }
                } else {
                    int sign[][2] = {{1, 1}, {-1, 1}, {1, -1}, {-1, -1}};
                    for (int k = 0; k < 4; k++) {
                        prez = state.ptr() + prez_offset + (1 + 2 * l) * q + q * (count + k);
                        err = error(dtid, tvals.ptr()+dtv, p, q, phi.ptr(), theta.ptr(), prez, delta, i, j, sign[k][0], sign[k][1]);
                        state[4 + 2 * l + count + k] += err * err;
                        update_prez(prez, q, err);
                    }
                    count += 4;
                }
            }
        }
    }

    if (distid == 1) { state[2] += static_cast<double>(tvals.size()); }
    else { state[2] += static_cast<double>(tvals.size() - p); }

    return state;
}

// ----------------------------------------------------------------------

AnyType arima_lm_stat_ffunc::run (AnyType& args)
{
    // extract residual variance, compute log-likelihood
    ArrayHandle<double> state = args[0].getAs<ArrayHandle<double> >();
    int l = static_cast<int>(state[0]);
    double delta = state[1];
    int N = static_cast<int>(state[2]);
    int p = static_cast<int>(state[state.size() - 1]);

    double z2 = state[3];
    double sigma2 = z2 / (N-p);
    double loglik = -(1 + log(2 * M_PI * sigma2)) * N / 2;
    double delta2 = delta * delta * 2. * z2 / N;

    // compute the Hessian matrix
    double * hessian = new double[l * l];
    int count = 0;
    int offset = 4 + 2 * l;
    for (int i = 0; i < l; i++) {
        for (int j = i; j < l; j++) {
            if(j == i) {
                hessian[l * i + i] = (state[4 + i * 2] - 2 * z2 + state[4 + i * 2 + 1]) / delta2;
            } else {
                hessian[l * i + j] = (state[offset + count] - state[offset + count + 1]
                    - state[offset + count + 2] + state[offset + count + 3]) / delta2;
                hessian[l * j + i] = hessian[l * i + j];
                count += 4;
            }
        }
    }

    // inverse the Hessian matrixand to obtain the std errors
    Eigen::Map<Matrix> m(hessian, l, l);
    SymmetricPositiveDefiniteEigenDecomposition<Matrix> decomposition(
         m.transpose(), EigenvaluesOnly, ComputePseudoInverse);
    ColumnVector diag = decomposition.pseudoInverse().diagonal();

    // FIXME: construct_array functions circumvent the abstraction layer. These
    // should be replaced with appropriate Allocator:: calls.
    MutableArrayHandle<double> std_err(
        madlib_construct_array(
            NULL, l, FLOAT8OID, sizeof(double), true, 'd'));
    for (int i = 0; i < l; i++)
        std_err[i] = sqrt(diag[i]);

    delete [] hessian;

    AnyType tuple;
    tuple << std_err << sigma2 << loglik;
    return tuple;
}

}
}
}
