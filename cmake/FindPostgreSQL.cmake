# - Find PostgreSQL
# Find the PostgreSQL include directory and binary
#
# == Using PostgreSQL
#  find_package(PostgreSQL REQUIRED)
#
# This module sets the following variables:
#  POSTGRESQL_FOUND - set to true if headers and binary were found
#  POSTGRESQL_LIB_DIR - PostgreSQL library directory
#  POSTGRESQL_CLIENT_INCLUDE_DIR - client include directory
#  POSTGRESQL_SERVER_INCLUDE_DIR - server include directory
#  POSTGRESQL_EXECUTABLE - path to postgres binary
#  POSTGRESQL_VERSION_MAJOR - major version number
#  POSTGRESQL_VERSION_MINOR - minor version number
#  POSTGRESQL_VERSION_PATCH - patch version number
#  POSTGRESQL_VERSION_STRING - version number as a string (ex: "9.0.3")
#
# This package tries to find pg_config and uses it to find out other needed
# paths. It is possible to omit the search steps, and define the variable
# POSTGRESQL_PG_CONFIG instead.
#
# Copyright (c) 2011, Florian Schoppmann, <Florian.Schoppmann@emc.com>
#
# Distributed under the BSD-License.

# According to
# http://www.cmake.org/cmake/help/cmake2.6docs.html#variable:CMAKE_VERSION
# variable CMAKE_VERSION is only defined starting 2.6.3. And doing simple versin
# checks is the least we require...
cmake_minimum_required(VERSION 2.6.3)

if(NOT POSTGRESQL_PG_CONFIG)
    find_program(POSTGRESQL_PG_CONFIG pg_config)
endif(NOT POSTGRESQL_PG_CONFIG)

if(POSTGRESQL_PG_CONFIG)
    execute_process(COMMAND ${POSTGRESQL_PG_CONFIG} --includedir
        OUTPUT_VARIABLE POSTGRESQL_CLIENT_INCLUDE_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif(POSTGRESQL_PG_CONFIG)

if(POSTGRESQL_PG_CONFIG AND POSTGRESQL_CLIENT_INCLUDE_DIR)
	set(POSTGRESQL_VERSION_MAJOR 0)
	set(POSTGRESQL_VERSION_MINOR 0)
	set(POSTGRESQL_VERSION_PATCH 0)
	
	if(EXISTS "${POSTGRESQL_CLIENT_INCLUDE_DIR}/pg_config.h")
		# Read and parse postgres version header file for version number
		file(READ "${POSTGRESQL_CLIENT_INCLUDE_DIR}/pg_config.h" _POSTGRESQL_HEADER_CONTENTS)
        
        string(REGEX REPLACE ".*#define PACKAGE_NAME \"([^\"]+)\".*" "\\1" POSTGRESQL_PACKAGE_NAME "${_POSTGRESQL_HEADER_CONTENTS}")
		string(REGEX REPLACE ".*#define PG_VERSION \"([0-9.]+)\".*" "\\1" POSTGRESQL_VERSION_STRING "${_POSTGRESQL_HEADER_CONTENTS}")
		string(REGEX REPLACE "([0-9]+).*" "\\1" POSTGRESQL_VERSION_MAJOR "${POSTGRESQL_VERSION_STRING}")
		string(REGEX REPLACE "[0-9]+\\.([0-9]+).*" "\\1" POSTGRESQL_VERSION_MINOR "${POSTGRESQL_VERSION_STRING}")
		string(REGEX REPLACE "[0-9]+\\.[0-9]+\\.([0-9]+).*" "\\1" POSTGRESQL_VERSION_PATCH "${POSTGRESQL_VERSION_STRING}")
	endif(EXISTS "${POSTGRESQL_CLIENT_INCLUDE_DIR}/pg_config.h")

    if(POSTGRESQL_PACKAGE_NAME STREQUAL "PostgreSQL")
        set(POSTGRESQL_FOUND "YES")
        
        execute_process(COMMAND ${POSTGRESQL_PG_CONFIG} --bindir
            OUTPUT_VARIABLE POSTGRESQL_EXECUTABLE
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        set(POSTGRESQL_EXECUTABLE "${POSTGRESQL_EXECUTABLE}/postgres")

        execute_process(COMMAND ${POSTGRESQL_PG_CONFIG} --includedir-server
            OUTPUT_VARIABLE POSTGRESQL_SERVER_INCLUDE_DIR
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        execute_process(COMMAND ${POSTGRESQL_PG_CONFIG} --libdir
            OUTPUT_VARIABLE POSTGRESQL_LIB_DIR
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    else(POSTGRESQL_PACKAGE_NAME STREQUAL "PostgreSQL")
        # Other vendors like Greenplum might ship installation that look very
        # similar to PostgreSQL. They contain pg_config, but we cannot use them
        # here.
        message(STATUS "Found pg_config at \"${POSTGRESQL_PG_CONFIG}\", but it does not point to a PostgreSQL installation.")
        unset(POSTGRESQL_CLIENT_INCLUDE_DIR)
    endif(POSTGRESQL_PACKAGE_NAME STREQUAL "PostgreSQL")
endif(POSTGRESQL_PG_CONFIG AND POSTGRESQL_CLIENT_INCLUDE_DIR)

# find_package_handle_standard_args has VERSION_VAR argument onl since version 2.8.4
if(${CMAKE_VERSION} VERSION_LESS "2.8.4")
    set(VERSION_VAR "dummy")
endif()

# Checks 'RECQUIRED', 'QUIET' and versions.
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PostgreSQL
	REQUIRED_VARS
        POSTGRESQL_CLIENT_INCLUDE_DIR
        POSTGRESQL_SERVER_INCLUDE_DIR
        POSTGRESQL_EXECUTABLE
	VERSION_VAR
        POSTGRESQL_VERSION_STRING
)

if(${CMAKE_VERSION} VERSION_LESS "2.8.4")
    unset(VERSION_VAR)
endif()
