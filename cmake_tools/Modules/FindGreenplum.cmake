# - Find Greenplum
# Find the Greenplum include directory and binary
#
# == Using Greenplum
#  find_package(Greenplum REQUIRED)
#
# This module sets the following variables:
#  GREENPLUM_FOUND - set to true if headers and binary were found
#  GREENPLUM_BASE_DIR - Greenplum base directory
#  GREENPLUM_INCLUDE_DIR - list of include directories
#  GREENPLUM_EXECUTABLE - path to Greenplum's postgres binary
#  GREENPLUM_VERSION_MAJOR - major version number
#  GREENPLUM_VERSION_MINOR - minor version number
#  GREENPLUM_VERSION_PATCH - patch version number
#  GREENPLUM_VERSION_STRING - version number as a string (ex: "4.1.1")
#
# Copyright (c) 2011, Florian Schoppmann, <Florian.Schoppmann@emc.com>
#
# Distributed under the BSD-License.


# Note: HINTS get looked at before PATHS, see find_program documentation
find_path(GREENPLUM_INCLUDE_DIR
	NAMES postgresql/server/postgres.h postgresql/server/fmgr.h
	HINTS 
	/usr/local/greenplum-db/include
)

if(GREENPLUM_INCLUDE_DIR)
    # Unfortunately, the regex engine of CMake is rather primitive.
    # It treats the following the same as "^(.*)/[^/]*$"
    string(REGEX REPLACE "^(.*)/([^/]|\\/)*$" "\\1" GREENPLUM_BASE_DIR ${GREENPLUM_INCLUDE_DIR})
    set(GREENPLUM_EXECUTABLE ${GREENPLUM_BASE_DIR}/bin/postgres)
    
    set(GREENPLUM_INTERNAL_INCLUDE_DIR ${GREENPLUM_INCLUDE_DIR}/postgresql/internal)
    set(GREENPLUM_INCLUDE_DIR ${GREENPLUM_INCLUDE_DIR}/postgresql/server)
endif(GREENPLUM_INCLUDE_DIR)

if(NOT EXISTS ${GREENPLUM_EXECUTABLE})
    unset(GREENPLUM_EXECUTABLE)
endif(NOT EXISTS ${GREENPLUM_EXECUTABLE})
#find_program(GREENPLUM_EXECUTABLE
#	NAMES postgres
#	HINTS
#	/usr/local/greenplum-db
#)

if(GREENPLUM_INCLUDE_DIR)
	set(GREENPLUM_VERSION_MAJOR 0)
	set(GREENPLUM_VERSION_MINOR 0)
	set(GREENPLUM_VERSION_PATCH 0)
	
	if(EXISTS "${GREENPLUM_INCLUDE_DIR}/pg_config.h")
		# Read and parse Greenplum version header file for version number
		file(READ "${GREENPLUM_INCLUDE_DIR}/pg_config.h" _GREENPLUM_HEADER_CONTENTS)
		string(REGEX REPLACE ".*#define GP_VERSION \"([^\"]+)\".*" "\\1" GREENPLUM_VERSION_STRING "${_GREENPLUM_HEADER_CONTENTS}")
		string(REGEX REPLACE "([0-9]+).*" "\\1" GREENPLUM_VERSION_MAJOR "${GREENPLUM_VERSION_STRING}")
		string(REGEX REPLACE "[0-9]+\\.([0-9]+).*" "\\1" GREENPLUM_VERSION_MINOR "${GREENPLUM_VERSION_STRING}")
		string(REGEX REPLACE "[0-9]+\\.[0-9]+\\.([0-9]+).*" "\\1" GREENPLUM_VERSION_PATCH "${GREENPLUM_VERSION_STRING}")
	endif(EXISTS "${GREENPLUM_INCLUDE_DIR}/pg_config.h")
endif(GREENPLUM_INCLUDE_DIR)

# server/funcapi.h ultimately includes server/access/xact.h, from which cdb/cdbpathlocus.h is included
list(APPEND GREENPLUM_INCLUDE_DIR ${GREENPLUM_INTERNAL_INCLUDE_DIR})

# Checks 'RECQUIRED', 'QUIET' and versions.
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Greenplum
	REQUIRED_VARS GREENPLUM_INCLUDE_DIR GREENPLUM_EXECUTABLE
	VERSION_VAR GREENPLUM_VERSION_STRING)
