project(MADlib)

# If Boost is not present on the system (or simply too old), this is the version
# we will download automatically. It is also possible to specify the path to the
# Boost tarball:
# -DBOOST_TAR_SOURCE=/path/to/boost_x_x_x.tar.gz

set(BOOST_TAR_VERSION "1.46.1")
set(BOOST_TAR_MD5 341e5d993b19d099bf1a548495ea91ec)

# If Armadillo is not present on the system, this is the version we will
# download automatically. It is also possible to specify the path to the
# Armadillo tarball:
# -DARMADILLO_TAR_SOURCE=/path/to/armadillo-x.x.x.tar.gz

set(ARMADILLO_TAR_VERSION "1.2.0")
set(ARMADILLO_TAR_MD5 856268f9730a3e55db38cbc1b7f8f2bc)

set(MADLIB_VERSION_YML ${CMAKE_SOURCE_DIR}/config/Version.yml)

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

include(ExternalProject)

find_program(M4_BINARY m4
    /usr/local/bin /usr/bin /bin /opt/local/bin
)
if(NOT M4_BINARY)
    message(FATAL_ERROR "Cannot find the m4 preprocessor.")
endif(NOT M4_BINARY)

# Set the directory for tools needed during build time
set(MAD_BUILD_TOOLS ${CMAKE_SOURCE_DIR}/cmake_tools)

set(MAD_THIRD_PARTY ${CMAKE_BINARY_DIR}/third_party)

list(APPEND CMAKE_MODULE_PATH
    "${MAD_BUILD_TOOLS}/Modules/")

include(Utils)
include(OSXUtils)

set(EXTERNALPROJECT_LIST_SEPARATOR "~!~")

IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
ENDIF(NOT CMAKE_BUILD_TYPE)

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(LINUX TRUE)
endif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")


# Support for Python files
macro(add_python_files OUT_PYTHON_TARGET_FILES IN_SOURCE_DIR IN_TARGET_DIR)
    set(IN_M4_ARGUMENTS ${ARGN})
    
    get_filename_component(SOURCE_DIR_ABS "${IN_SOURCE_DIR}" ABSOLUTE)
    get_filename_component(TARGET_DIR_ABS "${IN_TARGET_DIR}" ABSOLUTE)
    file(GLOB_RECURSE PYTHON_FILES
        RELATIVE "${SOURCE_DIR_ABS}"
        "${SOURCE_DIR_ABS}/*.py_in"
    )
    set(MADLIB_PYTHON_M4_PREPROCESSING
        COMMAND ${CMAKE_COMMAND} -E make_directory "\"\${OUTDIR}\""
        COMMAND ${M4_BINARY} ${IN_M4_ARGUMENTS} "\"\${CURRENT_PATH}\"" > "\"\${OUTFILE}\""
    )
    batch_add_command(
        TARGET_PREFIX "${TARGET_DIR_ABS}/"
        SOURCE_PREFIX "${SOURCE_DIR_ABS}/"
        TARGET_SUFFIX ".py"
        SOURCE_SUFFIX "[.]py_in"
        RUN "${MADLIB_PYTHON_M4_PREPROCESSING}"
        COMMENT "Preprocessing \${CURRENT_FILE} with m4."
        TARGET_FILE_LIST_REF ${OUT_PYTHON_TARGET_FILES}
        SOURCE_FILE_LIST ${PYTHON_FILES}
    )
endmacro(add_python_files)

macro(add_sql_files OUT_SQL_TARGET_FILES IN_SOURCE_DIR IN_TARGET_DIR)
    file(GLOB_RECURSE SQL_FILES
        RELATIVE "${SOURCE_DIR_ABS}"
        "${SOURCE_DIR_ABS}/*.sql_in"
    )
    add_files(${OUT_SQL_TARGET_FILES} "${IN_SOURCE_DIR}" "${IN_TARGET_DIR}" ${SQL_FILES})
#    
#    get_filename_component(SOURCE_DIR_ABS "${IN_SOURCE_DIR}" ABSOLUTE)
#    get_filename_component(TARGET_DIR_ABS "${IN_TARGET_DIR}" ABSOLUTE)
#    file(GLOB_RECURSE SQL_FILES
#        RELATIVE "${SOURCE_DIR_ABS}"
#        "${SOURCE_DIR_ABS}/*.sql_in"
#    )
#    set(MADLIB_SQL_COPY_COMMAND
#        COMMAND ${CMAKE_COMMAND} -E copy "\"\${CURRENT_PATH}\"" "\"\${OUTFILE}\""
#    )
#    batch_add_command(
#        TARGET_PREFIX "${TARGET_DIR_ABS}/"
#        SOURCE_PREFIX "${SOURCE_DIR_ABS}/"
#        TARGET_SUFFIX ".sql_in"
#        SOURCE_SUFFIX "[.]sql_in"
#        RUN "${MADLIB_SQL_COPY_COMMAND}"
#        COMMENT "Copying \${CURRENT_FILE}."
#        TARGET_FILE_LIST_REF ${OUT_SQL_TARGET_FILES}
#        SOURCE_FILE_LIST ${SQL_FILES}
#    )
endmacro(add_sql_files)



# Read and parse Version.yml file
file(READ "${MADLIB_VERSION_YML}" _MADLIB_VERSION_CONTENTS)
string(REGEX REPLACE "^.*version:[ \t]*([^\n]*)\n.*" "\\1" MADLIB_VERSION_STRING "${_MADLIB_VERSION_CONTENTS}")
string(REGEX REPLACE "([0-9]+).*" "\\1" MADLIB_VERSION_MAJOR "${MADLIB_VERSION_STRING}")
string(REGEX REPLACE "[0-9]+\\.([0-9]+).*" "\\1" MADLIB_VERSION_MINOR "${MADLIB_VERSION_STRING}")
string(REGEX REPLACE "[0-9]+\\.[0-9]+\\.([0-9]+).*" "\\1" MADLIB_VERSION_PATCH "${MADLIB_VERSION_STRING}")

set(MAD_MODULE_DIR ${CMAKE_SOURCE_DIR}/modules)

# By default, we include all modules
# foreach(MAD_MODULE ${MAD_MODULES})
# 	foreach(MAD_FILE ${SRC_${MAD_MODULE}})
# 		list(APPEND MAD_SOURCES ${MAD_MODULE_DIR}/${MAD_MODULE}/${MAD_FILE})
# 	endforeach()
# endforeach()

# FIXME: The CMake description of file(GLOB ...) says:
# "We do not recommend using GLOB to collect a list of source files from your
# source tree. If no CMakeLists.txt file changes when a source is added or
# removed then the generated build system cannot know when to ask CMake to
# regenerate."
# We still use GLOB here because we think the benefits outweigh the mentioned
# disadvantage.

file(GLOB_RECURSE MAD_CPP_SOURCES modules/*.cpp)
list(APPEND MAD_SOURCES
    ${MAD_CPP_SOURCES}
)

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

include_directories(${CMAKE_SOURCE_DIR})

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
    -DCMAKE_BUILD_TYPE=Release
#    -DCMAKE_INSTALL_PREFIX=../..
#    -DCMAKE_INSTALL_NAME_DIR=@rpath
#    -DINSTALL_LIB_DIR=lib
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
    INSTALL_COMMAND /usr/bin/env echo Ignored: make
    BINARY_DIR ${MAD_THIRD_PARTY}/src/EP_armadillo
)
add_library(armadillo SHARED IMPORTED)

# The Armadillo CMake script uses the default settings for 
# a shared library. We can therefore know what the installed library
# path will be.
set_target_properties(armadillo
    PROPERTIES IMPORTED_LOCATION
    "${MAD_THIRD_PARTY}/src/EP_armadillo/${CMAKE_SHARED_LIBRARY_PREFIX}armadillo${CMAKE_SHARED_LIBRARY_SUFFIX}")


set(ARMADILLO_INCLUDE_DIRS ${MAD_THIRD_PARTY}/src/EP_armadillo/include)
#endif(NOT ARMADILLO_INCLUDE_DIRS)
include_directories(${ARMADILLO_INCLUDE_DIRS})

message(STATUS ${ARMADILLO_LIBRARY})

# Create library
add_library(
    madlib
    SHARED
    ${MAD_SOURCES}
)
# set_target_properties(madlib PROPERTIES
#     OUTPUT_NAME mad)

add_dependencies(madlib EP_armadillo)

install(TARGETS madlib
    LIBRARY DESTINATION lib)

# INSTALL_NAME_DIR is a Mac OS X only property
set_target_properties(madlib PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY lib
    INSTALL_NAME_DIR "@rpath")

if(APPLE)
    find_library(ACCELERATE_FRAMEWORK Accelerate)
    osx_archs(${ACCELERATE_FRAMEWORK}/Accelerate ACCELERATE_ARCHS)
    message(STATUS "Will build madlib for same architecture as detected in "
        "${ACCELERATE_FRAMEWORK}, which has architectures "
        "${ACCELERATE_ARCHS}")
    set_target_properties(madlib PROPERTIES
        OSX_ARCHITECTURES "${ACCELERATE_ARCHS}")
    
    # On the Mac, the Accelerate framework is already an umbrella for everything
    # we need for Armadillo
    target_link_libraries(madlib "-framework Accelerate")
else(APPLE)
    # On other platforms, we link to the armadillo library
    target_link_libraries(madlib armadillo)
    install(TARGETS armadillo
        LIBRARY DESTINATION lib)
    
    # the RPATH to be used when installing. This is set to "$ORIGIN" because
    # the armadillo library will reside in the same directory as the MADlib
    # library
    set_target_properties(madlib PROPERTIES
        INSTALL_RPATH "\$ORIGIN")
endif(APPLE)

add_subdirectory(bin)
add_subdirectory(config)
add_subdirectory(madpack)

add_python_files(
    PYTHON_TARGET_FILES
    "modules"
    "${CMAKE_CURRENT_BINARY_DIR}/modules"
)
add_custom_target(pythonFiles ALL DEPENDS ${PYTHON_TARGET_FILES})

add_sql_files(
    SQL_TARGET_FILES
    "modules"
    "${CMAKE_CURRENT_BINARY_DIR}/modules"
)
add_custom_target(sqlFiles ALL DEPENDS ${SQL_TARGET_FILES})

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/modules
    DESTINATION .
    REGEX "^(.*/)?\\.DS_Store\$" EXCLUDE
)

# Add Ports
add_subdirectory(ports)

#file(READ extra/packageDescription.txt CPACK_RPM_PACKAGE_SUMMARY)

if(APPLE)
    list(APPEND CPACK_GENERATOR 
        PackageMaker
    )
elseif(UNIX)
    list(APPEND CPACK_GENERATOR
        RPM
    )
endif()

set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/extra/packageDescription.txt")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Open-Source Library for Scalable in-Database Analytics")
set(CPACK_PACKAGE_FILE_NAME "madlib-${MADLIB_VERSION_STRING}-${CMAKE_SYSTEM_NAME}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "madlib")
set(CPACK_PACKAGE_NAME "madlib")
set(CPACK_PACKAGE_VENDOR "madlib")
set(CPACK_PACKAGE_VERSION ${MADLIB_VERSION_STRING})
set(CPACK_PACKAGE_VERSION_MAJOR ${MADLIB_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${MADLIB_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${MADLIB_VERSION_PATCH})
set(CPACK_SOURCE_GENERATOR "TGZ;TZ")

# FIXME: Maybe we should not hard code this
set(CPACK_RPM_PACKAGE_ARCHITECTURE x86_64)
set(CPACK_RPM_PACKAGE_LICENSE "New BSD License")
set(CPACK_RPM_PACKAGE_GROUP "Development/Libraries")
set(CPACK_RPM_PACKAGE_REQUIRES "python >= 2.4, lapack >= 3.0, m4 >= 1.4")

set(CPACK_OSX_PACKAGE_VERSION "10.5")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr/local/share/madlib")

include(CPack)
