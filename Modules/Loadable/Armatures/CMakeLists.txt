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

#-----------------------------------------------------------------------------
set(MODULE_NAME "Armatures") # Do not use 'project()'
set(MODULE_TITLE "Armatures")

string(TOUPPER ${MODULE_NAME} MODULE_NAME_UPPER)

find_package(Bender REQUIRED)
include(${Bender_USE_FILE})

#-----------------------------------------------------------------------------
add_subdirectory(MRML)
add_subdirectory(Logic)

#-----------------------------------------------------------------------------
set(MODULE_EXPORT_DIRECTIVE "Q_SLICER_QTMODULES_${MODULE_NAME_UPPER}_EXPORT")

# Additional includes - Current_{source,binary} and Slicer_{Libs,Base} already included
set(MODULE_INCLUDE_DIRECTORIES
  ${CMAKE_CURRENT_SOURCE_DIR}/Logic
  ${CMAKE_CURRENT_BINARY_DIR}/Logic
  ${vtkSlicerModelsModuleLogic_SOURCE_DIR}
  ${vtkSlicerModelsModuleLogic_BINARY_DIR}
  ${qSlicerAnnotationsModuleWidgets_SOURCE_DIR}
  ${qSlicerAnnotationsModuleWidgets_BINARY_DIR}
  )

set(MODULE_SRCS
  qSlicerArmaturesIO.cxx
  qSlicerArmaturesIO.h
  qSlicerArmaturesModule.cxx
  qSlicerArmaturesModule.h
  qSlicerArmaturesModuleWidget.cxx
  qSlicerArmaturesModuleWidget.h
  qSlicerArmaturesModuleWidget_p.h
  )

set(MODULE_MOC_SRCS
  qSlicerArmaturesIO.h
  qSlicerArmaturesModule.h
  qSlicerArmaturesModuleWidget.h
  qSlicerArmaturesModuleWidget_p.h
  )

set(MODULE_UI_SRCS
  Resources/UI/qSlicerArmaturesModule.ui
  )

set(MODULE_TARGET_LIBRARIES
  qSlicerAnnotationsModuleWidgets
  vtkSlicerArmaturesModuleLogic
  vtkBenderArmaturesModuleMRMLDisplayableManagers
  )

set(MODULE_RESOURCES
  Resources/qSlicerArmaturesModule.qrc
  )

#-----------------------------------------------------------------------------
slicerMacroBuildQtModule(
  NAME ${MODULE_NAME}
  TITLE ${MODULE_TITLE}
  EXPORT_DIRECTIVE ${MODULE_EXPORT_DIRECTIVE}
  INCLUDE_DIRECTORIES ${MODULE_INCLUDE_DIRECTORIES}
  SRCS ${MODULE_SRCS}
  MOC_SRCS ${MODULE_MOC_SRCS}
  UI_SRCS ${MODULE_UI_SRCS}
  TARGET_LIBRARIES ${MODULE_TARGET_LIBRARIES}
  RESOURCES ${MODULE_RESOURCES}
  )

#-----------------------------------------------------------------------------
if(BUILD_TESTING)
  add_subdirectory(Testing)
endif()
