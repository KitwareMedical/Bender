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
# VTK Bone Widgets Testing
#

create_test_sourcelist(${KIT}_TEST_SRCS
  vtkBoneWidgetTests.cxx
  vtkArmatureWidgetArmTest.cxx
  vtkArmatureWidgetTest.cxx
  vtkBoneWidgetTest_Basics.cxx
  vtkBoneWidgetTest_RestMode.cxx
  vtkBoneWidgetTest_PoseMode.cxx
  vtkBoneWidgetRepresentationAndInteractionTest.cxx
  )

add_executable (${PROJECT_NAME}Tests ${${KIT}_TEST_SRCS})
target_link_libraries(${PROJECT_NAME}Tests
  ${PROJECT_NAME}
  )
set(BenderWidgetTest_BINARY_DIR "${Bender_BINARY_DIR}/${Bender_INSTALL_BIN_DIR}")

add_test(vtkArmatureWidgetTest ${BenderWidgetTest_BINARY_DIR}/BenderWidgetTests vtkArmatureWidgetTest)

add_test(vtkBoneWidgetTest_Basics ${BenderWidgetTest_BINARY_DIR}/BenderWidgetTests vtkBoneWidgetTest_Basics)

add_test(vtkBoneWidgetTest_RestMode ${BenderWidgetTest_BINARY_DIR}/BenderWidgetTests vtkBoneWidgetTest_RestMode)

add_test(vtkBoneWidgetTest_PoseMode ${BenderWidgetTest_BINARY_DIR}/BenderWidgetTests vtkBoneWidgetTest_PoseMode)

add_test(vtkBoneWidgetRepresentationAndIntercationTest ${BenderWidgetTest_BINARY_DIR}/BenderWidgetTests vtkBoneWidgetRepresentationAndIntercationTest)

add_test(vtkArmatureWidgetArmTest ${BenderWidgetTest_BINARY_DIR}/BenderWidgetTests vtkArmatureWidgetArmTest)
