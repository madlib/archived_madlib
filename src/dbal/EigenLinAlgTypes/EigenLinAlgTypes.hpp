/* ----------------------------------------------------------------------- *//**
 *
 * @file EigenLinAlgTypes.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_EIGEN_LINALGTYPES_HPP
// Note: We also use MADLIB_EIGEN_LINALGTYPES_HPP for a workaround for Doxygen:
// *_{impl|proto}.hpp files should be ingored if not included by
// EigenLinAlgTypes.hpp
#define MADLIB_EIGEN_LINALGTYPES_HPP

// Plugin with methods to add to Eigen's MatrixBase
#define EIGEN_MATRIXBASE_PLUGIN <dbal/EigenLinAlgTypes/EigenPlugin.hpp>
#include <Eigen/Dense>

namespace madlib {

    namespace dbal {
        #include "EigenTypes_proto.hpp"
        #include "HandleMap_proto.hpp"
        #include "SymmetricPositiveDefiniteEigenDecomposition_proto.hpp"
        
        #include "HandleMap_impl.hpp"
        #include "SymmetricPositiveDefiniteEigenDecomposition_impl.hpp"
    }
    
}

#endif // defined(MADLIB_EIGEN_LINALGTYPES_HPP)
