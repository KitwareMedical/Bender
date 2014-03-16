#============================================================================
#
# Program: Bender
#
# Copyright (c) Kitware Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0.txt
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
#============================================================================

#
# Cleaver Testing
#

include(BenderMacroSimpleTest)

set(TEST_NAMES_CXX
    TestLabelMapField.cxx
    )

set(TEST_EXEC_NAME ${PROJECT_NAME}CxxTests)

create_test_sourcelist(TESTS
  ${TEST_EXEC_NAME}.cxx
  ${TEST_NAMES_CXX}
  )

add_executable(${TEST_EXEC_NAME} ${TESTS})
target_link_libraries(${TEST_EXEC_NAME} ${ITK_LIBRARIES} ${Cleaver_LIBRARIES})

set(${PROJECT_NAME}_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Baseline")
SIMPLE_TEST(${TEST_EXEC_NAME} TestLabelMapField)
