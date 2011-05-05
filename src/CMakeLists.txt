project(MADlib)

# List all methods that need to be compiled from source
# Note: The module names must match the directory names in the module/
# hierarchy.

set(MAD_MODULES
    prob
    regress)


# For each module, list all source files.

set(SRC_prob
	student.cpp
)

set(SRC_regress
	linear.cpp
	logistic.cpp
)


set(BOOST_TAR_VERSION "1.46.1")
set(BOOST_TAR_MD5 341e5d993b19d099bf1a548495ea91ec)

# If Armadillo is not present on the system, this is the version we will download
# automatically. It is also possible to specify the path to the Armadillo tarball:
# -DARMADILLO_TAR_SOURCE=/path/to/armadillo-x.x.x.tar.gz

set(ARMADILLO_TAR_VERSION "1.2.0")
set(ARMADILLO_TAR_MD5 856268f9730a3e55db38cbc1b7f8f2bc)


# ==============================================================================
# From here on, modifications should rarely be necessary.
# In other words: Be careful when you make changes. You have been warned. Don't
# try this at home.
# ==============================================================================

# Since Armadillo is not yet a widely available package on Linux distros, we
# want to build it along with MADlib. For that we rely on
# ExternalProject_Add, which requires CMake 2.8.
# http://www.kitware.com/news/home/browse/252
cmake_minimum_required(VERSION 2.8)

include (ExternalProject)

# Set the directory for tools needed during build time
set(MAD_BUILD_TOOLS ${CMAKE_SOURCE_DIR}/cmake_tools)

set(MAD_THIRD_PARTY ${CMAKE_BINARY_DIR}/third_party)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${MAD_BUILD_TOOLS}/Modules/")

set(EXTERNALPROJECT_LIST_SEPARATOR "~!~")

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(LINUX TRUE)
endif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")

# Get the MADlib version number
execute_process(
	COMMAND ${MAD_BUILD_TOOLS}/extract_version.sh
	OUTPUT_VARIABLE MADLIB_VERSION
)

set(MAD_MODULE_DIR ${CMAKE_SOURCE_DIR}/modules)

# By default, we include all modules
foreach(MAD_MODULE ${MAD_MODULES})
	foreach(MAD_FILE ${SRC_${MAD_MODULE}})
		list(APPEND MAD_SOURCES ${MAD_MODULE_DIR}/${MAD_MODULE}/${MAD_FILE})
	endforeach()
endforeach()

if(LINUX)
    # On Linux, we export "C" all symbols with their SQL name.
    # See ports/linux/declarations.cpp for explanations.
    list(APPEND MAD_SOURCES ports/linux/declarations.cpp)
endif(LINUX)

# Get the architectures in a Mac OS X binary
macro(osx_archs FILENAME OUT_ARCHS)
    execute_process(
        COMMAND /usr/bin/lipo -info ${FILENAME}
        OUTPUT_VARIABLE _LIPO_OUTPUT)
    string(REPLACE "\n" "" _LIPO_OUTPUT ${_LIPO_OUTPUT})
    string(REGEX REPLACE ".*:[ ]*([^ ].*[^ ])[ ]*\$" "\\1" ${OUT_ARCHS} "${_LIPO_OUTPUT}")
    string(REPLACE " " ";" ${OUT_ARCHS} ${${OUT_ARCHS}})
endmacro(osx_archs)

include_directories(${CMAKE_SOURCE_DIR}/..)

find_package(Boost 1.34)
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
else(Boost_FOUND)
    message(STATUS "No sufficiently recent version (>= 1.34) of Boost was found. Will download.")

    string(REPLACE "." "_" _BOOST_TAR_VERSION_UNDERSCORES ${BOOST_TAR_VERSION})
    set(BOOST_TAR "boost_${_BOOST_TAR_VERSION_UNDERSCORES}.tar.gz")
    set(BOOST_URL "http://sourceforge.net/projects/boost/files/boost/${BOOST_TAR_VERSION}/${BOOST_TAR}")

    if(NOT BOOST_TAR_SOURCE)
        find_file(BOOST_TAR_SOURCE ${BOOST_TAR}
            PATHS ${MAD_THIRD_PARTY}/downloads)
    endif(NOT BOOST_TAR_SOURCE)

    if(NOT BOOST_TAR_SOURCE)
        set(BOOST_TAR_SOURCE ${BOOST_URL})
    endif (NOT BOOST_TAR_SOURCE)

    ExternalProject_Add(EP_boost
        PREFIX ${MAD_THIRD_PARTY}
#        LIST_SEPARATOR ${EXTERNALPROJECT_LIST_SEPARATOR}
        DOWNLOAD_DIR ${MAD_THIRD_PARTY}/downloads
        URL ${BOOST_TAR_SOURCE}
        URL_MD5 ${BOOST_TAR_MD5}
        CMAKE_COMMAND /usr/bin/env echo Ignored: cmake
        BUILD_COMMAND /usr/bin/env echo Ignored: make
        INSTALL_COMMAND /usr/bin/env echo Ignored: make
        BINARY_DIR ${MAD_THIRD_PARTY}/src/EP_boost
    )
    list(APPEND ARMADILLO_DEPENDS
        EP_boost)
    list(APPEND ARMADILLO_CMAKE_ARGS
        -DCMAKE_INCLUDE_PATH=${MAD_THIRD_PARTY}/src/EP_boost)
    include_directories(${MAD_THIRD_PARTY}/src/EP_boost)
endif(Boost_FOUND)

# @loader_path is the Mac OS X equivalent to $ORIGIN.
# For now, we want the library in the ${MAD_THIRD_PARTY}/lib directory
# (and not ${MAD_THIRD_PARTY}/lib64), even if building on a 64-bit architecture
# Note: INSTALL_LIB_DIR was previously LIB_INSTALL_DIR in Armadillo. We might
# want to anticipate further changes...
list(APPEND ARMADILLO_CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=../..
    -DCMAKE_INSTALL_NAME_DIR=@rpath
    -DINSTALL_LIB_DIR=lib
)

# On a Mac, find out the architectures in the Accelerate Framework.
# We also need to build the Armadillo framework so that it is an umbrella for
# the Accelerate framework. We only do this to be consistent with other platforms
# where armadillo can be an umbrella to multiple other libraries.
# if(APPLE)
#     find_library(ACCELERATE_FRAMEWORK Accelerate)
#     osx_archs(${ACCELERATE_FRAMEWORK}/Accelerate ACCELERATE_ARCHS)
#     list(APPEND ARMADILLO_CMAKE_ARGS
#         -DCMAKE_OSX_ARCHITECTURES=${ACCELERATE_ARCHS}
#         "-DCMAKE_SHARED_LINKER_FLAGS=-sub_umbrella Accelerate"
#     )
#     
#     message(STATUS "Building Armadillo for architectures: " ${ACCELERATE_ARCHS})
# endif(APPLE)

#find_package(Armadillo)
#if (NOT ARMADILLO_INCLUDE_DIRS)
# If Armadillo is not present on the system, we will download it automatically
# message(STATUS "No Armadillo installation was not found on the system. "
#    "We will therefore retrieve its source tarball.")

set(ARMADILLO_TAR "armadillo-${ARMADILLO_TAR_VERSION}.tar.gz")
set(ARMADILLO_URL "http://downloads.sourceforge.net/project/arma/${ARMADILLO_TAR}")

if(NOT ARMADILLO_TAR_SOURCE)
    find_file(ARMADILLO_TAR_SOURCE ${ARMADILLO_TAR}
        PATHS ${MAD_THIRD_PARTY}/downloads)
endif(NOT ARMADILLO_TAR_SOURCE)

if(NOT ARMADILLO_TAR_SOURCE)
    set(ARMADILLO_TAR_SOURCE ${ARMADILLO_URL})
endif (NOT ARMADILLO_TAR_SOURCE)

#    message(STATUS "Building with ${ARMADILLO_CMAKE_ARGS}")
ExternalProject_Add(EP_armadillo
    DEPENDS ${ARMADILLO_DEPENDS}
    PREFIX ${MAD_THIRD_PARTY}
    LIST_SEPARATOR ${EXTERNALPROJECT_LIST_SEPARATOR}
    DOWNLOAD_DIR ${MAD_THIRD_PARTY}/downloads
    URL ${ARMADILLO_TAR_SOURCE}
    URL_MD5 ${ARMADILLO_TAR_MD5}
    CMAKE_ARGS ${ARMADILLO_CMAKE_ARGS}
    BINARY_DIR ${MAD_THIRD_PARTY}/src/EP_armadillo
)
add_library(armadillo SHARED IMPORTED)

# The Armadillo CMake script uses the default settings for 
# a shared library. We can therefore know what the installed library
# path will be.
set_target_properties(armadillo
    PROPERTIES IMPORTED_LOCATION
    "${MAD_THIRD_PARTY}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}armadillo${CMAKE_SHARED_LIBRARY_SUFFIX}")


set(ARMADILLO_INCLUDE_DIRS ${MAD_THIRD_PARTY}/include)
#endif(NOT ARMADILLO_INCLUDE_DIRS)
include_directories(${ARMADILLO_INCLUDE_DIRS})

message(STATUS ${ARMADILLO_LIBRARY})

# Unfortunately, at this point, we do not have access to config.hpp, yet.
# file(READ "${ARMADILLO_INCLUDE_DIRS}/armadillo_bits/config.hpp" _ARMADILLO_HEADER_CONTENTS)
# set(ARMA_DEFINES ARMA_USE_LAPACK ARMA_USE_BLAS)
# foreach(MACRO ${ARMA_DEFINES})
#     string(REGEX MATCH "^#define ${MACRO}\$" ${MACRO} "${_ARMADILLO_HEADER_CONTENTS}")
# endforeach(MACRO ${ARMA_DEFINES})
# message(STATUS "ARMA_USE_LAPACK: ${ARMA_USE_LAPACK}")

# FIXME: The following should probably only be set for gcc
# set( CMAKE_CXX_FLAGS "-Wall -Wno-div-by-zero" CACHE STRING
#    "Flags used by the C++ compiler during builds."
#    FORCE )


# Create library
add_library(
    madlib
    SHARED
    ${MAD_SOURCES}
)
#set_target_properties(madlib
#    PROPERTIES PREFIX ""
#    OUTPUT_NAME madlib)
add_dependencies(madlib EP_armadillo)

if(APPLE)
    find_library(ACCELERATE_FRAMEWORK Accelerate)
    osx_archs(${ACCELERATE_FRAMEWORK}/Accelerate ACCELERATE_ARCHS)
    message(STATUS "Will build madlib for same architecture as detected in "
        "${ACCELERATE_FRAMEWORK}, which has architectures "
        "${ACCELERATE_ARCHS}")
    set_target_properties(madlib
        PROPERTIES OSX_ARCHITECTURES "${ACCELERATE_ARCHS}")
    
    # On the Mac, the Accelerate framework is already an umbrella for everything
    # we need for Armadillo
    target_link_libraries(madlib "-framework Accelerate")
else(APPLE)
    # On other platforms, we link to the armadillo library
    target_link_libraries(madlib armadillo)
    
    # the RPATH to be used when installing. This is set to "$ORIGIN" because
    # the armadillo library will reside in the same directory as the MADlib
    # library
    set_target_properties(madlib
        PROPERTIES INSTALL_RPATH "\$ORIGIN")
endif(APPLE)


# Add Ports
add_subdirectory(ports)
