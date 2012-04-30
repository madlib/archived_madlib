/* ----------------------------------------------------------------------- *//**
 *
 * @file dbconnector.hpp
 *
 * @brief This file should be included by user code (and nothing else)
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_DBCONNECTOR_HPP
#define MADLIB_POSTGRES_DBCONNECTOR_HPP

// On platforms based on PostgreSQL we can include a different set of headers.
// Also, there may be different compatibility headers for other platforms.
#ifndef MADLIB_POSTGRES_HEADERS
#define MADLIB_POSTGRES_HEADERS

extern "C" {
    #include <postgres.h>
    #include <funcapi.h>
    #include <catalog/pg_proc.h>
    #include <catalog/pg_type.h>
    #include <executor/executor.h> // For GetAttributeByNum()
    #include <miscadmin.h>         // Memory allocation, e.g., HOLD_INTERRUPTS
    #include <utils/acl.h>
    #include <utils/array.h>
    #include <utils/builtins.h>    // needed for format_procedure()
    #include <utils/datum.h>
    #include <utils/lsyscache.h>   // for type lookup, e.g., type_is_rowtype
    #include <utils/memutils.h>
    #include <utils/syscache.h>    // for direct access to catalog, e.g., SearchSysCache()
    #include <utils/typcache.h>    // type conversion, e.g., lookup_rowtype_tupdesc
    #include "../../../../methods/svec/src/pg_gp/sparse_vector.h" // Legacy sparse vectors
} // extern "C"

#include "Compatibility.hpp"

// FIXME: For now we make use of TR1 but not of C++11. Import all TR1 names into std.
namespace std {
    using boost::unordered_map;
    using boost::hash;
}
#endif MADLIB_POSTGRES_HEADERS

// Unfortunately, we have to clean up some #defines in PostgreSQL headers. They
// interfere with C++ code.
// From c.h:
#ifdef Abs
#undef Abs
#endif

#ifdef Max
#undef Max
#endif

#ifdef Min
#undef Min
#endif

#ifdef gettext
#undef gettext
#endif

#ifdef dgettext
#undef dgettext
#endif

#ifdef ngettext
#undef ngettext
#endif

#ifdef dngettext
#undef dngettext
#endif


// Note: If errors occur in the following include files, it could indicate that
// new macros have been added to PostgreSQL header files.
#include <boost/type_traits/is_const.hpp>
#include <boost/math/special_functions/fpclassify.hpp>
#include <boost/utility/enable_if.hpp>
#include <limits>
#include <stdexcept>
#include <vector>
#include <fstream>

#define MADLIB_DEFAULT_EXCEPTION std::runtime_error("Internal error")
#define madlib_assert(_cond, _exception) \
    do { \
        if(!(_cond)) \
            throw _exception; \
    } while(false)

#define eigen_assert(x) \
    do { \
        if(!Eigen::internal::copy_bool(x)) \
            throw std::runtime_error(std::string( \
                "Internal error. Eigen assertion failed (" \
                EIGEN_MAKESTRING(x) ") in function ") + __PRETTY_FUNCTION__ + \
                " at " __FILE__ ":" EIGEN_MAKESTRING(__LINE__)); \
    } while(false)

#include <dbal/dbal.hpp>
#include <utils/Reference.hpp>
#include <utils/MallocAllocator.hpp> // for hash map of which type is a tuple

namespace madlib {
    namespace dbconnector {
    
        /**
         * @brief C++ abstraction layer for PostgreSQL
         */
        namespace postgres {
            #include "AbstractionLayer_proto.hpp"
            #include "Allocator_proto.hpp"
            #include "ArrayHandle_proto.hpp"
            #include "AnyType_proto.hpp"
            #include "TransparentHandle_proto.hpp"
            #include "PGException_proto.hpp"
            #include "OutputStreamBuffer_proto.hpp"
            #include "UDF_proto.hpp"
        } // namespace postgres
    } // namespace dbconnector

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
} // namespace madlib

#include <dbal/EigenLinAlgTypes/EigenLinAlgTypes.hpp>

namespace madlib {
    typedef dbal::EigenTypes<Eigen::Unaligned> DefaultLinAlgTypes;

    namespace dbconnector {
        namespace postgres {
            #include "AbstractionLayer_impl.hpp"
            #include "TypeTraits.hpp"
            #include "Allocator_impl.hpp"
            #include "AnyType_impl.hpp"
            #include "ArrayHandle_impl.hpp"
            #include "TransparentHandle_impl.hpp"
            #include "OutputStreamBuffer_impl.hpp"
            #include "UDF_impl.hpp"
        } // namespace postgres
    } // namespace dbconnector
} // namespace madlib


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








#endif // defined(MADLIB_POSTGRES_DBCONNECTOR_HPP)
