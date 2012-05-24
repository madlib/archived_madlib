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

#include <dbal/dbal.hpp>
#include <utils/Reference.hpp>
#include <utils/Math.hpp>

// Note: If errors occur in the following include files, it could indicate that
// new macros have been added to PostgreSQL header files.
#include <boost/type_traits/is_const.hpp>
#include <boost/math/special_functions/fpclassify.hpp>
#include <boost/utility/enable_if.hpp>
#include <limits>
#include <stdexcept>
#include <vector>
#include <fstream>

#define eigen_assert(x) \
    do { \
        if(!Eigen::internal::copy_bool(x)) \
            throw std::runtime_error(std::string( \
                "Internal error. Eigen assertion failed (" \
                EIGEN_MAKESTRING(x) ") in function ") + __PRETTY_FUNCTION__ + \
                " at " __FILE__ ":" EIGEN_MAKESTRING(__LINE__)); \
    } while(false)

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


#include "Allocator_proto.hpp"
#include "ArrayHandle_proto.hpp"
#include "AnyType_proto.hpp"
#include "FunctionHandle_proto.hpp"
#include "PGException_proto.hpp"
#include "OutputStreamBuffer_proto.hpp"
#include "SystemInformation_proto.hpp"
#include "TransparentHandle_proto.hpp"
#include "UDF_proto.hpp"

// Several backend functions (APIs) need a wrapper, so that they can be called
// safely from a C++ context.
#include "Backend.hpp"

namespace madlib {

// Import MADlib types into madlib namespace
using dbconnector::postgres::Allocator;
using dbconnector::postgres::AnyType;
using dbconnector::postgres::ArrayHandle;
using dbconnector::postgres::FunctionHandle;
using dbconnector::postgres::MutableArrayHandle;
using dbconnector::postgres::MutableTransparentHandle;
using dbconnector::postgres::TransparentHandle;

// Import MADlib functions into madlib namespace
using dbconnector::postgres::defaultAllocator;
using dbconnector::postgres::Null;

} // namespace madlib


// FIXME: This should be further up. Currently dependency on Allocator
#include <dbal/EigenIntegration/EigenIntegration.hpp>

// FIXME: The following is not the right place...
namespace madlib {

namespace dbal {

namespace eigen_integration {

template <class EigenType>
struct DefaultHandle<EigenType, true> {
    typedef dbconnector::postgres::ArrayHandle<typename EigenType::Scalar> type;
};
template <class EigenType>
struct DefaultHandle<EigenType, false> {
    typedef dbconnector::postgres::MutableArrayHandle<typename EigenType::Scalar> type;
};

} // namespace eigen_integration

} // namespace dbal

} // namespace madlib

// FIXME: This is messy, having Sparse vector here
#include "SparseVector_proto.hpp"

#include "TypeTraits.hpp"
#include "Allocator_impl.hpp"
#include "AnyType_impl.hpp"
#include "ArrayHandle_impl.hpp"
#include "FunctionHandle_impl.hpp"
#include "OutputStreamBuffer_impl.hpp"
#include "TransparentHandle_impl.hpp"
#include "UDF_impl.hpp"
#include "SystemInformation_impl.hpp"
#include "SparseVector_impl.hpp"


#define DECLARE_UDF(_module, _name) \
    namespace madlib { \
    namespace modules { \
    namespace _module { \
    struct _name : public dbconnector::postgres::UDF { \
        inline _name(FunctionCallInfo fcinfo) : dbconnector::postgres::UDF(fcinfo) { }  \
        AnyType run(AnyType &args); \
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
