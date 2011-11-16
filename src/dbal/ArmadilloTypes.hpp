/* ----------------------------------------------------------------------- *//**
 *
 * @file ArmadilloTypes.hpp
 *
 * @brief Helper class for integration of Vector_const with Armadillo
 *
 * An arma::Base object can be converted to a Mat object by the unwrap class.
 * See also armadillo_bits/base.hpp and armadillo_bits/unwrap.hpp
 *
 *//* ----------------------------------------------------------------------- */

struct ArmadilloTypes {
//    template <template <class> class T, typename eT> class Vector;
//    template <template <class> class T, typename eT> class Vector_const;
//    template <typename eT> class Matrix;

    #include "ArmadilloMatrix.hpp"
    #include "ArmadilloVector.hpp"
    #include "ArmadilloVector_const.hpp"

    typedef Matrix<double> DoubleMat;
    typedef Vector<arma::Col, double> DoubleCol;
    typedef Vector_const<arma::Col, double> DoubleCol_const;
    typedef Vector<arma::Row, double> DoubleRow;
    typedef Vector_const<arma::Row, double> DoubleRow_const;
};

#define USE_ARMADILLO_TYPES \
    typedef ArmadilloTypes::Matrix<double> DoubleMat; \
    typedef ArmadilloTypes::Vector<arma::Col, double> DoubleCol; \
    typedef ArmadilloTypes::Vector_const<arma::Col, double> DoubleCol_const; \
    typedef ArmadilloTypes::Vector<arma::Row, double> DoubleRow; \
    typedef ArmadilloTypes::Vector_const<arma::Row, double> DoubleRow_const
