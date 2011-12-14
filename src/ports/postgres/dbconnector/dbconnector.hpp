/* ----------------------------------------------------------------------- *//**
 *
 * @file dbconnector.hpp
 *
 * @brief This file should be included by user code (and nothing else)
 *
 *//* ----------------------------------------------------------------------- */


#ifndef MADLIB_POSTGRES_DBCONNECTOR_HPP
#define MADLIB_POSTGRES_DBCONNECTOR_HPP

extern "C" {
    #include <postgres.h>
    #include <funcapi.h>
    #include <catalog/pg_type.h>
    #include <miscadmin.h>         // Memory allocation, e.g., HOLD_INTERRUPTS
    #include <utils/array.h>
    #include <utils/builtins.h>    // needed for format_procedure()
    #include <executor/executor.h> // Greenplum requires this for GetAttributeByNum()
    #include <utils/typcache.h>    // type conversion, e.g., lookup_rowtype_tupdesc
    #include <utils/lsyscache.h>   // for type lookup, e.g., type_is_rowtype
} // extern "C"

#include "Compatibility.hpp"

#include <boost/unordered_map.hpp> // For Oid to bool map (if types are tuples)
#include <boost/type_traits/is_const.hpp>
#include <boost/math/special_functions/fpclassify.hpp>
#include <boost/logic/tribool.hpp>
#include <limits>
#include <stdexcept>
#include <vector>
#include <fstream>

// FIXME: For now we make use of TR1 but not of C++11. Import all TR1 names into std.
namespace std {
    using boost::unordered_map;
    using boost::hash;
}

#define MADLIB_DEFAULT_EXCEPTION std::runtime_error("Internal error")
#define madlib_assert(_cond, _exception) \
    do { \
        if(!(_cond)) \
            throw _exception; \
    } while(false)

#include <dbal/dbal.hpp>
#include <utils/Reference.hpp>
#include <utils/MallocAllocator.hpp> // for hash map of which type is a tuple

namespace madlib {
    namespace dbconnector {
        namespace postgres {
            template <int MapOptions>
            struct EigenTypes;

            #include "AbstractionLayer_proto.hpp"
            #include "Allocator_proto.hpp"
            #include "ArrayHandle_proto.hpp"
            #include "AnyType_proto.hpp"
            #include "TransparentHandle_proto.hpp"
            #include "PGException_proto.hpp"
            #include "OutputStreamBuffer_proto.hpp"
            #include "UDF_proto.hpp"
        }
    }

    // Import required names into the madlib namespace
    using dbconnector::postgres::AbstractionLayer;
    typedef AbstractionLayer::AnyType AnyType; // Needed for defining UDFs
    
    /**
     * @brief Get the default allocator
     */
    inline AbstractionLayer::Allocator &defaultAllocator() {
        static AbstractionLayer::Allocator sDefaultAllocator(NULL);
        return sDefaultAllocator;
    }
}

#define EIGEN_MATRIXBASE_PLUGIN <dbal/EigenPlugin.hpp>
#include <Eigen/Dense>

namespace madlib {
    #include <dbal/EigenTypes.hpp>

    typedef EigenTypes<Eigen::Unaligned> DefaultLinAlgTypes;

    namespace dbconnector {
        namespace postgres {
            #include "TypeTraits.hpp"
            #include "Allocator_impl.hpp"
            #include "AnyType_impl.hpp"
            #include "ArrayHandle_impl.hpp"
            #include "TransparentHandle_impl.hpp"
            #include "OutputStreamBuffer_impl.hpp"
            #include "UDF_impl.hpp"
        }
    }
}

#define DECLARE_UDF(name) \
    struct name : public madlib::dbconnector::postgres::UDF, public DefaultLinAlgTypes { \
        typedef DefaultLinAlgTypes LinAlgTypes; \
        inline name(FunctionCallInfo fcinfo) : madlib::dbconnector::postgres::UDF(fcinfo) { }  \
        AnyType run(AnyType &args); \
    };


#define DECLARE_UDF_EXTERNAL(name) \
    namespace external { \
        extern "C" { \
            PG_FUNCTION_INFO_V1(name); \
            Datum name(PG_FUNCTION_ARGS) { \
                return madlib::dbconnector::postgres::UDF::call<MADLIB_CURRENT_NAMESPACE::name>(fcinfo); \
            } \
        } \
    }

#endif
