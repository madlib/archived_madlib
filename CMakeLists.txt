# ------------------------------------------------------------------------------
# MADlib CMake Build Script
# ------------------------------------------------------------------------------

# -- Initializations that need to run even before project()

# For Solaris, we want to use the Sun compiler
# See http://www.cmake.org/Bug/view.php?id=8530
set(CMAKE_GENERATOR_CC  suncc gcc)
set(CMAKE_GENERATOR_CXX sunCC g++)

# -- CMake setup ---------------------------------------------------------------

project(MADlib)

# FindPostgreSQL.cmake needs at least 2.8.3. We are on the safe side and require
# the minimum version tested, which is 2.8.4.
cmake_minimum_required(VERSION 2.8.4 FATAL_ERROR)

include(ExternalProject)

# -- Local definitions (filenames, paths, etc.) --------------------------------

# The default MADlib root directory should be "/usr/local/madlib" and not
# "/usr/local" (which is the CMake default)
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr/local/madlib" CACHE PATH
        "Install path prefix, prepended onto install directories." FORCE
    )
endif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

set(MADLIB_VERSION_YML ${CMAKE_CURRENT_SOURCE_DIR}/src/config/Version.yml)

set(MAD_THIRD_PARTY ${CMAKE_BINARY_DIR}/third_party)

# Set the directory for tools needed during build time
set(MAD_BUILD_TOOLS ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(EXTERNALPROJECT_LIST_SEPARATOR "~!~")

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(LINUX TRUE)
endif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")

if(${CMAKE_SYSTEM_NAME} MATCHES "SunOS")
    set(SOLARIS TRUE)
endif(${CMAKE_SYSTEM_NAME} MATCHES "SunOS")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
endif(NOT CMAKE_BUILD_TYPE)

if(CMAKE_COMPILER_IS_GNUCC)
    # Let's store the gcc version in a variable
    execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion
        OUTPUT_VARIABLE GNUCC_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE)

    # A useful summary of warning options can be found here:
    # http://developer.apple.com/tools/xcode/compilercodewarnings.html
    # Note: gcc does not implicitly set _POSIX_C_SOURCE or _XOPEN_SOURCE when using -std=c99.
    # http://pubs.opengroup.org/onlinepubs/9699919799/functions/V2_chap02.html#tag_15_02_01_01
    # We specify that we are POSIX.1-2001 compliant and XSI-conforming. We only
    # need to specify _XOPEN_SOURCE as _POSIX_C_SOURCE will be set implicitly.
    set(CMAKE_C_FLAGS "-std=c99 -pedantic -Wall -Wextra -D_XOPEN_SOURCE=600"
        CACHE STRING
        "Flags used by the compiler during all build types." FORCE)

    # See comments below for C++:
    # Weird enough, the following property is set only for C++ but not for C
    # http://stackoverflow.com/questions/3371127/use-isystem-instead-of-i-with-cmake
    set(CMAKE_INCLUDE_SYSTEM_FLAG_C "-isystem ")
elseif(CMAKE_C_COMPILER_ID STREQUAL "SunPro")
    set(CMAKE_C_FLAGS "-xc99=%all")
endif()

if(CMAKE_COMPILER_IS_GNUCXX)
    # Let's store the gcc version in a variable
    execute_process(COMMAND ${CMAKE_CXX_COMPILER} -dumpversion
        OUTPUT_VARIABLE GNUCXX_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE)

    # We need the 1998 standard plus amendments (ISO/IEC 14882:2003) plus TR1
    # Unfortunately, we only get this with gnu++98
    # Special notes:
    # - long long is not part of the C++ 1998/2003 standard, but it is such a
    # common (and useful) extension that we do not want to hear warnings about
    # it.
    # FIXME: -msse2 should only be added on i386
    set(CMAKE_CXX_FLAGS "-std=gnu++98 -fdiagnostics-show-option -Wall -Wextra -pedantic -Wconversion -Wno-long-long -msse2" CACHE STRING
        "Flags used by the compiler during all build types." FORCE)

    # We want to include some header files as system header files in order to
    # disable warnings. However, on Mac OS X, a CMake variable is not set
    # correctly on Mac OS X. http://www.cmake.org/Bug/view.php?id=10837
    if(APPLE)
        set(CMAKE_INCLUDE_SYSTEM_FLAG_CXX "-isystem ")
    endif(APPLE)
endif(CMAKE_COMPILER_IS_GNUCXX)

set(M4_ARGUMENTS
    # force a `m4_' prefix to all builtins
    "--prefix-builtins"
)


# Read and parse Version.yml file
file(READ "${MADLIB_VERSION_YML}" _MADLIB_VERSION_CONTENTS)
string(REGEX REPLACE "^.*version:[ \t]*([^\n]*)\n.*" "\\1" MADLIB_VERSION_STRING "${_MADLIB_VERSION_CONTENTS}")
string(REGEX REPLACE "([0-9]+).*" "\\1" MADLIB_VERSION_MAJOR "${MADLIB_VERSION_STRING}")
string(REGEX REPLACE "[0-9]+\\.([0-9]+).*" "\\1" MADLIB_VERSION_MINOR "${MADLIB_VERSION_STRING}")
if("${MADLIB_VERSION_STRING}" MATCHES "[0-9]+\\.[0-9]+\\.([0-9]+).*")
    string(REGEX REPLACE "[0-9]+\\.[0-9]+\\.([0-9]+).*" "\\1" MADLIB_VERSION_PATCH "${MADLIB_VERSION_STRING}")
else()
    set(MADLIB_VERSION_PATCH 0)
endif()

# Save build time
execute_process(
    COMMAND date -u
    OUTPUT_VARIABLE MADLIB_BUILD_TIME
    OUTPUT_STRIP_TRAILING_WHITESPACE)
# Save git revision
execute_process(
    COMMAND git describe
    OUTPUT_VARIABLE MADLIB_GIT_REVISION
    ERROR_QUIET
    RESULT_VARIABLE _RESULT
    OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT _RESULT EQUAL 0)
    set(MADLIB_GIT_REVISION "unknown")
endif(NOT _RESULT EQUAL 0)
if(CMAKE_COMPILER_IS_GNUCC)
    set(MADLIB_C_COMPILER "gcc ${GNUCC_VERSION}")
else(CMAKE_COMPILER_IS_GNUCC)
    set(MADLIB_C_COMPILER "unknown")
endif(CMAKE_COMPILER_IS_GNUCC)
if(CMAKE_COMPILER_IS_GNUCXX)
    set(MADLIB_CXX_COMPILER "gcc ${GNUCC_VERSION}")
else(CMAKE_COMPILER_IS_GNUCXX)
    set(MADLIB_CXX_COMPILER "unknown")
endif(CMAKE_COMPILER_IS_GNUCXX)

# -- Third-party dependencies: Find m4 -----------------------------------------

# Unfortunately, we currently rely on GNU m4 due to option --prefix-builtins,
# which is not POSIX-standardized.
if(SOLARIS)
    # Solaris ships GNU m4 as gm4, so we want to use that
    find_program(M4_BINARY gm4
        PATHS /usr/sfw/bin
        DOC "Path to the GNU m4 preprocessor."
    )
else()
    find_program(M4_BINARY m4
        PATHS /usr/local/bin /usr/bin /bin /opt/local/bin
        DOC "Path to the GNU m4 preprocessor."
    )
endif()

if(NOT M4_BINARY)
    message(FATAL_ERROR "Cannot find the m4 preprocessor.")
endif(NOT M4_BINARY)

# -- Install Read-me files and license directory -------------------------------

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/license"
    DESTINATION .
    COMPONENT core
    PATTERN ".DS_Store" EXCLUDE
)
install(
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/ReadMe.txt"
        "${CMAKE_CURRENT_SOURCE_DIR}/ReleaseNotes.txt"
    DESTINATION doc
    COMPONENT core
)

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
