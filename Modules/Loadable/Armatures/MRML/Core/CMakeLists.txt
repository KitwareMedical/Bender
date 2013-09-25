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

project(vtkBender${MODULE_NAME}ModuleMRMLCore)

set(KIT ${PROJECT_NAME})

set(${KIT}_EXPORT_DIRECTIVE
    "VTK_BENDER_${MODULE_NAME_UPPER}_MRML_CORE_EXPORT")

set(${KIT}_INCLUDE_DIRECTORIES
  ${vtkSlicerAnnotationsModuleMRML_SOURCE_DIR}
  ${vtkSlicerAnnotationsModuleMRML_BINARY_DIR}
  ${Bender_INCLUDE_DIRS}
  )

set(${KIT}_SRCS
  vtkMRMLArmatureNode.cxx
  vtkMRMLArmatureNode.h
  vtkMRMLArmatureStorageNode.cxx
  vtkMRMLArmatureStorageNode.h
  vtkMRMLBoneDisplayNode.cxx
  vtkMRMLBoneDisplayNode.h
  vtkMRMLBoneNode.cxx
  vtkMRMLBoneNode.h
  vtkMRMLNodeHelper.cxx
  vtkMRMLNodeHelper.h
  )

set(${KIT}_TARGET_LIBRARIES
  vtkSlicerAnnotationsModuleMRML
  ${Bender_LIBRARIES}
  )

#-----------------------------------------------------------------------------
SlicerMacroBuildModuleMRML(
  NAME ${KIT}
  EXPORT_DIRECTIVE ${${KIT}_EXPORT_DIRECTIVE}
  INCLUDE_DIRECTORIES ${${KIT}_INCLUDE_DIRECTORIES}
  SRCS ${${KIT}_SRCS}
  TARGET_LIBRARIES ${${KIT}_TARGET_LIBRARIES}
  )
