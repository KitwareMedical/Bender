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
set(MODULE_NAME ArmatureWeight) # Do not use 'project()'

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_MODULE_PATH})

find_package(ITK REQUIRED)
include(${ITK_USE_FILE})

find_package(VTK REQUIRED)
include(${VTK_USE_FILE})

find_package(Bender REQUIRED)
include(${Bender_USE_FILE})

set(MODULE_INCLUDE_DIRECTORIES
  ${Bender_INCLUDE_DIRS}
  ${Eigen_INCLUDE_DIRS}
  )

set(MODULE_ADDITIONAL_SRCS
  ArmatureWeightWriter.cxx
  ArmatureWeightWriter.h
  )
set(MODULE_TARGET_LIBRARIES
  ${Bender_LIBRARIES}
  )

SEMMacroBuildCLI(
  NAME ${MODULE_NAME}
  ADDITIONAL_SRCS ${MODULE_ADDITIONAL_SRCS}
  LOGO_HEADER ${Bender_SOURCE_DIR}/Utilities/Logos/AFRL.h
  INCLUDE_DIRECTORIES ${MODULE_INCLUDE_DIRECTORIES}
  TARGET_LIBRARIES ${MODULE_TARGET_LIBRARIES} ${ITK_LIBRARIES} vtkIO vtkGraphics
  )

#-----------------------------------------------------------------------------
if(BUILD_TESTING)
  add_subdirectory(Testing)
endif()
