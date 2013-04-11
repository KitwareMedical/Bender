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

set(KIT_PYTHON_SCRIPTS
  SkinModelMaker.py
  Workflow.py
  )

set(KIT_PYTHON_RESOURCES
  Resources/UI/SkinModelMaker.ui
  Resources/UI/Workflow.ui
  )

set(MODULE_SCRIPTS
  )

foreach(module ${KIT_PYTHON_SCRIPTS})
  list(FIND Slicer_QTSCRIPTEDMODULES_DISABLED ${module} _module_disabled)
  if(${_module_disabled} STREQUAL "-1")
    list(APPEND MODULE_SCRIPTS ${module})
  endif()
endforeach()

ctkMacroCompilePythonScript(
  TARGET_NAME BenderScriptedModules
  SCRIPTS "${MODULE_SCRIPTS}"
  RESOURCES "${KIT_PYTHON_RESOURCES}"
  DESTINATION_DIR ${Slicer_BINARY_DIR}/${Slicer_QTSCRIPTEDMODULES_LIB_DIR}
  INSTALL_DIR ${Slicer_INSTALL_QTSCRIPTEDMODULES_LIB_DIR}
  NO_INSTALL_SUBDIR
  )

if(BUILD_TESTING AND PYTHON_EXECUTABLE)
  #add_subdirectory(Testing)
endif()
