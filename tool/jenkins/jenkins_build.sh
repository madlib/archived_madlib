#!/bin/bash
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

#####################################################################################
workdir=`pwd`
user_name=`whoami`
reponame=${REPONAME:=madlib}

echo "======================================================================"
echo "Build user: $user_name"
echo "Work directory: $workdir"
echo "Git reponame: $reponame"
echo "----------------------------------------------------------------------"
echo "ls -la"
ls -la
echo "-------------------------------"
echo "rm -rf build"
rm -rf build
echo "-------------------------------"
echo "rm -rf logs"
rm -rf logs
echo "mkdir logs"
mkdir logs
echo "-------------------------------"

echo "docker kill madlib"
docker kill madlib
echo "docker rm madlib"
docker rm madlib

echo "Creating docker container"
# Pull down the base docker images
echo "docker pull madlib/postgres_9.6:jenkins"
docker pull madlib/postgres_9.6:jenkins
# Launch docker container with volume mounted from workdir
echo "-------------------------------"
cat <<EOF
docker run -d --name madlib -v "${workdir}/${reponame}":/madlib madlib/postgres_9.6:jenkins | tee logs/docker_setup.log
EOF
docker run -d --name madlib -v "${workdir}/${reponame}":/madlib madlib/postgres_9.6:jenkins | tee logs/docker_setup.log
echo "-------------------------------"

## This sleep is required since it takes a couple of seconds for the docker
## container to come up, which is required by the docker exec command that follows.
sleep 5

echo "---------- Building package -----------"
# cmake, make, make install, and make package
cat <<EOF
docker exec madlib bash -c 'rm -rf /build; mkdir /build; cd /build; cmake ../madlib; make clean; make; make install; make package' | tee $workdir/logs/madlib_compile.log
EOF
docker exec madlib bash -c 'rm -rf /build; mkdir /build; cd /build; cmake ../madlib; make clean; make; make install; make package' | tee $workdir/logs/madlib_compile.log

echo "---------- Installing and running install-check --------------------"
# Install MADlib and run install check
cat <<EOF
docker exec madlib bash -c '/build/src/bin/madpack -s mad -p postgres -c postgres/postgres@localhost:5432/postgres install' | tee $workdir/logs/madlib_install.log
EOF
docker exec madlib bash -c '/build/src/bin/madpack -s mad -p postgres -c postgres/postgres@localhost:5432/postgres install' | tee $workdir/logs/madlib_install.log

cat <<EOF
docker exec madlib bash -c 'mkdir /tmp'
docker exec madlib bash -c '/build/src/bin/madpack -s mad -p postgres  -c postgres/postgres@localhost:5432/postgres -d /tmp install-check' | tee $workdir/logs/madlib_install_check.log
EOF

docker exec madlib bash -c 'mkdir /tmp'
docker exec madlib bash -c '/build/src/bin/madpack -s mad -p postgres  -c postgres/postgres@localhost:5432/postgres -d /tmp install-check' | tee $workdir/logs/madlib_install_check.log

echo "--------- Copying packages -----------------"
echo "docker cp madlib:build $workdir"
docker cp madlib:build $workdir
echo "docker cp madlib:tmp $workdir"
docker cp madlib:tmp $workdir

echo "-------------------------------"
echo "ls -la"
ls -la
echo "-------------------------------"
echo "ls -la build"
ls -la build/
echo "-------------------------------"

# convert install-check test results to junit format for reporting
cat <<EOF
python ${reponame}/tool/jenkins/junit_export.py $workdir/logs/madlib_install_check.log $workdir/logs/madlib_install_check.xml
EOF
python ${reponame}/tool/jenkins/junit_export.py $workdir $workdir/logs/madlib_install_check.log $workdir/logs/madlib_install_check.xml
