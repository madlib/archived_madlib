Please read all sections marked with **. All others are optional and provide
background information.


** Madlib C++ Code
==================

Current Ingredients:

1. Database Abstraction Layer (DBAL)
2. Ports
   - PostgreSQL
   - Greenplum


Installation from Source
========================

Requirements:

- LAPACK and BLAS libraries and their header files.
  
  Platform notes:
  + Mac OS X comes with LAPACK and BLAS preinstalled (as part of the Accelerate
    framework)
  + On RedHat/CentOS, LAPACK and BLAS come with with the lapack-devel and
    blas-devel packages. When compiling against PostgreSQL, other "-devel"
    packages might be needed (e.g., openssl-devel).
  + On Solaris, LAPACK and BLAS come with the Orcale Performance Studio

- Internet connection to automatically download MADlib's dependencies if needed
  (Boost, Armadillo). See notes below.
  
Optional:

- doxygen >= 1.7.3 to generate the user-level API reference in HTML format
- git to download/update the local MathJax installation, which is used for
  displaying formulas in the developer-level documentation


** Build instructions (required read):

From the MADlib root directory, execute the following commands:

	./configure
	cd build/
	make
    
To build the user-level documentation using doxygen, run:

    make doc

The developer documentation can be built by running "make devdoc" instead.

Optionally, install MADlib with

    make install

Depending on the environment, ./configure might have to be called with
additional configuration parameters:

    CMAKE_BUILD_TYPE
        "Debug", "Release", "RelWithDebInfo", or "MinSizeRel"

    CMAKE_PREFIX_PATH
        List (separated by ";" without trailing space) of additional search
        paths (for each ${PREFIX_PATH} in ${CMAKE_PREFIX_PATH}, binaries are
        searched for in ${PREFIX_PATH}/bin, headers are searched for in 
        ${PREFIX_PATH}/include, etc.)
    
    CMAKE_INSTALL_PREFIX
        Prefix when installing MADlib with "make install". The default is
        "/usr/local". All files will be installed within
        "${CMAKE_INSTALL_PREFIX}/madlib".

    POSTGRESQL_PG_CONFIG
        Path to pg_config of PostgreSQL installation, which is used to determine
        various PostgreSQL search paths.

    GREENPLUM_PG_CONFIG
        Path to pg_config of Greenplum installation, which is used to determine
        various Greenplum search paths.
    

If your are missing a required library, the ./configure or make step will
notice. Refer to your operating system's manual for instructions how to install
the above prerequisites.

Notes:

- To speed things up, run "make -j X" instead of "make" where X is the number of
  jobs (commands) to run simultaneously. A good choice is the number of
  processor cores in your machine.
- MADlib depends on the C++ Boost header files. If the installed version of
  Boost is too outdated (pre 1.34), it is downloaded automatically.
  Alternatively, the path to the sources can be specified by calling cmake with
  "-DBOOST_TAR_SOURCE=/path/to/boost_x.tar.gz"
- MADlib depends on the linear-algebra library Armadillo. We always build it
  during as part of our own build process. On all platforms except Mac OS X, we
  install the armadillo shared library inside the MADlib directory. (The
  Armadillo shared library is a mere umbrella library for the lower-level maths
  libraries LAPACK, BLAS, ...)
  Armadillo is downloaded automatically, unless the you call cmake with
  "-DARMADILLO_TAR_SOURCE=/path/to/armadillo_x.tar.gz", in which case this file
  is used.



To Do
=====

- C++ Library:
  + For DBMSs other than PG/GP, we need some proper reference counting for
    AbstractHandles. Perhaps wrapping shared_ptr will already work


Debugging
=========

For debugging it is helpful to generate an IDE project (e.g., XCode) with cmake
and then connect to the running database process:

0. Generate XCode project with CMake (in MADlib root directory):
   mkdir XCode && cmake -G Xcode ../src
1. Add an executable in XCode that points to the postgres binary
   (e.g., /usr/local/bin/postgres)
2. Do a "select pg_backend_pid();" in psql
3. Choose "Run" -> "Attach to Process" -> "Process ID..." in XCode and enter
   the process ID obtained in psql
