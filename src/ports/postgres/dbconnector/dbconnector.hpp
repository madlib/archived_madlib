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

// A recent change in the postgres port.h file plays some havoc with /usr/include/stdlib.h
// (commit https://github.com/postgres/postgres/commit/a919937f112eb2f548d5f9bd1b3a7298375e6380)
// Since we don't need anything from ports.h we can cheat and say its already been declared.
// Warning: This could cause problems in the future...
#define PG_PORT_H

extern "C" {
    #include <postgres.h>
    #include <pg_config.h>         // Use the macro defined in the header to detect the platform
    #include <funcapi.h>
    #include <catalog/pg_proc.h>
    #include <catalog/pg_type.h>
    #include <executor/executor.h> // For GetAttributeByNum()
    #include <miscadmin.h>         // Memory allocation, e.g., HOLD_INTERRUPTS
    #include <utils/acl.h>
    #include <utils/array.h>
    #include <utils/builtins.h>    // needed for format_procedure()
#if PG_VERSION_NUM >= 100000
    #include <utils/regproc.h>     // needed for format_procedure() - PostgreSQL 10
#endif
    #include <utils/datum.h>
    #include <utils/lsyscache.h>   // for type lookup, e.g., type_is_rowtype
    #include <utils/memutils.h>
    #include <utils/syscache.h>    // for direct access to catalog, e.g., SearchSysCache()
    #include <utils/typcache.h>    // type conversion, e.g., lookup_rowtype_tupdesc
    #include "../../../../methods/svec/src/pg_gp/sparse_vector.h" // Legacy sparse vectors
} // extern "C"

#include "Compatibility.hpp"

#endif // !defined(MADLIB_POSTGRES_HEADERS)

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
#include <boost/mpl/if.hpp>
#include <boost/any.hpp>
#include <boost/type_traits/is_const.hpp>
#include <boost/type_traits/remove_cv.hpp>
#include <boost/math/special_functions/fpclassify.hpp>
#include <boost/utility/enable_if.hpp>
#include <boost/tr1/array.hpp>
#include <boost/tr1/functional.hpp>
#include <boost/tr1/tuple.hpp>
#include <algorithm>
#include <complex>
#include <limits>
#include <stdexcept>
#include <vector>
#include <fstream>

#include <dbal/dbal_proto.hpp>
#include <utils/Reference.hpp>
#include <utils/Math.hpp>

namespace std {
    // Import names from TR1.

    // The following are currently provided by boost.
    using tr1::array;
    using tr1::bind;
    using tr1::function;
    using tr1::get;
    using tr1::make_tuple;
    using tr1::tie;
    using tr1::tuple;
}

#if !defined(NDEBUG) && !defined(EIGEN_NO_DEBUG)
#define eigen_assert(x) \
    do { \
        if(!Eigen::internal::copy_bool(x)) \
            throw std::runtime_error(std::string( \
                "Internal error. Eigen assertion failed (" \
                EIGEN_MAKESTRING(x) ") in function ") + __PRETTY_FUNCTION__ + \
                " at " __FILE__ ":" EIGEN_MAKESTRING(__LINE__)); \
    } while(false)
#endif // !defined(NDEBUG) && !defined(EIGEN_NO_DEBUG)

// We need to make _oldContext volatile because if an exception occurs, the
// register holding its value might have been overwritten (and the variable
// value is read from in the PG_CATCH block). On the other hand, no need to make
// _errorData volatile: If no PG exception occurs, no register will be
// overwritten. If an exception occurs, _errorData will be set before it is read
// again.
#define MADLIB_PG_TRY \
    do { \
        volatile MemoryContext _oldContext \
            = CurrentMemoryContext; \
        ErrorData* _errorData = NULL; \
        PG_TRY();

// CopyErrorData() copies into the current memory context, so we
// need to switch away from the error context first
#define MADLIB_PG_CATCH \
        PG_CATCH(); { \
            MemoryContextSwitchTo(_oldContext); \
            _errorData = CopyErrorData(); \
            FlushErrorState(); \
        } PG_END_TRY(); \
        if (_errorData)

#define MADLIB_PG_END_TRY \
    } while(false)

#define MADLIB_PG_RE_THROW \
    throw PGException(_errorData)

#define MADLIB_PG_ERROR_DATA() \
    _errorData

#define MADLIB_PG_DEFAULT_CATCH_AND_END_TRY \
    MADLIB_PG_CATCH { \
        MADLIB_PG_RE_THROW; \
    } MADLIB_PG_END_TRY

#define MADLIB_WRAP_PG_FUNC(_returntype, _pgfunc, _arglist, _passedlist) \
inline \
_returntype \
madlib ## _ ## _pgfunc _arglist { \
    _returntype _result = static_cast<_returntype>(0); \
    MADLIB_PG_TRY { \
        _result = _pgfunc _passedlist; \
    } MADLIB_PG_DEFAULT_CATCH_AND_END_TRY; \
    return _result; \
}

#define MADLIB_WRAP_VOID_PG_FUNC(_pgfunc, _arglist, _passedlist) \
inline \
void \
madlib ## _ ## _pgfunc _arglist { \
    MADLIB_PG_TRY { \
        _pgfunc _passedlist; \
    } MADLIB_PG_DEFAULT_CATCH_AND_END_TRY; \
}

/**
 * The maximum number of arguments that can be passed to a function via a
 * FunctionHandle.
 * Note that PostgreSQL defines FUNC_MAX_ARGS, which we could use here. However,
 * this can be a fairly large number that might exceed BOOST_PP_LIMIT_REPEAT.
 * In fmgr.h, PostgreSQL provides support functions (FunctionCall*Coll) for only
 * up to 9 arguments.
 */
#define MADLIB_FUNC_MAX_ARGS 9

/**
 * The maximum number of dimensions in an array
 */
#define MADLIB_MAX_ARRAY_DIMS 2


#include "Allocator_proto.hpp"
#include "ArrayHandle_proto.hpp"
#include "ArrayWithNullException_proto.hpp"
#include "AnyType_proto.hpp"
#include "ByteString_proto.hpp"
#include "NativeRandomNumberGenerator_proto.hpp"
#include "PGException_proto.hpp"
#include "OutputStreamBuffer_proto.hpp"
#include "SystemInformation_proto.hpp"
#include "TransparentHandle_proto.hpp"
#include "TypeTraits_proto.hpp"
#include "UDF_proto.hpp"
// Need to move FunctionHandle down because it has dependencies
#include "FunctionHandle_proto.hpp"

// Several backend functions (APIs) need a wrapper, so that they can be called
// safely from a C++ context.
#include "Backend.hpp"

namespace madlib {

// Import MADlib types into madlib namespace
using dbconnector::postgres::Allocator;
using dbconnector::postgres::AnyType;
using dbconnector::postgres::ArrayHandle;
using dbconnector::postgres::ByteString;
using dbconnector::postgres::FunctionHandle;
using dbconnector::postgres::MutableArrayHandle;
using dbconnector::postgres::MutableByteString;
using dbconnector::postgres::NativeRandomNumberGenerator;
using dbconnector::postgres::TransparentHandle;

// Import MADlib functions into madlib namespace
using dbconnector::postgres::AnyType_cast;
using dbconnector::postgres::defaultAllocator;
using dbconnector::postgres::funcPtr;
using dbconnector::postgres::Null;

// Import MADlib exceptions into madlib namespace
using dbconnector::postgres::ArrayWithNullException;

namespace dbconnector {

namespace postgres {

#ifndef NDEBUG
extern std::ostream dbout;
extern std::ostream dberr;
#endif
} // namespace postgres

} // namespace dbconnector

#ifndef NDEBUG
// Import MADlib global variables into madlib namespace
using dbconnector::postgres::dbout;
using dbconnector::postgres::dberr;
#endif

// A wrapper function added as a workaround to fix MADLIB-769
// To support grouping, we cannot throw an exception because
// it will exit the process. Instead, we print a message and
// set an internal status in the state variable in the C++ code.
inline void warning(std::string msg){
    elog(WARNING, "%s", msg.c_str());
}

} // namespace madlib

#include <dbal/dbal_impl.hpp>

// FIXME: The following include should be further up. Currently dependent on
// dbal_impl.hpp which depends on the memory allocator.
#include "EigenIntegration_proto.hpp"

#include "Allocator_impl.hpp"
#include "AnyType_impl.hpp"
#include "ArrayHandle_impl.hpp"
#include "ByteString_impl.hpp"
#include "EigenIntegration_impl.hpp"
#include "FunctionHandle_impl.hpp"
#include "NativeRandomNumberGenerator_impl.hpp"
#include "OutputStreamBuffer_impl.hpp"
#include "TransparentHandle_impl.hpp"
#include "TypeTraits_impl.hpp"
#include "UDF_impl.hpp"
#include "SystemInformation_impl.hpp"

namespace madlib {

typedef dbal::DynamicStructRootContainer<
    ByteString, dbconnector::postgres::TypeTraits> RootContainer;
typedef dbal::DynamicStructRootContainer<
    MutableByteString, dbconnector::postgres::TypeTraits> MutableRootContainer;

} // namespace madlib

#define DECLARE_UDF(_module, _name) \
    namespace madlib { \
    namespace modules { \
    namespace _module { \
    struct _name : public dbconnector::postgres::UDF { \
        inline _name() { }  \
        AnyType run(AnyType &args); \
        inline void *SRF_init(AnyType&) {return NULL;}; \
        inline AnyType SRF_next(void *, bool *){return AnyType();}; \
    }; \
    } \
    } \
    }

#define DECLARE_SR_UDF(_module, _name) \
    namespace madlib { \
    namespace modules { \
    namespace _module { \
    struct _name : public dbconnector::postgres::UDF { \
        inline _name() { }  \
        inline AnyType run(AnyType &){return AnyType();}; \
        void *SRF_init(AnyType &args); \
        AnyType SRF_next(void *user_fctx, bool *is_last_call); \
    }; \
    } \
    } \
    }

#define DECLARE_UDF_EXTERNAL(_module, _name) \
    namespace external { \
        extern "C" { \
            PG_FUNCTION_INFO_V1(_name); \
            Datum _name(PG_FUNCTION_ARGS) { \
                return madlib::dbconnector::postgres::UDF::call< \
                    madlib::modules::_module::_name>(fcinfo); \
            } \
        } \
    }

#endif // defined(MADLIB_POSTGRES_DBCONNECTOR_HPP)
