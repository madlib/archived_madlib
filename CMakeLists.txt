# ------------------------------------------------------------------------------
# MADlib CMake Build Script
# ------------------------------------------------------------------------------

# -- CMake setup ---------------------------------------------------------------

project(MADlib)

# We automatically download and build missing parts. For that we rely on
# ExternalProject_Add, which requires CMake 2.8.
# http://www.kitware.com/news/home/browse/252

cmake_minimum_required(VERSION 2.8)

include(ExternalProject)


# -- Local definitions (filenames, paths, etc.) --------------------------------

# The default MADlib root directory should be "/usr/local/madlib" and not
# "/usr/local" (which is the CMake default)
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr/local/madlib")
endif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

set(MADLIB_VERSION_YML ${CMAKE_CURRENT_SOURCE_DIR}/src/config/Version.yml)

set(MAD_THIRD_PARTY ${CMAKE_BINARY_DIR}/third_party)

# Set the directory for tools needed during build time
set(MAD_BUILD_TOOLS ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(EXTERNALPROJECT_LIST_SEPARATOR "~!~")

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(LINUX TRUE)
endif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
endif(NOT CMAKE_BUILD_TYPE)

set(M4_ARGUMENTS
    # force a `m4_' prefix to all builtins
    "--prefix-builtins"
)


# Read and parse Version.yml file
file(READ "${MADLIB_VERSION_YML}" _MADLIB_VERSION_CONTENTS)
string(REGEX REPLACE "^.*version:[ \t]*([^\n]*)\n.*" "\\1" MADLIB_VERSION_STRING "${_MADLIB_VERSION_CONTENTS}")
string(REGEX REPLACE "([0-9]+).*" "\\1" MADLIB_VERSION_MAJOR "${MADLIB_VERSION_STRING}")
string(REGEX REPLACE "[0-9]+\\.([0-9]+).*" "\\1" MADLIB_VERSION_MINOR "${MADLIB_VERSION_STRING}")
string(REGEX REPLACE "[0-9]+\\.[0-9]+\\.([0-9]+).*" "\\1" MADLIB_VERSION_PATCH "${MADLIB_VERSION_STRING}")


# -- Third-party dependencies: Find m4 -----------------------------------------

find_program(M4_BINARY m4
    /usr/local/bin /usr/bin /bin /opt/local/bin
)
if(NOT M4_BINARY)
    message(FATAL_ERROR "Cannot find the m4 preprocessor.")
endif(NOT M4_BINARY)


# -- Local includes ------------------------------------------------------------

list(APPEND CMAKE_MODULE_PATH
    "${MAD_BUILD_TOOLS}")

include(Utils)
include(LinuxUtils)
include(OSXUtils)


# -- Include all parts ---------------------------------------------------------

# -- Add subdirectories --------------------------------------------------------

add_subdirectory(src)
add_subdirectory(doc)
add_subdirectory(deploy)
