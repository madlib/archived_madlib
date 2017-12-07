![](doc/imgs/magnetic-icon.png?raw=True) ![](doc/imgs/agile-icon.png?raw=True) ![](doc/imgs/deep-icon.png?raw=True)
=================================================
**MADlib<sup>&reg;</sup>** is an open-source library for scalable in-database analytics.
It provides data-parallel implementations of mathematical, statistical and
machine learning methods for structured and unstructured data.

[![master build status](https://builds.apache.org/buildStatus/icon?job=madlib-master-build&style=plastic)](https://builds.apache.org/job/madlib-master-build)


Installation and Contribution
==============================
See the project website  [`MADlib Home`](http://madlib.apache.org/) for links to the
latest binary and source packages. For installation and contribution guides,
and other useful information
please refer to the [`MADlib Wiki`](https://cwiki.apache.org/confluence/display/MADLIB/)

[`Compiling from source on Linux`](https://cwiki.apache.org/confluence/display/MADLIB/Installation+Guide#InstallationGuide-CompileFromSourceCompilingFromSource) details are 
also on the wiki.


Development with Docker
=======================
We provide a Docker image with necessary dependencies required to compile and test MADlib on PostgreSQL 9.6. You can view the dependency Docker file at ./tool/docker/base/Dockerfile_postgres_9_6. The image is hosted on Docker Hub at madlib/postgres_9.6:latest. Later we will provide a similar Docker image for Greenplum Database.

We provide a script to quickly run this docker image at ./tool/docker_start.sh, which will mount your local madlib directory, build MADlib and run install check on this Docker image. At the end, it will `docker exec` as postgres user. Note that you have to run this script from inside your madlib directory, and you can specify your docker CONTAINER_NAME (default is madlib) and IMAGE_TAG (default is latest). Here is an example:

```
CONTAINER_NAME=my_madlib IMAGE_TAG=LaTex ./tool/docker_start.sh
```
Notice that this script only needs to be run once. After that, you will have a local docker container with CONTAINER_NAME running. To get access to the container, run the following command and you can keep working on it.

```
docker exec -it CONTAINER_NAME bash
```

To kill this docker container, run:

```
docker kill CONTAINER_NAME
docker rm CONTAINER_NAME
```

You can also manually run those commands to do the same thing:

```
## 1) Pull down the `madlib/postgres_9.6:latest` image from docker hub:
docker pull madlib/postgres_9.6:latest

## 2) Launch a container corresponding to the MADlib image, name it
##    madlib, mounting the source code folder to the container:
docker run -d -it --name madlib \
    -v (path to madlib directory):/madlib/ madlib/postgres_9.6
# where madlib is the directory where the MADlib source code resides.

################################# * WARNING * #################################
# Please be aware that when mounting a volume as shown above, any changes you
# make in the "madlib" folder inside the Docker container will be
# reflected on your local disk (and vice versa). This means that deleting data
# in the mounted volume from a Docker container will delete the data from your
# local disk also.
###############################################################################

## 3) When the container is up, connect to it and build MADlib:
docker exec -it madlib bash
mkdir /madlib/build_docker
cd /madlib/build_docker
cmake ..
make
make doc
make install

## 4) Install MADlib:
src/bin/madpack -p postgres -c postgres/postgres@localhost:5432/postgres install

## 5) Several other commands can now be run, such as:
# Run install check, on all modules:
src/bin/madpack -p postgres -c postgres/postgres@localhost:5432/postgres install-check
# Run install check, on a specific module, say svm:
src/bin/madpack -p postgres -c postgres/postgres@localhost:5432/postgres install-check -t svm
# Reinstall MADlib:
src/bin/madpack -p postgres -c postgres/postgres@localhost:5432/postgres reinstall

## 6) Kill and remove containers (after exiting the container):
docker kill madlib
docker rm madlib
```

Instruction for building design pdf on Docker:

For users who wants to build design pdf, make sure you use the `IMAGE_TAG=LaTex` parameter when running the script. After launching your docker container, run the following to get `design.pdf`:

```
cd /madlib/build_docker
make design_pdf
cd doc/design
```

Detailed build instructions are available in [`ReadMe_Build.txt`](ReadMe_Build.txt)

User and Developer Documentation
==================================
The latest documentation of MADlib modules can be found at [`MADlib
Docs`](http://madlib.apache.org/docs/latest/index.html).


Architecture
=============
The following block-diagram gives a high-level overview of MADlib's
architecture.


![MADlib Architecture](doc/imgs/architecture.png?raw=True)


Third Party Components
======================
MADlib incorporates software from the following third-party components.  Bundled with source code:

1. [`libstemmer`](http://snowballstem.org/) "small string processing language"
2. [`m_widen_init`](licenses/third_party/_M_widen_init.txt) "allows compilation with recent versions of gcc with runtime dependencies from earlier versions of libstdc++"
3. [`argparse 1.2.1`](http://code.google.com/p/argparse/) "provides an easy, declarative interface for creating command line tools"
4. [`PyYAML 3.10`](http://pyyaml.org/wiki/PyYAML) "YAML parser and emitter for Python"
5. [`UseLATEX.cmake`](https://github.com/kmorel/UseLATEX/blob/master/UseLATEX.cmake) "CMAKE commands to use the LaTeX compiler"

Downloaded at build time (or supplied as build dependencies):

6. [`Boost 1.61.0 (or newer)`](http://www.boost.org/) "provides peer-reviewed portable C++ source libraries"
7. [`PyXB 1.2.6`](http://pyxb.sourceforge.net/) "Python library for XML Schema Bindings"
8. [`Eigen 3.2.2`](http://eigen.tuxfamily.org/index.php?title=Main_Page) "C++ template library for linear algebra"

Licensing
==========
Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements. See the [`NOTICE`](NOTICE) file distributed with this work for additional information regarding copyright ownership. The ASF licenses this project to You under the Apache License, Version 2.0 (the "License"); you may not use this project except in compliance with the License. You may obtain a copy of the License at [`LICENSE`](LICENSE).

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.

As specified in [`LICENSE`](LICENSE) additional license information regarding included third-party libraries can be
found inside the [`licenses`](licenses) directory.

Release Notes
=============
Changes between MADlib versions are described in the
[`ReleaseNotes.txt`](RELEASE_NOTES) file.

Papers and Talks
=================
* [`MAD Skills : New Analysis Practices for Big Data (VLDB 2009)`](http://db.cs.berkeley.edu/papers/vldb09-madskills.pdf)
* [`Hybrid In-Database Inference for Declarative Information Extraction (SIGMOD 2011)`](https://amplab.cs.berkeley.edu/publication/hybrid-in-database-inference-for-declarative-information-extraction/)
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
