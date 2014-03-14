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

set(KIT ${PROJECT_NAME}Plugins)

set(${KIT}_EXPORT "Q_SLICER_MODULE_${MODULE_NAME_UPPER}_WIDGETS_PLUGINS_EXPORT")

set(${KIT}_SRCS
  qSlicerArmaturesModuleWidgetsAbstractPlugin.cxx
  qSlicerArmaturesModuleWidgetsAbstractPlugin.h
  qSlicerArmaturesModuleWidgetsPlugin.cxx
  qSlicerArmaturesModuleWidgetsPlugin.h

  qMRMLArmaturesAnimationWidgetPlugin.cxx
  )

set(${KIT}_MOC_SRCS
  qSlicerArmaturesModuleWidgetsPlugin.h

  qMRMLArmaturesAnimationWidgetPlugin.h
  )

set(${KIT}_TARGET_LIBRARIES
  qSlicerArmaturesModuleWidgets
  )

#-----------------------------------------------------------------------------
ctkMacroBuildQtDesignerPlugin(
  NAME ${KIT}
  EXPORT_DIRECTIVE ${${KIT}_EXPORT}
  SRCS ${${KIT}_SRCS}
  MOC_SRCS ${${KIT}_MOC_SRCS}
  TARGET_LIBRARIES ${${KIT}_TARGET_LIBRARIES}
  )
