![](https://github.com/apache/incubator-madlib/blob/master/doc/imgs/magnetic-icon.png) ![](https://github.com/apache/incubator-madlib/blob/master/doc/imgs/agile-icon.png) ![](https://github.com/apache/incubator-madlib/blob/master/doc/imgs/deep-icon.png)
=================================================
**MADlib<sup>&reg;</sup>** is an open-source library for scalable in-database analytics.
It provides data-parallel implementations of mathematical, statistical and
machine learning methods for structured and unstructured data.

Installation and Contribution
==============================
See the project webpage  [`MADlib Home`](http://madlib.net) for links to the
latest binary and source packages. For installation and contribution guides,
please see [`MADlib Wiki`](https://github.com/apache/incubator-madlib/wiki)

User and Developer Documentation
==================================
The latest documentation of MADlib modules can be found at [`MADlib
Docs`](http://doc.madlib.net) or can be accessed directly from the MADlib
installation directory by opening `doc/user/html/index.html`.


Architecture
=============
The following block-diagram gives a high-level overview of MADlib's
architecture.


![MADlib Architecture](https://github.com/apache/incubator-madlib/blob/master/doc/imgs/architecture.png)


Third Party Components
======================
MADlib incorporates material from the following third-party components

1. [`argparse 1.2.1`](http://code.google.com/p/argparse/) "provides an easy, declarative interface for creating command line tools"
2. [`Boost 1.47.0 (or newer)`](http://www.boost.org/) "provides peer-reviewed portable C++ source libraries"
3. [`Eigen 3.2.2`](http://eigen.tuxfamily.org/index.php?title=Main_Page) "is a C++ template library for linear algebra"
4. [`PyYAML 3.10`](http://pyyaml.org/wiki/PyYAML) "is a YAML parser and emitter for Python"
5. [`PyXB 1.2.4`](http://pyxb.sourceforge.net/) "is a Python library for XML Schema Bindings"

Licensing
==========
License information regarding MADlib and included third-party libraries can be
found inside the [`license`](https://github.com/apache/incubator-madlib/blob/master/licenses) directory.

Release Notes
=============
Changes between MADlib versions are described in the
[`ReleaseNotes.txt`](https://github.com/apache/incubator-madlib/blob/master/ReleaseNotes.txt) file.

Papers and Talks
=================
* [`MAD Skills : New Analysis Practices for Big Data (VLDB 2009)`](http://db.cs.berkeley.edu/papers/vldb09-madskills.pdf)
* [`Hybrid In-Database Inference for Declarative Information Extraction (SIGMOD 2011)`](http://www.cise.ufl.edu/~daisyw/sigmod11.pdf)
* [`Towards a Unified Architecture for In-Database Analytics (SIGMOD 2012)`](http://www.cs.stanford.edu/~chrismre/papers/bismarck-full.pdf)
* [`The MADlib Analytics Library or MAD Skills, the SQL (VLDB 2012)`](http://www.eecs.berkeley.edu/Pubs/TechRpts/2012/EECS-2012-38.html)


Related Software
=================
* [`PivotalR`](https://github.com/pivotalsoftware/PivotalR) - PivotalR also
lets the user run the functions of the open-source big-data machine learning
package `MADlib` directly from R.
* [`PyMADlib`](https://github.com/pivotalsoftware/pymadlib) - PyMADlib is a python
wrapper for MADlib, which brings you the power and flexibility of python
with the number crunching power of `MADlib`.
