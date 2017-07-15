# ======================================================================
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements. See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# 
# http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ======================================================================

# ----------------------------------------------------------------------
# Check for code coverage utilities
# ----------------------------------------------------------------------

find_program(LCOV_PATH lcov
  DOC "Path to the lcov utility."
  )

if(NOT LCOV_PATH)
  message(FATAL_ERROR "lcov not found!")
endif(NOT LCOV_PATH)

find_program(GENHTML_PATH genhtml
  DOC "Path to the genhtml utility."
  )

if(NOT GENHTML_PATH)
  message(FATAL_ERROR "genhtml not found!")
endif(NOT GENHTML_PATH)

# ----------------------------------------------------------------------
# Setup code coverage compilation/link options
# ----------------------------------------------------------------------

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")

set(CMAKE_CXX_OUTPUT_EXTENSION_REPLACE 1)

# ----------------------------------------------------------------------
# Custom code coverage targets
# ----------------------------------------------------------------------

add_custom_target(GenCoverageReport
  # Capture gcov counters and generate report
  COMMAND ${LCOV_PATH} --directory . --capture --output-file CodeCoverage.info
  COMMAND ${LCOV_PATH} --remove CodeCoverage.info '/usr/include/*' '/usr/local/*' '*build/third_party/*' --output-file CodeCoverage-filtered.info
  COMMAND ${GENHTML_PATH} --title="MADlib" --legend --function-coverage --output-directory CodeCoverageReport CodeCoverage-filtered.info
  )

add_custom_target(ResetCoverageCounters
  # Zero gcov counters
  COMMAND ${LCOV_PATH} --directory . --zerocounters
)
