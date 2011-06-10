# - Find Greenplum
# Find the Greenplum include directory and binary
#
# == Using Greenplum
#  find_package(Greenplum REQUIRED)
#
# This module sets the following variables:
#  GREENPLUM_FOUND - set to true if headers and binary were found
#  GREENPLUM_LIB_DIR - Greenplum base directory
#  GREENPLUM_CLIENT_INCLUDE_DIR - client include directory
#  GREENPLUM_SERVER_INCLUDE_DIR - server include directory
#  GREENPLUM_ADDITIONAL_INCLUDE_DIRS - additional needed include directories
#  GREENPLUM_EXECUTABLE - path to Greenplum's postgres binary
#  GREENPLUM_VERSION_MAJOR - major version number
#  GREENPLUM_VERSION_MINOR - minor version number
#  GREENPLUM_VERSION_PATCH - patch version number
#  GREENPLUM_VERSION_STRING - version number as a string (ex: "4.1.1")
#
# Copyright (c) 2011, Florian Schoppmann, <Florian.Schoppmann@emc.com>
#
# Distributed under the BSD-License.

# According to
# http://www.cmake.org/cmake/help/cmake2.6docs.html#variable:CMAKE_VERSION
# variable CMAKE_VERSION is only defined starting 2.6.3. And doing simple versin
# checks is the least we require...
cmake_minimum_required(VERSION 2.6.3)

if(NOT GREENPLUM_PG_CONFIG)
    find_program(GREENPLUM_PG_CONFIG pg_config
        HINTS /usr/local/greenplum-db/bin
            "$ENV{GPHOME}/bin"
    )
endif(NOT GREENPLUM_PG_CONFIG)

if(GREENPLUM_PG_CONFIG)
    execute_process(COMMAND ${GREENPLUM_PG_CONFIG} --includedir
        OUTPUT_VARIABLE GREENPLUM_CLIENT_INCLUDE_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif(GREENPLUM_PG_CONFIG)

if(GREENPLUM_PG_CONFIG AND GREENPLUM_CLIENT_INCLUDE_DIR)
	set(GREENPLUM_VERSION_MAJOR 0)
	set(GREENPLUM_VERSION_MINOR 0)
	set(GREENPLUM_VERSION_PATCH 0)
	
	if(EXISTS "${GREENPLUM_CLIENT_INCLUDE_DIR}/pg_config.h")
		# Read and parse Greenplum version header file for version number
		file(READ "${GREENPLUM_CLIENT_INCLUDE_DIR}/pg_config.h" _GREENPLUM_HEADER_CONTENTS)

        string(REGEX REPLACE ".*#define PACKAGE_NAME \"([^\"]+)\".*" "\\1" GREENPLUM_PACKAGE_NAME "${_GREENPLUM_HEADER_CONTENTS}")
		string(REGEX REPLACE ".*#define GP_VERSION \"([^\"]+)\".*" "\\1" GREENPLUM_VERSION_STRING "${_GREENPLUM_HEADER_CONTENTS}")
		string(REGEX REPLACE "([0-9]+).*" "\\1" GREENPLUM_VERSION_MAJOR "${GREENPLUM_VERSION_STRING}")
		string(REGEX REPLACE "[0-9]+\\.([0-9]+).*" "\\1" GREENPLUM_VERSION_MINOR "${GREENPLUM_VERSION_STRING}")
		string(REGEX REPLACE "[0-9]+\\.[0-9]+\\.([0-9]+).*" "\\1" GREENPLUM_VERSION_PATCH "${GREENPLUM_VERSION_STRING}")
	endif(EXISTS "${GREENPLUM_CLIENT_INCLUDE_DIR}/pg_config.h")

    if(GREENPLUM_PACKAGE_NAME STREQUAL "Greenplum Database")
        set(GREENPLUM_FOUND, "YES")

        execute_process(COMMAND ${GREENPLUM_PG_CONFIG} --bindir
            OUTPUT_VARIABLE GREENPLUM_EXECUTABLE
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        set(GREENPLUM_EXECUTABLE "${GREENPLUM_EXECUTABLE}/postgres")

        execute_process(COMMAND ${GREENPLUM_PG_CONFIG} --includedir-server
            OUTPUT_VARIABLE GREENPLUM_SERVER_INCLUDE_DIR
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        execute_process(COMMAND ${GREENPLUM_PG_CONFIG} --libdir
            OUTPUT_VARIABLE GREENPLUM_LIB_DIR
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        
        # server/funcapi.h ultimately includes server/access/xact.h, from which
        # cdb/cdbpathlocus.h is included
        execute_process(COMMAND ${GREENPLUM_PG_CONFIG} --pkgincludedir
            OUTPUT_VARIABLE GREENPLUM_ADDITIONAL_INCLUDE_DIRS
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    else(GREENPLUM_PACKAGE_NAME STREQUAL "Greenplum Database")
        message(STATUS "Found pg_config at \"${GREENPLUM_PG_CONFIG}\", but it does not point to a Greenplum installation.")
        unset(GREENPLUM_CLIENT_INCLUDE_DIR)
    endif(GREENPLUM_PACKAGE_NAME STREQUAL "Greenplum Database")
endif(GREENPLUM_PG_CONFIG AND GREENPLUM_CLIENT_INCLUDE_DIR)

# find_package_handle_standard_args has VERSION_VAR argument onl since version 2.8.4
if(${CMAKE_VERSION} VERSION_LESS "2.8.4")
    set(VERSION_VAR "dummy")
endif()

# Checks 'RECQUIRED', 'QUIET' and versions.
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Greenplum
	REQUIRED_VARS GREENPLUM_CLIENT_INCLUDE_DIR GREENPLUM_SERVER_INCLUDE_DIR
        GREENPLUM_ADDITIONAL_INCLUDE_DIRS
        GREENPLUM_EXECUTABLE
	VERSION_VAR GREENPLUM_VERSION_STRING)

if(${CMAKE_VERSION} VERSION_LESS "2.8.4")
    unset(VERSION_VAR)
endif()
