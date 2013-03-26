
/*
  Some utility functions shared by multiple optimizers
 */

#include "dbconnector/dbconnector.hpp"

#ifndef ELASTIC_NET_SHARED_UTILS_
#define ELASTIC_NET_SHARED_UTILS_

namespace madlib {
namespace modules {
namespace elastic_net {

using namespace madlib::dbal::eigen_integration;

typedef HandleTraits<MutableArrayHandle<double> >::ColumnVectorTransparentHandleMap CVector;

namespace {

/*
  dot product of sparese vector
 */
inline double sparse_dot (CVector& coef, MappedColumnVector& x)
{
    double sum = 0;
    for (int i = 0; i < x.size(); i++)
        if (coef(i) != 0) sum += coef(i) * x(i);
    return sum;
}

// ------------------------------------------------------------------------

inline double sparse_dot (CVector& coef, CVector& x)
{
    double sum = 0;
    for (int i = 0; i < x.size(); i++)
        if (coef(i) != 0) sum += coef(i) * x(i);
    return sum;
}

// ------------------------------------------------------------------------

inline double sparse_dot (ColumnVector& coef, CVector& x)
{
    double sum = 0;
    for (int i = 0; i < x.size(); i++)
        if (coef(i) != 0) sum += coef(i) * x(i);
    return sum;
}

// ------------------------------------------------------------------------

inline double sparse_dot (ColumnVector& coef, ColumnVector& x)
{
    double sum = 0;
    for (int i = 0; i < x.size(); i++)
        if (coef(i) != 0) sum += coef(i) * x(i);
    return sum;
}

// ------------------------------------------------------------------------

inline double sparse_dot (MappedColumnVector& coef, MappedColumnVector& x)
{
    double sum = 0;
    for (int i = 0; i < x.size(); i++)
        if (coef(i) != 0) sum += coef(i) * x(i);
    return sum;
}

}

}
}
}

#endif
