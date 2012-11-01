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
# Create the VTK Widgets projects for bone manipulation and animation.
#

cmake_minimum_required(VERSION 2.6)

set(KIT Widgets)
project(vtkBender${KIT})
string(TOUPPER ${KIT} KIT_UPPER)

#-----------------------------------------------------------------------------
# Get VTK

find_package(VTK REQUIRED)
include(${VTK_USE_FILE})

#-----------------------------------------------------------------------------
# Create library

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
  ${vtkBenderCommon_SOURCE_DIR}
  )

set(${KIT}_SRCS
  vtkArmatureRepresentation.cxx
  vtkArmatureRepresentation.h
  vtkArmatureWidget.cxx
  vtkArmatureWidget.h
  vtkBoneRepresentation.cxx
  vtkBoneRepresentation.h
  vtkBoneWidget.cxx
  vtkBoneWidget.h
  vtkCylinderBoneRepresentation.cxx
  vtkCylinderBoneRepresentation.h
  vtkDoubleConeBoneRepresentation.cxx
  vtkDoubleConeBoneRepresentation.h
  )

add_library(${PROJECT_NAME} ${LIB_TYPE} ${${KIT}_SRCS})
target_link_libraries(${PROJECT_NAME}
  ${VTK_LIBRARIES}
  #vtkBenderCommon
  )

#-----------------------------------------------------------------------------
# Configure export file

set(MyLibraryExportDirective "VTK_BENDER_${KIT_UPPER}_EXPORT")
set(MyExportHeaderPrefix ${PROJECT_NAME})
set(MyLibName ${PROJECT_NAME})

configure_file(
  ${Bender_SOURCE_DIR}/CMake/BenderExport.h.in
  ${PROJECT_NAME}Export.h
  )

set(dynamicHeaders
  "${dynamicHeaders};${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Export.h")

#-----------------------------------------------------------------------------
# Add testing
enable_testing()

add_subdirectory(Testing)
