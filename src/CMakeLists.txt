# ------------------------------------------------------------------------------
# MADlib CMake Build Script
# ------------------------------------------------------------------------------

# -- Paths and MD5 hashes of third-party downloadable source code (third-party
#    components needed only by specific ports are downloaded there) ------------

# For in-house testing, we might want to change the base URL to something local
# "-DSOURCEFORGE_BASE_URL=http://test.local/projects"
set(SOURCEFORGE_BASE_URL "http://sourceforge.net/projects" CACHE STRING
    "Base URL for Sourceforge projects. May be overridden for testing purposes.")

# Boost might not be present on the system (or simply too old). In this case, we
# download the following version (unless it is already present in
# ${CMAKE_CURRENT_BINARY_DIR}/third_party/downloads).
# It is also possible to specify an alternative path to the Boost tarball when
# running cmake:
# "-DBOOST_TAR_SOURCE=/path/to/boost_x_x_x.tar.gz"

set(BOOST_TAR_VERSION "1.46.1")
set(BOOST_TAR_MD5 341e5d993b19d099bf1a548495ea91ec)

string(REPLACE "." "_" _BOOST_TAR_VERSION_UNDERSCORES ${BOOST_TAR_VERSION})
set(BOOST_TAR "boost_${_BOOST_TAR_VERSION_UNDERSCORES}.tar.gz")
set(BOOST_URL "${SOURCEFORGE_BASE_URL}/boost/files/${BOOST_TAR}")

# We always download Armadillo (unless it is already present in
# ${CMAKE_CURRENT_BINARY_DIR}/third_party/downloads). It is also possible to
# specify an alternative path to the Armadillo tarball:
# -DARMADILLO_TAR_SOURCE=/path/to/armadillo-x.x.x.tar.gz

set(ARMADILLO_TAR_VERSION "1.2.0")
set(ARMADILLO_TAR_MD5 856268f9730a3e55db38cbc1b7f8f2bc)

set(ARMADILLO_TAR "armadillo-${ARMADILLO_TAR_VERSION}.tar.gz")
set(ARMADILLO_URL "${SOURCEFORGE_BASE_URL}/arma/files/${ARMADILLO_TAR}")


# -- Local definitions (filenames, paths, etc.) --------------------------------

set(MAD_PATCH_DIR ${CMAKE_CURRENT_BINARY_DIR}/patch)

set(MAD_MODULE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/modules)


# ==============================================================================
# From here on, modifications should rarely be necessary.
# In other words: Be careful when you make changes. You have been warned. Don't
# try this at home.
# ==============================================================================



# -- Third-party dependencies: Find or download Boost --------------------------

find_package(Boost 1.46)

# We use BOOST_ASSERT_MSG, which only exists in Boost 1.46 and later.
# FIXME: Think whether we really need the latest 1.46 (and BOOST_ASSERT_MSG).
# We need Boost >= 1.34. Unfortunately, the FindBoost module seems to be broken
# with respect to version checking, so we will set Boost_FOUND to FALSE if the
# version is too old.
if(Boost_FOUND)
    if(Boost_VERSION LESS 103400)
        set(Boost_FOUND FALSE)
    endif(Boost_VERSION LESS 103400 )
endif(Boost_FOUND)

if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
else(Boost_FOUND)
    message(STATUS "No sufficiently recent version (>= 1.34) of Boost was found. Will download.")

    if(NOT BOOST_TAR_SOURCE)
        find_file(BOOST_TAR_SOURCE ${BOOST_TAR}
            PATHS ${MAD_THIRD_PARTY}/downloads)
    endif(NOT BOOST_TAR_SOURCE)

    if(NOT BOOST_TAR_SOURCE)
        set(BOOST_TAR_SOURCE ${BOOST_URL})
    endif (NOT BOOST_TAR_SOURCE)

    ExternalProject_Add(EP_boost
        PREFIX ${MAD_THIRD_PARTY}
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
    include_directories(BEFORE ${MAD_THIRD_PARTY}/src/EP_boost)
endif(Boost_FOUND)


# -- Third-party dependencies: Download Armadillo. It provides an umbrella
#    library for LAPACK, BLAS, and ATLAS ---------------------------------------

# For now, we want the library in the ${MAD_THIRD_PARTY}/lib directory
# (and not ${MAD_THIRD_PARTY}/lib64), even if building on a 64-bit architecture
# Note: INSTALL_LIB_DIR was previously LIB_INSTALL_DIR in Armadillo. We might
# want to anticipate further changes...
list(APPEND ARMADILLO_CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=Release
    "-DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}"
    -DINSTALL_LIB_DIR=lib
)

if(NOT ARMADILLO_TAR_SOURCE)
    find_file(ARMADILLO_TAR_SOURCE ${ARMADILLO_TAR}
        PATHS ${MAD_THIRD_PARTY}/downloads)
endif(NOT ARMADILLO_TAR_SOURCE)

if(NOT ARMADILLO_TAR_SOURCE)
    set(ARMADILLO_TAR_SOURCE ${ARMADILLO_URL})
endif (NOT ARMADILLO_TAR_SOURCE)

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
set_target_properties(armadillo PROPERTIES
    IMPORTED_LOCATION
        "${CMAKE_CURRENT_BINARY_DIR}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}armadillo${CMAKE_SHARED_LIBRARY_SUFFIX}"
)

set(ARMADILLO_INCLUDE_DIRS ${CMAKE_CURRENT_BINARY_DIR}/include)
include_directories(${ARMADILLO_INCLUDE_DIRS})


# -- Macros to be used by ports ------------------------------------------------

# Get the architectures in a Mac OS X binary
macro(osx_archs FILENAME OUT_ARCHS)
    execute_process(
        COMMAND /usr/bin/lipo -info ${FILENAME}
        OUTPUT_VARIABLE _LIPO_OUTPUT)
    string(REPLACE "\n" "" _LIPO_OUTPUT ${_LIPO_OUTPUT})
    string(REGEX REPLACE ".*:[ ]*([^ ].*[^ ])[ ]*\$" "\\1" ${OUT_ARCHS} "${_LIPO_OUTPUT}")
    string(REPLACE " " ";" ${OUT_ARCHS} ${${OUT_ARCHS}})
endmacro(osx_archs)

# Add Python files to be preprocessed with m4
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
        COMMAND ${M4_BINARY} ${M4_ARGUMENTS} ${IN_M4_ARGUMENTS}
            "\"\${CURRENT_PATH}\"" > "\"\${OUTFILE}\""
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

# Add sql files to be copied
macro(add_sql_files OUT_SQL_TARGET_FILES IN_SOURCE_DIR IN_TARGET_DIR)
    get_filename_component(SOURCE_DIR_ABS "${IN_SOURCE_DIR}" ABSOLUTE)
    file(GLOB_RECURSE SQL_FILES
        RELATIVE "${SOURCE_DIR_ABS}"
        "${SOURCE_DIR_ABS}/*.sql_in"
    )
    add_files(${OUT_SQL_TARGET_FILES} "${IN_SOURCE_DIR}" "${IN_TARGET_DIR}" ${SQL_FILES})
endmacro(add_sql_files)

# Add a connector library for a specific DBMS port
macro(add_madlib_connector_library IN_DBMS IN_OSX_BUNDLE_LOADER OUT_TARGET_NAME)
    set(IN_LIBRARY_SOURCES ${ARGN})

    set(${OUT_TARGET_NAME} "madlib_${IN_DBMS}")
    add_library(
        ${${OUT_TARGET_NAME}}
        MODULE
        ${IN_LIBRARY_SOURCES}
    )
    add_dependencies(${${OUT_TARGET_NAME}} madlib)
    set_target_properties(${${OUT_TARGET_NAME}} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY lib
        BUILD_WITH_INSTALL_RPATH YES
    )
    
    if(APPLE)
        osx_archs("${IN_OSX_BUNDLE_LOADER}" _MAD_ARCHITECTURES)
        message(STATUS "Will build MADlib ${IN_DBMS} connector for same "
            "architectures as detected in ${IN_OSX_BUNDLE_LOADER}, which has "
            "architectures ${_MAD_ARCHITECTURES}")
        set_target_properties(${${OUT_TARGET_NAME}} PROPERTIES
            OSX_ARCHITECTURES "${_MAD_ARCHITECTURES}"
            LINK_FLAGS "-bundle_loader ${IN_OSX_BUNDLE_LOADER} -Wl,-rpath -Wl,@loader_path/../../../lib")

        # The MADlib connector library for port xyz resides in
        # $MADLIB_ROOT/ports/xyz/lib. The MADlib core library is in
        # $MADLIB_ROOT/lib, hence we set the RPATH to "@loader_path/../../../lib"
        set_target_properties(${${OUT_TARGET_NAME}}
            PROPERTIES INSTALL_RPATH "@loader_path/../../../lib")
    else(APPLE)
        # See comment above. On UNIX/Linux, \$ORIGIN has essentially the same
        # menaing as @loader_path on Mac OS X
        set_target_properties(${${OUT_TARGET_NAME}}
            PROPERTIES INSTALL_RPATH "\$ORIGIN/../../../lib")
    endif(APPLE)
endmacro(add_madlib_connector_library)


# -- Speciy files that will be compiled into MADlib core library ---------------

# FIXME: The CMake description of file(GLOB ...) says:
# "We do not recommend using GLOB to collect a list of source files from your
# source tree. If no CMakeLists.txt file changes when a source is added or
# removed then the generated build system cannot know when to ask CMake to
# regenerate."
# We still use GLOB here because we think the benefits outweigh the mentioned
# disadvantage.

file(GLOB_RECURSE MAD_CPP_SOURCES
    dbal/*.hpp
    modules/*.cpp modules/*.hpp
    utils/*.hpp
)
list(APPEND MAD_SOURCES
    ${MAD_CPP_SOURCES}
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})


# -- Build and install MADlib core library -------------------------------------

# Create library
add_library(
    madlib
    SHARED
    ${MAD_SOURCES}
)
add_dependencies(madlib EP_armadillo)

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
    # On other platforms with dreict binding, we dynamically link to the
    # armadillo library
    if(NOT LINUX)
        # When running on Linux / systems with a flat namespace, there are DBMSs
        # that need to load Armadillo with dlopen, where RTLD_DEEPBIND is ORed
        # in for the second argument (to ensure that all LAPACK/BLAS symbols are
        # resolved properly and do not interference with symbols of the same
        # name in the main executable). Hence, we cannot declare librarmadillo
        # a dependency of the core library.
        target_link_libraries(madlib armadillo)
    endif(NOT LINUX)
    
    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib
        DESTINATION .
        COMPONENT core
        FILES_MATCHING REGEX "^.*/[^/]*armadillo[^/]*\$"
    )
    
    # the RPATH to be used when installing. This is set to "$ORIGIN" because
    # the armadillo library will reside in the same directory as the MADlib
    # library
    set_target_properties(madlib PROPERTIES
        INSTALL_RPATH "\$ORIGIN")
endif(APPLE)

install(TARGETS madlib
    LIBRARY DESTINATION lib
    COMPONENT core
)


# -- On platforms with a flat namespace for symbols in executables, patch
#    Armadillo so that it calls madlib_{LAPACK/BLAS function} instead of
#    {LAPACK/BLAS function}. This is necessary because some DBMSs contain
#    LAPACK/BLAS symbols, which we do not want to use. Instead, the connector
#    libraries will define madlib_{LAPACK/BLAS function} to look up
#    {LAPACK/BLAS function} dynamically with dlsym() ---------------------------

if(LINUX)
    file(MAKE_DIRECTORY "${MAD_PATCH_DIR}")
    add_custom_command(
        OUTPUT
            "${MAD_PATCH_DIR}/armadillo"
            "${MAD_PATCH_DIR}/armadillo_bits/compiler_setup.hpp"
        COMMAND ${CMAKE_COMMAND} -E copy
            "${MAD_THIRD_PARTY}/src/EP_armadillo/include/armadillo"
            "${MAD_PATCH_DIR}/armadillo"
        COMMAND ${CMAKE_COMMAND} -E copy
            "${MAD_THIRD_PARTY}/src/EP_armadillo/include/armadillo_bits/compiler_setup.hpp"
            "${MAD_PATCH_DIR}/armadillo_bits/compiler_setup.hpp"
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/patch/Armadillo.sh"
        DEPENDS
            "${MAD_THIRD_PARTY}/src/EP_armadillo/include/armadillo"
            "${MAD_THIRD_PARTY}/src/EP_armadillo/include/armadillo_bits/compiler_setup.hpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/patch/Armadillo.sh"
        WORKING_DIRECTORY "${MAD_PATCH_DIR}"
        COMMENT "Patching armadillo_bits/compiler_setup.hpp: Route LAPACK/BLAS calls through MADlib connector."
    )
    list(APPEND MADLIB_TARGET_PATCH_FILES
        "${MAD_PATCH_DIR}/armadillo_bits/compiler_setup.hpp"
    )
    add_custom_target(madlibPatches ALL DEPENDS ${MADLIB_TARGET_PATCH_FILES})
    include_directories(BEFORE "${MAD_PATCH_DIR}")

    add_dependencies(madlibPatches EP_armadillo)
    add_dependencies(madlib madlibPatches)
endif(LINUX)

# -- Preprocess/copy all Python/SQL files --------------------------------------

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
    COMPONENT core
    REGEX "^(.*/)?\\.DS_Store\$" EXCLUDE
)


# -- Add subdirectories --------------------------------------------------------

add_subdirectory(bin)
add_subdirectory(config)
add_subdirectory(madpack)
add_subdirectory(ports)
