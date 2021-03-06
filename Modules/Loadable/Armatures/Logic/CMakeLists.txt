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

project(vtkSlicer${MODULE_NAME}ModuleLogic)

set(KIT ${PROJECT_NAME})

set(${KIT}_EXPORT_DIRECTIVE "VTK_SLICER_${MODULE_NAME_UPPER}_MODULE_LOGIC_EXPORT")

set(${KIT}_INCLUDE_DIRECTORIES
  ${vtkBender${MODULE_NAME}ModuleMRMLCore_SOURCE_DIR}
  ${vtkBender${MODULE_NAME}ModuleMRMLCore_BINARY_DIR}
  ${vtkSlicerModelsModuleLogic_SOURCE_DIR}
  ${vtkSlicerModelsModuleLogic_BINARY_DIR}
  )

set(${KIT}_SRCS
  vtkSlicer${MODULE_NAME}Logic.cxx
  vtkSlicer${MODULE_NAME}Logic.h
  )

set(${KIT}_TARGET_LIBRARIES
  ${ITK_LIBRARIES}
  ${Bender_LIBRARIES}
  vtkBender${MODULE_NAME}ModuleMRMLCore
  vtkSlicerModelsModuleLogic
  vtkSlicerAnnotationsModuleLogic
  )

#-----------------------------------------------------------------------------
SlicerMacroBuildModuleLogic(
  NAME ${KIT}
  EXPORT_DIRECTIVE ${${KIT}_EXPORT_DIRECTIVE}
  INCLUDE_DIRECTORIES ${${KIT}_INCLUDE_DIRECTORIES}
  SRCS ${${KIT}_SRCS}
  TARGET_LIBRARIES ${${KIT}_TARGET_LIBRARIES}
  )

set(${KIT}_PRESET_RESOURCES
  Adult-armature.vtk
  Arm-armature.vtk
  Child-armature.vtk
  StickMan-armature.vtk
  )
set (${KIT}_PRESET_RESOURCES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../Resources/Armatures)

foreach(resource ${${KIT}_PRESET_RESOURCES})
  configure_file(
    ${${KIT}_PRESET_RESOURCES_DIR}/${resource}
    ${CMAKE_BINARY_DIR}/${Slicer_QTLOADABLEMODULES_SHARE_DIR}/${MODULE_NAME}/${resource}
    COPYONLY)

  install(
    FILES ${CMAKE_BINARY_DIR}/${Slicer_QTLOADABLEMODULES_SHARE_DIR}/${MODULE_NAME}/${resource}
    DESTINATION ${Slicer_INSTALL_QTLOADABLEMODULES_SHARE_DIR}/${MODULE_NAME} COMPONENT Runtime)
endforeach()
