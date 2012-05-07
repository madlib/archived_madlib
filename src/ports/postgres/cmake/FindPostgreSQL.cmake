# ------------------------------------------------------------------------------
# Find PostgreSQL binary, include directories, version information, etc.
# ------------------------------------------------------------------------------
#
# == Using PostgreSQL (primary form)
#  find_package(PostgreSQL)
#
# == Alternative form
#  find_package(DBMS_X_Y)
#  where there is a file "FindDBMS_X_Y.cmake" with content
#  --8<--
#  set(_FIND_PACKAGE_FILE "${CMAKE_CURRENT_LIST_FILE}")
#  include("${CMAKE_CURRENT_LIST_DIR}/FindPostgreSQL.cmake")
#  -->8--
#
# This module sets the following variables, where PKG_NAME will be "PostgreSQL"
# when the primary form above is used, and PKG_NAME will be "DBMS_X_Y" if
# the alternative form above is used.
#
#  PKG_NAME - uppercased package name, as decuded from the current file name
#      (see below)
#  ${PKG_NAME}_FOUND - set to true if headers and binary were found
#  ${PKG_NAME}_LIB_DIR - PostgreSQL library directory
#  ${PKG_NAME}_SHARE_DIR - PostgreSQL share directory
#  ${PKG_NAME}_CLIENT_INCLUDE_DIR - client include directory
#  ${PKG_NAME}_SERVER_INCLUDE_DIR - server include directory
#  ${PKG_NAME}_EXECUTABLE - path to postgres binary
#  ${PKG_NAME}_VERSION_MAJOR - major version number
#  ${PKG_NAME}_VERSION_MINOR - minor version number
#  ${PKG_NAME}_VERSION_PATCH - patch version number
#  ${PKG_NAME}_VERSION_STRING - version number as a string (ex: "9.1.2")
#  ${PKG_NAME}_ARCHITECTURE - DBMS architecture (ex: "x86_64")
#
# This package locates pg_config and uses it to determine all other paths. If
# ${PKG_NAME}_PG_CONFIG is defined, then the search steps will be omitted.
#
# Copyright (c) 2011-2012, Florian Schoppmann, <Florian.Schoppmann@emc.com>
#
# Distributed under the BSD-License.

# According to
# http://www.cmake.org/files/v2.8/CMakeChangeLog-2.8.3
# the form of find_package_handle_standard_args we are using requires
# cmake >= 2.8.3
cmake_minimum_required(VERSION 2.8.3 FATAL_ERROR)

# Set defaults that can be overridden by files that include this file:
if(NOT DEFINED _FIND_PACKAGE_FILE)
    set(_FIND_PACKAGE_FILE "${CMAKE_CURRENT_LIST_FILE}")
endif(NOT DEFINED _FIND_PACKAGE_FILE)
if(NOT DEFINED _NEEDED_PG_CONFIG_PACKAGE_NAME)
    set(_NEEDED_PG_CONFIG_PACKAGE_NAME "PostgreSQL")
endif(NOT DEFINED _NEEDED_PG_CONFIG_PACKAGE_NAME)
if(NOT DEFINED _PG_CONFIG_VERSION_NUM_MACRO)
    set(_PG_CONFIG_VERSION_NUM_MACRO "PG_VERSION_NUM")
endif(NOT DEFINED _PG_CONFIG_VERSION_NUM_MACRO)
if(NOT DEFINED _PG_CONFIG_VERSION_MACRO)
    set(_PG_CONFIG_VERSION_MACRO "PG_VERSION")
endif(NOT DEFINED _PG_CONFIG_VERSION_MACRO)

# Set:
# - PACKAGE_FILE_NAME to the package name, as deduced from the current file
#   name, which is of form "FindXXXX.cmake".
# - PKG_NAME to the uppercased ${PACKAGE_FILE_NAME}
# - PACKAGE_FIND_VERSION to the requested version "X.Y", as decuded from
#   ${PACKAGE_FILE_NAME}. It will remain undefined if ${PACKAGE_FILE_NAME} does
#   not have a version suffix of form "_X_Y".
get_filename_component(_CURRENT_FILE_NAME "${_FIND_PACKAGE_FILE}" NAME)
string(REGEX REPLACE "Find([^.]+)\\..*" "\\1" PACKAGE_FIND_NAME
    "${_CURRENT_FILE_NAME}")
string(TOUPPER ${PACKAGE_FIND_NAME} PKG_NAME)
if("${PACKAGE_FIND_NAME}" MATCHES "^.*([0-9]+)_([0-9]+)$")
    string(REGEX REPLACE "^.*([0-9]+)_([0-9]+)$" "\\1.\\2" PACKAGE_FIND_VERSION
        "${PACKAGE_FIND_NAME}")
endif("${PACKAGE_FIND_NAME}" MATCHES "^.*([0-9]+)_([0-9]+)$")

# If ${PKG_NAME}_PG_CONFIG is defined, then the search steps will be omitted.
if(NOT DEFINED ${PKG_NAME}_PG_CONFIG)
    find_program(${PKG_NAME}_PG_CONFIG pg_config
        HINTS ${_SEARCH_PATH_HINTS}
    )
endif(NOT DEFINED ${PKG_NAME}_PG_CONFIG)

if(${PKG_NAME}_PG_CONFIG)
    execute_process(COMMAND ${${PKG_NAME}_PG_CONFIG} --includedir
        OUTPUT_VARIABLE ${PKG_NAME}_CLIENT_INCLUDE_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif(${PKG_NAME}_PG_CONFIG)

if(${PKG_NAME}_PG_CONFIG AND ${PKG_NAME}_CLIENT_INCLUDE_DIR)
    set(${PKG_NAME}_VERSION_MAJOR 0)
    set(${PKG_NAME}_VERSION_MINOR 0)
    set(${PKG_NAME}_VERSION_PATCH 0)
    
    if(EXISTS "${${PKG_NAME}_CLIENT_INCLUDE_DIR}/pg_config.h")
        # Read and parse postgres version header file for version number
        if(${CMAKE_COMPILER_IS_GNUCC})
            # If we know the compiler, we can do something that is a little
            # smarter: Dump the definitions only.
            execute_process(
                COMMAND ${CMAKE_C_COMPILER} -E -dD
                    "${${PKG_NAME}_CLIENT_INCLUDE_DIR}/pg_config.h"
                OUTPUT_VARIABLE _PG_CONFIG_HEADER_CONTENTS
            )
        else(${CMAKE_COMPILER_IS_GNUCC})
            file(READ "${${PKG_NAME}_CLIENT_INCLUDE_DIR}/pg_config.h"
            _PG_CONFIG_HEADER_CONTENTS)
        endif(${CMAKE_COMPILER_IS_GNUCC})
        
        string(REGEX REPLACE ".*#define PACKAGE_NAME \"([^\"]+)\".*" "\\1"
            _PACKAGE_NAME "${_PG_CONFIG_HEADER_CONTENTS}")
            
        string(REGEX REPLACE
            ".*#define ${_PG_CONFIG_VERSION_NUM_MACRO} ([0-9]+).*" "\\1"
            ${PKG_NAME}_VERSION_NUM "${_PG_CONFIG_HEADER_CONTENTS}")
        
        if(${PKG_NAME}_VERSION_NUM MATCHES "^[0-9]+$")
            math(EXPR ${PKG_NAME}_VERSION_MAJOR "${${PKG_NAME}_VERSION_NUM} / 10000")
            math(EXPR ${PKG_NAME}_VERSION_MINOR "(${${PKG_NAME}_VERSION_NUM} % 10000) / 100")
            math(EXPR ${PKG_NAME}_VERSION_PATCH "${${PKG_NAME}_VERSION_NUM} % 100")
            set(${PKG_NAME}_VERSION_STRING "${${PKG_NAME}_VERSION_MAJOR}.${${PKG_NAME}_VERSION_MINOR}.${${PKG_NAME}_VERSION_PATCH}")
        else(${PKG_NAME}_VERSION_NUM MATCHES "^[0-9]+$")
            # Macro with version number was not found. We use the version string
            # macro as a fallback. Example when this is used: Greenplum < 4.1
            # does not have a GP_VERSION_NUM macro, but only GP_VERSION.
            string(REGEX REPLACE
                ".*#define ${_PG_CONFIG_VERSION_MACRO} \"([^\"]+)\".*" "\\1"   
                ${PKG_NAME}_VERSION_STRING "${_PG_CONFIG_HEADER_CONTENTS}")
            string(REGEX REPLACE "([0-9]+).*" "\\1"
                ${PKG_NAME}_VERSION_MAJOR "${${PKG_NAME}_VERSION_STRING}")
            string(REGEX REPLACE "[0-9]+\\.([0-9]+).*" "\\1"
                ${PKG_NAME}_VERSION_MINOR "${${PKG_NAME}_VERSION_STRING}")
            string(REGEX REPLACE "[0-9]+\\.[0-9]+\\.([0-9]+).*" "\\1"
                ${PKG_NAME}_VERSION_PATCH "${${PKG_NAME}_VERSION_STRING}")
        endif(${PKG_NAME}_VERSION_NUM MATCHES "^[0-9]+$")
    endif(EXISTS "${${PKG_NAME}_CLIENT_INCLUDE_DIR}/pg_config.h")

    if(_PACKAGE_NAME STREQUAL "${_NEEDED_PG_CONFIG_PACKAGE_NAME}")
        if((NOT DEFINED PACKAGE_FIND_VERSION) OR
            (PACKAGE_FIND_VERSION VERSION_EQUAL
            "${${PKG_NAME}_VERSION_MAJOR}.${${PKG_NAME}_VERSION_MINOR}"))

            execute_process(COMMAND ${${PKG_NAME}_PG_CONFIG} --bindir
                OUTPUT_VARIABLE ${PKG_NAME}_EXECUTABLE
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            set(${PKG_NAME}_EXECUTABLE "${${PKG_NAME}_EXECUTABLE}/postgres")

            execute_process(COMMAND ${${PKG_NAME}_PG_CONFIG} --includedir-server
                OUTPUT_VARIABLE ${PKG_NAME}_SERVER_INCLUDE_DIR
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )

            execute_process(COMMAND ${${PKG_NAME}_PG_CONFIG} --libdir
                OUTPUT_VARIABLE ${PKG_NAME}_LIB_DIR
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )

            execute_process(COMMAND ${${PKG_NAME}_PG_CONFIG} --sharedir
                OUTPUT_VARIABLE ${PKG_NAME}_SHARE_DIR
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )

            architecture("${${PKG_NAME}_EXECUTABLE}" ${PKG_NAME}_ARCHITECTURE)
        else()
            message(STATUS "Found pg_config at \"${${PKG_NAME}_PG_CONFIG}\", "
                "but it belongs to version ${${PKG_NAME}_VERSION_STRING} "
                "whereas ${PACKAGE_FIND_VERSION} was requested. We will ignore "
                "this installation.")
        endif()
    else(_PACKAGE_NAME STREQUAL "${_NEEDED_PG_CONFIG_PACKAGE_NAME}")
        # There are DBMSs derived from PostgreSQL that also contain pg_config.
        # So there might be many pg_config installed on a system.
        message(STATUS "Found pg_config at "
            "\"${${PKG_NAME}_PG_CONFIG}\", but it does not belong to "
            "${_NEEDED_PG_CONFIG_PACKAGE_NAME}. "
            "We will ignore this installation.")
    endif(_PACKAGE_NAME STREQUAL "${_NEEDED_PG_CONFIG_PACKAGE_NAME}")
endif(${PKG_NAME}_PG_CONFIG AND ${PKG_NAME}_CLIENT_INCLUDE_DIR)

# Checks 'REQUIRED', 'QUIET' and versions. Note that the first parameter is
# passed in original casing.
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(${PACKAGE_FIND_NAME}
    REQUIRED_VARS
        ${PKG_NAME}_EXECUTABLE
        ${PKG_NAME}_CLIENT_INCLUDE_DIR
        ${PKG_NAME}_SERVER_INCLUDE_DIR
    VERSION_VAR
        ${PKG_NAME}_VERSION_STRING
)
