# ------------------------------------------------------------------------------
# Greenplum Port
# ------------------------------------------------------------------------------

set(PORT "Greenplum")
string(TOUPPER ${PORT} PORT_UC)
string(TOLOWER ${PORT} PORT_LC)
set(PORT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
get_filename_component(PORT_DIR_NAME "${PORT_SOURCE_DIR}" NAME)
set(PORT_DEPLOY_SCRIPT "${CMAKE_BINARY_DIR}/deploy/Component_${PORT}.cmake")

if(APPLE)
    # FIXME: This should be handled in a better way. 
    # Greenplum is 32bit on the Mac.
    set(ADDITIONAL_GCC_FLAGS "-m32")
else(APPLE)
    unset(ADDITIONAL_GCC_FLAGS)
endif(APPLE)

list(APPEND CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/../postgres/cmake)
include(PostgreSQLUtils)
include(GreenplumUtils)


# -- 1. Specify files that will be compiled into the shared library, for *all*
#       versions of this port --------------------------------------------------

include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

set(MAD_DBAL_SOURCES
    ${MAD_SOURCES})
list(APPEND MAD_DBAL_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../postgres/dbconnector/Allocator_impl.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../postgres/dbconnector/Allocator_proto.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../postgres/dbconnector/AnyType_impl.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../postgres/dbconnector/AnyType_proto.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../postgres/dbconnector/ArrayHandle_impl.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../postgres/dbconnector/ArrayHandle_proto.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../postgres/dbconnector/Backend.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/dbconnector/Compatibility.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../postgres/dbconnector/dbconnector.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../postgres/dbconnector/EigenIntegration_impl.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../postgres/dbconnector/EigenIntegration_proto.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../postgres/dbconnector/FunctionHandle_impl.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../postgres/dbconnector/FunctionHandle_proto.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../postgres/dbconnector/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../postgres/dbconnector/NewDelete.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../postgres/dbconnector/OutputStreamBuffer_impl.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../postgres/dbconnector/OutputStreamBuffer_proto.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../postgres/dbconnector/PGException_proto.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../postgres/dbconnector/SystemInformation_impl.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../postgres/dbconnector/SystemInformation_proto.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../postgres/dbconnector/TransparentHandle_impl.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../postgres/dbconnector/TransparentHandle_proto.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../postgres/dbconnector/TypeTraits.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../postgres/dbconnector/UDF_impl.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../postgres/dbconnector/UDF_proto.hpp"
)

# FIXME: Convert legacy source code written in C
# BEGIN Legacy Code

    file(GLOB_RECURSE LEGACY_C_FILES
        "${CMAKE_SOURCE_DIR}/methods/*.c")
    list(APPEND MAD_DBAL_SOURCES ${LEGACY_C_FILES})

# END Legacy Code


# -- 2. Copy all SQL files. Since SQL files contain file names, they are only
#       preprocessed at installation time, i.e., by madpack. -------------------

add_sql_files(
    SQL_TARGET_FILES
    "../postgres/modules"
    "${CMAKE_CURRENT_BINARY_DIR}/modules"
)
# Add Greenplum-specific modules. Files will be appended to SQL_TARGET_FILES.
add_sql_files(
    SQL_TARGET_FILES
    "modules"
    "${CMAKE_CURRENT_BINARY_DIR}/modules"
)
# FIXME: Check legacy code for compliance with new architecture
# BEGIN Legacy Code
    
    file(GLOB_RECURSE LEGACY_SQL_FILES RELATIVE
        "${CMAKE_SOURCE_DIR}/methods"
        "${CMAKE_SOURCE_DIR}/methods/*.sql_in")
    foreach(CURRENT_FILE ${LEGACY_SQL_FILES})
        get_filename_component(CURRENT_PATH "${CMAKE_SOURCE_DIR}/methods/${CURRENT_FILE}" ABSOLUTE)
        set(OUTFILE "${CMAKE_CURRENT_BINARY_DIR}/modules/${CURRENT_FILE}")
        string(REPLACE "/src/pg_gp" "" OUTFILE ${OUTFILE})
        string(REPLACE "/sql/" "/test/" OUTFILE ${OUTFILE})
        add_custom_command(OUTPUT "${OUTFILE}"
            COMMAND "${CMAKE_SOURCE_DIR}/cmake/TestIfNoUTF8BOM.py" "${CURRENT_PATH}"
            COMMAND ${CMAKE_COMMAND} -E copy "${CURRENT_PATH}" "${OUTFILE}"
            DEPENDS "${CURRENT_PATH}"
            COMMENT "Validating and copying ${CURRENT_FILE}"
        )
        list(APPEND SQL_TARGET_FILES ${OUTFILE})
    endforeach(CURRENT_FILE ${LEGACY_SQL_FILES})
    
# END Legacy Code
add_custom_target(sqlFiles_${PORT_LC} ALL DEPENDS ${SQL_TARGET_FILES})


# -- 3. Install all SQL files --------------------------------------------------

cpack_add_port_group_and_component_for_all_versions()
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/modules
    DESTINATION ports/${PORT_DIR_NAME}
    COMPONENT ${PORT_LC}_any
    REGEX "^(.*/)?\\.DS_Store\$" EXCLUDE
)


# -- 4. Building the shared library is specific for each version of
#       this port --------------------------------------------------------------

function(add_${PORT_LC}_library IN_PORT_VERSION)
    string(REPLACE "." "_" PORT_VERSION_UNDERSCORE "${IN_PORT_VERSION}")
    set(DBMS "${PORT_LC}_${PORT_VERSION_UNDERSCORE}")
    set(DBMS_UC "${PORT_UC}_${PORT_VERSION_UNDERSCORE}")

# -- 4.1. Compile C/C++ files---------------------------------------------------

    message(">> Adding ${PORT} "
        "${${DBMS_UC}_VERSION_MAJOR}."
        "${${DBMS_UC}_VERSION_MINOR} "
        "(${${DBMS_UC}_ARCHITECTURE}) "
        "to target list...")

    add_executable(${DBMS} IMPORTED)
    set_target_properties(${DBMS} PROPERTIES
        IMPORTED_LOCATION "${${DBMS_UC}_EXECUTABLE}"
    )

    include_directories(SYSTEM
        ${${DBMS_UC}_SERVER_INCLUDE_DIR}
        ${${DBMS_UC}_ADDITIONAL_INCLUDE_DIRS}
    )

    add_madlib_connector_library(madlib_${DBMS}
        lib
        "${${DBMS_UC}_EXECUTABLE}"
        ${MAD_DBAL_SOURCES}
    )
    
    if(CMAKE_COMPILER_IS_GNUCXX)
        # The source code specifies that we are POSIX.1-2001 compliant:
        # On the Mac, this is done in $GP_HOME/include/pg_config_manual.h.
        # On Linux, gcc predefines _GNU_SOURCE, which leads to
        # _POSIX_C_SOURCE being defined in /usr/include/features.h
        # We therefore define HAVE_ERAND48, so that lrand48 is not redeclared
        # in $GP_HOME/include/postgresql/server/port.h.
        set_source_files_properties(${MAD_DBAL_SOURCES} PROPERTIES
            COMPILE_FLAGS "-DHAVE_ERAND48 ${ADDITIONAL_GCC_FLAGS}")
        get_property(_OLD_VALUE TARGET madlib_${DBMS} PROPERTY LINK_FLAGS)
        # If property has not been initialized, _OLD_VALUE is "_OLD_VALUE-NOTFOUND"
        set_target_properties(madlib_${DBMS} PROPERTIES
            LINK_FLAGS "${_OLD_VALUE} ${ADDITIONAL_GCC_FLAGS}")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "SunPro")
        # In syncbitvector.h, an array has zero length. This is non-standard,
        # but most compilers allow it. SunPro needs an extra option.
        set_source_files_properties(${MAD_DBAL_SOURCES} PROPERTIES
            COMPILE_FLAGS "${CMAKE_CXX_FLAGS} -features=zla")
    endif(CMAKE_COMPILER_IS_GNUCXX)

    # FIXME: Convert legacy source code written in C
    # BEGIN Legacy Code
        
        # From the CMake documentation:
        # "Source file properties are visible only to targets added in the same
        # directory (CMakeLists.txt)." We therefore have to set them for each
        # version of this port!
        if(CMAKE_COMPILER_IS_GNUCC)
            # FIXME: Is there a portable (not just for gcc) way of including a header file?
            # Due to Greenplum bug MPP-13254, we need to include <sys/time.h>
            # before <postgres.h>
            # FIXME: In the C code, we have several places where strict aliasing
            # rules are violated. See this web page for some background:
            # http://dbp-consulting.com/tutorials/StrictAliasing.html
            # For now, we tell GCC that it cannot rely on strict aliasing rules.
            # Downside: We forgo some potential optimization.
            # The warning GCC would output without -fno-strict-aliasing is:
            # dereferencing type-punned pointer will break strict-aliasing rules
            set_source_files_properties(${LEGACY_C_FILES} PROPERTIES
                COMPILE_FLAGS "-fno-strict-aliasing -include sys/time.h ${ADDITIONAL_GCC_FLAGS}")
        endif(CMAKE_COMPILER_IS_GNUCC)

        set_source_files_properties(${LEGACY_C_FILES} PROPERTIES
            COMPILE_DEFINITIONS "NO_PG_MODULE_MAGIC"
        )
        
        # sparse_vector.c indirectly includes openssl/ssl.h (via libpq/libpq-be.h)
        include_directories(SYSTEM ${${DBMS_UC}_CLIENT_INCLUDE_DIR})
    
    # END Legacy Code


# -- 4.2. Preprocess all Python files ------------------------------------------
    
    define_greenplum_features(${IN_PORT_VERSION} DBMS_FEATURES)
    define_m4_macros(M4_DEFINES_CMD_LINE M4_DEFINES_CODE ${DBMS_FEATURES})
    
    add_python_files(
        PYTHON_TARGET_FILES
        "${PORT_SOURCE_DIR}/../postgres/modules"
        "${CMAKE_CURRENT_BINARY_DIR}/modules"
        ${M4_DEFINES_CMD_LINE}
    )
    # FIXME: Check legacy code for compliance with new architecture
    # BEGIN Legacy Code
        
        file(GLOB_RECURSE LEGACY_PYTHON_FILES RELATIVE
            "${CMAKE_SOURCE_DIR}/methods"
            "${CMAKE_SOURCE_DIR}/methods/*.py_in")
        foreach(CURRENT_FILE ${LEGACY_PYTHON_FILES})
            get_filename_component(CURRENT_PATH "${CMAKE_SOURCE_DIR}/methods/${CURRENT_FILE}" ABSOLUTE)
            set(OUTFILE "${CMAKE_CURRENT_BINARY_DIR}/modules/${CURRENT_FILE}")
            string(REPLACE "/src/pg_gp" "" OUTFILE ${OUTFILE})
            string(REGEX REPLACE ".py_in\$" ".py" OUTFILE ${OUTFILE})
            get_dir_name(OUTDIR ${OUTFILE})
            add_custom_command(OUTPUT "${OUTFILE}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTDIR}"
                COMMAND ${M4_BINARY} ${M4_ARGUMENTS} ${M4_DEFINES_CMD_LINE}
                    "${CURRENT_PATH}" > "${OUTFILE}"
                DEPENDS "${CURRENT_PATH}"
                COMMENT "Preprocessing ${CURRENT_FILE} with m4."
            )
            list(APPEND PYTHON_TARGET_FILES ${OUTFILE})
        endforeach(CURRENT_FILE ${LEGACY_PYTHON_FILES})
        
    # END Legacy Code
    
    configure_file("${PORT_SOURCE_DIR}/../postgres/madpack/SQLCommon.m4_in"
        "${CMAKE_CURRENT_BINARY_DIR}/madpack/SQLCommon.m4"
        @ONLY
    )
    
    add_custom_target(pythonFiles_${DBMS} ALL
        DEPENDS ${PYTHON_TARGET_FILES})


# -- 4.3. Install shared library, Python files, and M4 header ------------------

    cpack_add_version_component()
    install(TARGETS madlib_${DBMS}
        LIBRARY DESTINATION ports/${PORT_DIR_NAME}/${IN_PORT_VERSION}/lib
        COMPONENT ${DBMS}
    )
    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/modules
        DESTINATION ports/${PORT_DIR_NAME}/${IN_PORT_VERSION}
        COMPONENT ${DBMS}
        REGEX "^(.*/)?\\.DS_Store\$" EXCLUDE
    )
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/madpack/SQLCommon.m4"
        DESTINATION ports/${PORT_DIR_NAME}/${IN_PORT_VERSION}/madpack
        COMPONENT ${DBMS}
    )
endfunction(add_${PORT_LC}_library)


# -- 5. Provide a macro to be called from CMakeLists.txt in the version
#       directory. We want these files to be one-liners. -----------------------

# Add the current version as a target. This is a macro because it calls
# find_package, and we want the side effects (the defined variables) to be
# visible after the invocation
# 
macro(add_current_${PORT_LC}_version)
    get_filename_component(_VERSION "${CMAKE_CURRENT_SOURCE_DIR}" NAME)
    string(REPLACE "." "_" _VERSION_UNDERSCORES ${_VERSION})
    
    find_package(${PORT}_${_VERSION_UNDERSCORES})
    if(${PORT_UC}_${_VERSION_UNDERSCORES}_FOUND)
        add_greenplum_library(${_VERSION})
    endif(${PORT_UC}_${_VERSION_UNDERSCORES}_FOUND)
endmacro(add_current_${PORT_LC}_version)


# -- 6. Build shared library and copy version-specific file for all
#       ${PORT_UC}_X_Y_PG_CONFIG macros defined by the user. If none has been
#       defined, try to find any version this port. ----------------------------

determine_target_versions(VERSIONS)
foreach(VERSION ${VERSIONS})
	add_subdirectory(${VERSION})
endforeach(VERSION)
