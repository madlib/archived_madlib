/* ----------------------------------------------------------------------- *//**
 *
 * @file newton.hpp
 *
 * Generic implementaion of Newton's method, in the fashion of
 * user-definied aggregates. They should be called by actually database
 * functions, after arguments are properly parsed.
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_CONVEX_NEWTON_HPP
#define MADLIB_MODULES_CONVEX_NEWTON_HPP

namespace madlib {

namespace modules {

namespace convex {

// use Eigen
using namespace dbal;
using namespace madlib::dbal::eigen_integration;

template <class Container, class Accumulator>
class Newton : public DynamicStruct<Newton<Container, Accumulator>,Container> {
public:
    typedef DynamicStruct<Newton,Container> Base;
    MADLIB_DYNAMIC_STRUCT_TYPEDEFS;

    Newton(Init_type& inInitialization) : Base(inInitialization) {
        this->initialize();
    }
    void bind(ByteStream_type& inStream) {
        inStream >> num_coef;
        uint16_t M = num_coef.isNull()
            ? static_cast<uint16_t>(0)
            : static_cast<uint16_t>(num_coef);
        inStream
            >> is_applied
            >> beta.rebind(M)
            >> grad.rebind(M)
            >> hessian.rebind(M, M);
    }
    void reset() {
        is_applied = false;
        grad.setZero();
        hessian.setZero();
    }
    void apply() {
        if (is_applied) { return; }
        SymmetricPositiveDefiniteEigenDecomposition<Matrix> decomposition(
                hessian, EigenvaluesOnly, ComputePseudoInverse);

        beta -= decomposition.pseudoInverse() * grad;
        hessian = decomposition.pseudoInverse(); // become inverse after apply
        is_applied = true;
    }

    uint16_type         num_coef;   // number of variables
    bool_type           is_applied; // apply done used accumulated derivatives
    ColumnVector_type   beta;       // coefficients
    ColumnVector_type   grad;       // accumulating value of gradient
    Matrix_type         hessian;    // accumulating expected value of Hessian
};

} // namespace convex

} // namespace modules

} // namespace madlib

#endif // MADLIB_MODULES_CONVEX_NEWTON_HPP
