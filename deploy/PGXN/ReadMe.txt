Apache MADlib (incubating) Read Me
--------------

MADlib is an open-source library for scalable in-database analytics.
It provides data-parallel implementations of mathematical, statistical
and machine learning methods for structured and unstructured data.

See the project web site located at http://madlib.incubator.apache.org/ for
links to the latest binary and source packages.

For installation and contribution guides, please see the MADlib wiki at
https://cwiki.apache.org/confluence/display/MADLIB.

The latest documentation of MADlib modules can be found at http://madlib.incubator.apache.org/docs
or can be accessed directly from the MADlib installation directory by opening
doc/user/html/index.html.

Changes between MADlib versions are described in the ReleaseNotes.txt file.

MADlib incorporates material from the following third-party components:

Bundled with source code:
- libstemmer "small string processing language"
  http://snowballstem.org/
- m_widen_init "allows compilation with recent versions of gcc with runtime
  dependencies from earlier versions of libstdc++"
  https://github.com/apache/incubator-madlib/blob/master/licenses/third_party/_M_widen_init.txt
- PyYAML 3.10 "is a YAML parser and emitter for Python"
  http://pyyaml.org/wiki/PyYAML
- argparse 1.2.1 "provides an easy, declarative interface for creating command
  line tools"
  http://code.google.com/p/argparse/
- UseLATEX.cmake "CMAKE commands to use the LaTeX compiler"
  https://github.com/kmorel/UseLATEX/blob/master/UseLATEX.cmake

Downloaded at build time:
- Boost 1.61.0 (or newer) "provides peer-reviewed portable C++ source
  libraries"
  http://www.boost.org/
- Eigen 3.2 "is a C++ template library for linear algebra"
  http://eigen.tuxfamily.org/index.php?title=Main_Page
- PyXB 1.2.4 "Python library for XML Schema Bindings"

License information regarding MADlib and included third-party libraries can be
found inside the 'licenses' directory.

-------------------------------------------------------------------------

The following list of functions have been deprecated and will be removed on
upgrading to the next major version:
    - All overloaded functions 'cox_prop_hazards' and 'cox_prop_hazards_regr'.
    - All overloaded functions 'mlogregr'.
    - Overloaded forms of function 'robust_variance_mlogregr' that accept
    individual optimizer parameters (max_iter, optimizer, tolerance). These
    parameters have been replaced with a single optimizer parameter.
    - Overloaded forms of function 'clusterd_variance_mlogregr' that accept
    individual optimizer parameters (max_iter, optimizer, tolerance).  These
    parameters have been replaced with a single optimizer parameter.
    - Overloaded forms of function 'margins_mlogregr' that accept
    individual optimizer parameters (max_iter, optimizer, tolerance).  These
    parameters have been replaced with a single optimizer parameter.
    - All overloaded functions 'margins_logregr'.


Apache MADlib is an effort undergoing incubation at the Apache Software
Foundation (ASF), sponsored by the Apache Incubator PMC.

Incubation is required of all newly accepted projects until a further
review indicates that the infrastructure, communications, and decision
making process have stabilized in a manner consistent with other
successful ASF projects.

While incubation status is not necessarily a reflection of the
completeness or stability of the code, it does indicate that the
project has yet to be fully endorsed by the ASF.
