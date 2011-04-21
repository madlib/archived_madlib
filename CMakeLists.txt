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

cmake_minimum_required(VERSION 2.8.4)

# Set the directory for tools needed during build time
set(MAD_BUILD_TOOLS ${CMAKE_SOURCE_DIR}/cmake_tools)

set(MAD_THIRD_PARTY ${CMAKE_BINARY_DIR}/third_party)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${MAD_BUILD_TOOLS}/Modules/")


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

# Get the architectures in a Mac OS X binary
macro(osx_archs FILENAME OUT_ARCHS)
    execute_process(
        COMMAND /usr/bin/lipo -info ${FILENAME}
        OUTPUT_VARIABLE _LIPO_OUTPUT)
    string(REPLACE "\n" "" _LIPO_OUTPUT ${_LIPO_OUTPUT})
    string(REGEX REPLACE ".*:[ ]*([^ ].*[^ ])[ ]*$" "\\1" ${OUT_ARCHS} "${_LIPO_OUTPUT}")
    string(REPLACE " " ";" ${OUT_ARCHS} ${${OUT_ARCHS}})
endmacro(osx_archs)

include_directories(${CMAKE_SOURCE_DIR}/..)

find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

find_package(Armadillo)
if (NOT ARMADILLO_INCLUDE_DIRS)
    # If Armadillo is not present on the system, we will download it automatically
    message(STATUS "No Armadillo installation was not found on the system. "
        "We will therefore retrieve its source tarball.")
    
    set(ARMADILLO_TAR "armadillo-${ARMADILLO_TAR_VERSION}.tar.gz")
    set(ARMADILLO_URL "http://downloads.sourceforge.net/project/arma/${ARMADILLO_TAR}")

    if(NOT ARMADILLO_TAR_SOURCE)
        find_file(ARMADILLO_TAR_SOURCE ${ARMADILLO_TAR}
            PATHS ${MAD_THIRD_PARTY}/downloads)
    endif(NOT ARMADILLO_TAR_SOURCE)

    if(NOT ARMADILLO_TAR_SOURCE)
        set(ARMADILLO_TAR_SOURCE ${ARMADILLO_URL})
    endif (NOT ARMADILLO_TAR_SOURCE)

    include (ExternalProject)  
    ExternalProject_Add(armadillo
        PREFIX ${MAD_THIRD_PARTY}
        DOWNLOAD_DIR ${MAD_THIRD_PARTY}/downloads
        URL ${ARMADILLO_TAR_SOURCE}
        URL_MD5 ${ARMADILLO_TAR_MD5}
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=../..
        BINARY_DIR ${MAD_THIRD_PARTY}/src/armadillo
    )

    set(ARMADILLO_INCLUDE_DIRS ${MAD_THIRD_PARTY}/include)
endif(NOT ARMADILLO_INCLUDE_DIRS)
include_directories(${ARMADILLO_INCLUDE_DIRS})

set( CMAKE_CXX_FLAGS "-Wall -Wno-div-by-zero" CACHE STRING
    "Flags used by the C++ compiler during builds."
    FORCE )

# On a Mac, find out which architectures we can build
if(APPLE)
    find_library(ACCELERATE_FRAMEWORK Accelerate)
    message(STATUS ${ACCELERATE_FRAMEWORK})
    osx_archs(${ACCELERATE_FRAMEWORK}/Accelerate MAD_OSX_ARCHS)
#    message(STATUS ${ACCELERATE_ARCHS})
#    list(APPEND ARMADILLO_CMAKE_ARGS
#        -DCMAKE_INSTALL_NAME_DIR=@loader_path
#        -DCMAKE_OSX_ARCHITECTURES=${ACCELERATE_ARCHS}
#    )
endif(APPLE)


# Add Ports
add_subdirectory(ports)
