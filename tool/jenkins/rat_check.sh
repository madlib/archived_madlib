# ----------------------------------------------------------------------
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
# ----------------------------------------------------------------------
#      This file captures the Apache Jenkins rat check script
# ----------------------------------------------------------------------

set -exu

workdir=`pwd`
reponame=${REPONAME:=madlib}

# Check if NOTICE file year is current
grep "Copyright 2016-$(date +"%Y") The Apache Software Foundation" "${workdir}/${reponame}/NOTICE"

# Check if pom.xml file version is current
# With below grep, it's possible to get a "False Positive" (i.e. no error when it should have)
# but won't give a "False Negative" (i.e. if it fails then there's definitely a problem)
grep "<version>$(cat "${workdir}/${reponame}/src/config/Version.yml" | cut -d" " -f2)</version>" \
    "${workdir}/${reponame}/pom.xml"

set +x

badfile_extentions="class jar tar tgz zip"
badfiles_found=false

for extension in ${badfile_extentions}; do
    echo "Searching for ${extension} files:"
    badfile_count=$(find . -name "${workdir}/${reponame}/*.${extension}" | wc -l)
    if [ ${badfile_count} != 0 ]; then
        echo "----------------------------------------------------------------------"
        echo "FATAL: ${extension} files should not exist"
        echo "For ASF compatibility: the source tree should not contain"
        echo "binary (jar) files as users have a hard time verifying their"
        echo "contents."

        find . -name "${workdir}/${reponame}/*.${extension}"
        echo "----------------------------------------------------------------------"
        badfiles_found=true
    else
        echo "PASSED: No ${extension} files found."
    fi
done

if [ ${badfiles_found} = "true" ]; then
    exit 1
fi

set -x
