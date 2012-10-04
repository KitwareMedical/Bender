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
  vtkBoneWidgetRepresentationAndInteractionTest.cxx
  vtkBoneWidgetTwoBonesTest.cxx
  vtkBoneWidgetThreeBonesTest.cxx
  vtkBoneWidgetTwoBonesTestRotationMatrix.cxx
  )

add_executable (${PROJECT_NAME}Tests ${${KIT}_TEST_SRCS})
target_link_libraries(${PROJECT_NAME}Tests
  ${PROJECT_NAME}
  )

add_test(vtkBoneWidgetRepresentationAndIntercationTest ${CXX_TEST_PATH}/BenderWidgetTests vtkBoneWidgetRepresentationAndIntercationTest)

add_test(vtkBoneWidgetTwoBonesTest ${CXX_TEST_PATH}/BenderWidgetTests vtkBoneWidgetTwoBonesTest)

add_test(vtkBoneWidgetThreeBonesTest ${CXX_TEST_PATH}/BenderWidgetTests vtkBoneWidgetThreeBonesTest)

add_test(vtkBoneWidgetTwoBonesTest ${CXX_TEST_PATH}/BenderWidgetTests vtkBoneWidgetTwoBonesTestRotationMatrix)
