Please read all sections marked with **. All others are optional and provide
background information.


Building and Installing from Source
===================================

** Run-time Requirements (required read):
-----------------------------------------

- None (except the C++ Standard Library)


** Build-time Requirements (required read):
-------------------------------------------

- CMake >= 2.8.4

- Internet connection to automatically download MADlib's dependencies if needed
  (Boost, Eigen). See configuration options below.

Optional:

- For generating user-level documentation (using `make doc`, see below):
  + doxygen >= 1.8.4, flex >= 2.5.33, and bison >= 2.4 to generate the
    user-level API reference in HTML format
  + A recent LaTeX installation for generating the formulas in the user-level
    documentation
  + graphviz >= 2.28 to generate graphs for the doxygen documentation

- For generating developer-level documentation (using `make devdoc`, see below):
  + doxygen, flex, and bison as above
  + git >= 1.7 to download/update the local MathJax installation, which is used
    for displaying formulas in the developer-level documentation

- For generating a complete installation package (RPM, Package Maker, etc.; see
  below):
  + PostgreSQL 8.4, 9.0, 9.1
  + Greenplum 4.0, 4.1, 4.2
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

If you are missing a required library, the `./configure` or `make` step will
notice. Refer to your operating system's manual for instructions to install
the above prerequisites.


Notes:
------

- To speed things up, run `make -j X` instead of `make` where X is the number of
  jobs (commands) to run simultaneously. A good choice is the number of
  processor cores in your machine.
- MADlib depends on the linear-algebra library Eigen. We always copy it into the
  MADlib build directory during the build process.


Building an installation package (RPM, Package Maker, ...)
----------------------------------------------------------

To create a binary installation package, run the following sequence of commands:

    ./configure
    cd build
    make doc
    make package

To create a complete installation package (for all supported DBMSs, equivalent
to what is offered on the MADlib web site), make sure that the build process is
able to locate the DBMS installations. For complete control, run `./configure`
with arguments `-D<DBMS>_PG_CONFIG=/path/to/pg_config` for all `<DBMS>` in
`POSTGRESQL_8_4`, `POSTGRESQL_9_0`, `POSTGRESQL_9_1`, `GREENPLUM_4_0`,
`GREENPLUM_4_1`, and `GREENPLUM_4_2`.


Configuration Options:
----------------------

Depending on the environment, `./configure` might have to be called with
additional configuration parameters. The way to specify a parameter `<PARAM>`
is to add a command-line argument `-D<PARAM>=<value>`.

The following provides an overview of the
most important options. Look at `build/CMakeCache.txt` (relative to the MADlib
root directory) for more options, after having run `cmake` the first time.

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

- `<DBMS>_PG_CONFIG` (for `<DBMS>` in `POSTGRESQL_8_4`, `POSTGRESQL_9_0`,
  `POSTGRESQL_9_1`, `GREENPLUM_4_0`, `GREENPLUM_4_1`, and `GREENPLUM_4_2`,
  default: *empty*)

    Path to `pg_config` of the respective DBMS. If none is set, the build
    script will check if `$(command -v pg_config)` points to a
    PostgreSQL/Greenplum installation.
    
    Note: If no `GREENPLUM<...>_PG_CONFIG` is specified, the build script will
    look for `/usr/local/greenplum-db/bin/pg_config`.

- `LIBSTDCXX_COMPAT` (default: *empty*)

    If GNU gcc is used to build MADlib and link against the GNU libstdc++, this
    option may be used to set the maximum version of libstdc++ acceptable as a
    runtime dependency (not supported on Mac OS X). E.g., if MADlib should
    require no more than the libstdc++ shipped with gcc 4.1.2, call
    `./configure` with `-DLIBSTDCXX_COMPAT=40102`.
    
    The current minimum value supported for option `LIBSTDCXX_COMPAT` is
    `40102`, and the latest version of gcc supported when setting this option is
    gcc 4.6.x.
    
    Setting this option will enable workarounds in
    `src/utils/libstdcxx-compatibility.cpp`.

- `BOOST_TAR_SOURCE` (default: *empty*)

    If no recent version of Boost is found (>= 1.46), Boost is downloaded
    automatically. Alternatively, the path to the (possibly gzip'ed)
    tarball can be specified by calling `./configure` with
    `-DBOOST_TAR_SOURCE=/path/to/boost_x.tar.gz`

- `EIGEN_TAR_SOURCE` (default: *empty*)

    Eigen is downloaded automatically, unless the you call `./configure`
    with `-DEIGEN_TAR_SOURCE=/path/to/eigen_x.tar.gz`, in which case
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
