#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

### This is work in progress, does not work at the moment.
#FROM pivotaldata/gpdb4310:latest
#
#### Get postgres specific add-ons
#RUN yum -y update \
#    && yum -y groupinstall "Development tools" \
#    && yum -y install epel-release      \
#    && yum -y install cmake             \
#                      openssl-devel     \
#                      openssl-libs      \
#                      openssh-server    \
#                      python-devel
#
#
#### Build MADlib
#ADD ./ /madlib
##RUN cd madlib && \
##     mkdir build && \
#	 cd build && \
#	 cmake .. && \
#	 make && \
#	 make install
#
###################################################################################################
################## PLACEHOLDER COMMANDS ##################
#### WARNING: This is under construction, for future reference####################
### Build the image from this docker file:
## docker build -t gpdb -f tool/gpdb/Dockerfile_4_3_10 .
#
#### Steps to use the image for installing MADlib, building changed source code:
### Run the container, mounting the source code's folder to the container. For example:
## 1) docker run -d -it --name gpdb -v (path-to-madlib)/src:/madlib/src gpdb bash
#
### When the container is up, connect to it and execute (Install MADlib):
## 2) docker exec -it gpdb /madlib/build/src/bin/madpack -p greenplum -c gpadmin@127.0.0.1:5432/gpadmin install
#
### Go into the container to build and run commands like install-check for modules:
## 3) docker exec -it gpdb sh
#
### The above command gives us terminal access to the container, run commands such as:
## - cd /madlib/build
## - make (This can be run after changing code in the madlib source code)
## - src/bin/madpack -p postgres  -c postgres/postgres@localhost:5432/postgres install-check -t svm
### Install or reinstall MADlib if required:
## - src/bin/madpack -p postgres  -c postgres/postgres@localhost:5432/postgres install
## - src/bin/madpack -p postgres  -c postgres/postgres@localhost:5432/postgres reinstall
#
#
#### Common docker commands:
### Kill and remove containers:
## - docker kill gpdb
## - docker rm gpdb
#