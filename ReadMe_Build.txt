Please read all sections marked with **. All others are optional and provide
background information.


Building and Installing from Source
===================================

** Run-time Requirements (required read):
-----------------------------------------

- LAPACK and BLAS libraries


** Build-time Requirements (required read):
-------------------------------------------

- LAPACK and BLAS libraries as above plus their header files.
  
    Platform notes:
    + Mac OS X comes with LAPACK and BLAS preinstalled (as part of the
      Accelerate framework)
    + On RedHat/CentOS, LAPACK and BLAS come with with the lapack-devel and
      blas-devel packages. When compiling against PostgreSQL, other "\*.-devel"
      packages might be needed (e.g., openssl-devel).
    + On Solaris, LAPACK and BLAS come with the Orcale Performance Studio

- Internet connection to automatically download MADlib's dependencies if needed
  (Boost, Armadillo). See configuration options below.

Optional:

- For generating user-level documentation (using `make doc`, see below):
  + doxygen >= 1.7.3 to generate the user-level API reference in HTML format
  + A recent LaTeX installation for generating the formulas in the user-level
    documentation.
  + graphviz >= 2.28 to generate graphs for the doxygen documentation

- For generating developer-level documentation (using `make devdoc`, see below):
  + doxygen as above
  + git >= 1.7 to download/update the local MathJax installation, which is used
    for displaying formulas in the developer-level documentation

- For generating a complete installation package (RPM, Package Maker, etc.; see
  below):
  + PostgreSQL 9.x
  + Greenplum 4.x
  + All requirements for generating user-level documentation (see above)


** Build instructions (required read):
--------------------------------------

From the MADlib root directory, execute the following commands:

	./configure
	cd build/
	make
    
To build the user-level documentation using doxygen, run:

    make doc

The developer documentation can be built by running `make devdoc` instead.

Optionally, install MADlib with

    make install

If your are missing a required library, the `./configure` or `make` step will
notice. Refer to your operating system's manual for instructions how to install
the above prerequisites.


Notes:
------

- To speed things up, run `make -j X` instead of `make` where X is the number of
  jobs (commands) to run simultaneously. A good choice is the number of
  processor cores in your machine.
- MADlib depends on the linear-algebra library Armadillo. We always build it
  during as part of our own build process. On all platforms except Mac OS X, we
  install the armadillo shared library inside the MADlib directory. (The
  Armadillo shared library is a mere umbrella library for the lower-level maths
  libraries LAPACK, BLAS, ...)


Building an installation package (RPM, Package Maker, ...)
----------------------------------------------------------

To create a binary installation package, run the following sequence of commands:

    ./configure
    cd build
    make doc
    make package

To create a complete installation package (for all supported DBMSs, equivalent
to what is offered on the MADlib web site), make sure that the build process is
able to locate the DBMS installations. For complete control, run `./configure
-DPOSTGRESQL_PG_CONFIG=/path/to/greenplum/pg_config
-DGREENPLUM_PG_CONFIG=/path/to/postgresql/pg_config` instead of just
`./configure`.


Configuration Options:
----------------------

Depending on the environment, `./configure` might have to be called with
additional configuration parameters. The following provides an overview of the
most important options. Look at `build/CMakeCache.txt` (relative to MADlib root
directory) for more options, after having run `cmake` the first time.

- `CMAKE_BUILD_TYPE` (default: `RelWithDebInfo`)

    `Debug`, `Release`, `RelWithDebInfo`, or `MinSizeRel`

- `CMAKE_PREFIX_PATH` (default: *empty*)

    List (separated by `;` without trailing space) of additional search
    paths (for each `${PREFIX_PATH}` in `${CMAKE_PREFIX_PATH}`, binaries are
    searched for in `${PREFIX_PATH}/bin`, headers are searched for in 
    `${PREFIX_PATH}/include`, etc.)
    
    For instance, if Boost header files are located under a non-standard
    location like `/opt/local/include/boost`, run
    `./configure -DCMAKE_PREFIX_PATH=/opt/local`.
    
- `CMAKE_INSTALL_PREFIX` (default: `/usr/local/madlib`)

    Prefix when installing MADlib with `make install`. All files will be
    installed within `${CMAKE_INSTALL_PREFIX}`.

- `POSTGRESQL_X_Y_PG_CONFIG` (for X.Y in {8.4, 9.0, 9.1}, default: *empty*)

    Path to `pg_config` of PostgreSQL version X.Y. If none is set, the build
    script will check if `$(command -v pg_config)` points to a PostgreSQL
    installation.

- `GREENPLUM_PG_CONFIG` (default: `/usr/local/greenplum-db/bin/pg_config` if
   available, otherwise `$(command -v pg_config)`)
    
    Path to `pg_config` of Greenplum installation, which is used to determine
    various Greenplum search paths.

- `BOOST_TAR_SOURCE` (default: *empty*)

    If no recent version of Boost is found (>= 1.46), Boost is downloaded
    automatically. Alternatively, the path to the (possibly gzip'ed)
    tarball can be specified by calling `./configure` with
    `-DBOOST_TAR_SOURCE=/path/to/boost_x.tar.gz`

- `-DARMADILLO_TAR_SOURCE` (default: *empty*)

    Armadillo is downloaded automatically, unless the you call `./configure`
    with `-DARMADILLO_TAR_SOURCE=/path/to/armadillo_x.tar.gz`, in which case
    this tarball is used.


Debugging
=========

For debugging it is helpful to generate an IDE project (e.g., XCode) with cmake
and then connect to the running database process:

0. Generate XCode project with CMake (in MADlib root directory):
   `mkdir -p build/Xcode && cd build/Xcode && cmake -G Xcode ../..`
1. Add an executable in XCode that points to the postgres binary
   (e.g., `/usr/local/bin/postgres`)
2. Do a `select pg_backend_pid();` in psql
3. Choose "Run" -> "Attach to Process" -> "Process ID..." in XCode and enter
   the process ID obtained in psql
