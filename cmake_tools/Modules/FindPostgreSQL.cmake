# - Find PostgreSQL
# Find the PostgreSQL include directory and binary
#
# == Using PostgreSQL
#  find_package(PostgreSQL REQUIRED)
#
# This module sets the following variables:
#  POSTGRESQL_FOUND - set to true if headers and binary were found
#  POSTGRESQL_BASE_DIR - Greenplum base directory
#  POSTGRESQL_INCLUDE_DIR - list of include directories
#  POSTGRESQL_EXECUTABLE - path to postgres binary
#  POSTGRESQL_VERSION_MAJOR - major version number
#  POSTGRESQL_VERSION_MINOR - minor version number
#  POSTGRESQL_VERSION_PATCH - patch version number
#  POSTGRESQL_VERSION_STRING - version number as a string (ex: "9.0.3")
#
# Copyright (c) 2011, Florian Schoppmann, <Florian.Schoppmann@emc.com>
#
# Distributed under the BSD-License.

# According to
# http://www.cmake.org/cmake/help/cmake2.6docs.html#variable:CMAKE_VERSION
# variable CMAKE_VERSION is only defined starting 2.6.3. And doing simple versin
# checks is the least we require...
cmake_minimum_required(VERSION 2.6.3)

find_path(POSTGRESQL_INCLUDE_DIR
	NAMES server/postgres.h server/fmgr.h
	PATHS 
	"$ENV{ProgramFiles}/PostgreSQL/*/include"
	"$ENV{SystemDrive}/PostgreSQL/*/include"
)

if(POSTGRESQL_INCLUDE_DIR)
    set(POSTGRESQL_INCLUDE_DIR ${POSTGRESQL_INCLUDE_DIR}/server)
endif(POSTGRESQL_INCLUDE_DIR)

find_program(POSTGRESQL_EXECUTABLE
	NAMES postgres
	PATHS
	$ENV{ProgramFiles}/PostgreSQL/*/bin
	$ENV{SystemDrive}/PostgreSQL/*/bin
)

if(POSTGRESQL_INCLUDE_DIR)
    # Unfortunately, the regex engine of CMake is rather primitive.
    # It treats the following the same as "^(.*)/[^/]*$"
    string(REGEX REPLACE "^(.*)/([^/]|\\/)*$" "\\1" POSTGRESQL_BASE_DIR ${POSTGRESQL_INCLUDE_DIR})

    set(POSTGRESQL_FOUND "YES")

	set(POSTGRESQL_VERSION_MAJOR 0)
	set(POSTGRESQL_VERSION_MINOR 0)
	set(POSTGRESQL_VERSION_PATCH 0)
	
	if(EXISTS "${POSTGRESQL_INCLUDE_DIR}/pg_config.h")
		# Read and parse postgres version header file for version number
		file(READ "${POSTGRESQL_INCLUDE_DIR}/pg_config.h" _POSTGRESQL_HEADER_CONTENTS)
		string(REGEX REPLACE ".*#define PG_VERSION \"([0-9.]+)\".*" "\\1" POSTGRESQL_VERSION_STRING "${_POSTGRESQL_HEADER_CONTENTS}")
		string(REGEX REPLACE "([0-9]+).*" "\\1" POSTGRESQL_VERSION_MAJOR "${POSTGRESQL_VERSION_STRING}")
		string(REGEX REPLACE "[0-9]+\\.([0-9]+).*" "\\1" POSTGRESQL_VERSION_MINOR "${POSTGRESQL_VERSION_STRING}")
		string(REGEX REPLACE "[0-9]+\\.[0-9]+\\.([0-9]+).*" "\\1" POSTGRESQL_VERSION_PATCH "${POSTGRESQL_VERSION_STRING}")
	endif(EXISTS "${POSTGRESQL_INCLUDE_DIR}/pg_config.h")
endif(POSTGRESQL_INCLUDE_DIR)

# find_package_handle_standard_args has VERSION_VAR argument onl since version 2.8.4
if(${CMAKE_VERSION} VERSION_LESS "2.8.4")
    set(VERSION_VAR "")
endif()

# Checks 'RECQUIRED', 'QUIET' and versions.
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PostgreSQL
	REQUIRED_VARS POSTGRESQL_INCLUDE_DIR POSTGRESQL_EXECUTABLE
	VERSION_VAR POSTGRESQL_VERSION_STRING)

if(${CMAKE_VERSION} VERSION_LESS "2.8.4")
    unset(VERSION_VAR)
endif()
