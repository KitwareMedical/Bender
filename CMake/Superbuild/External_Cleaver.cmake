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
# Cleaver
#
set(proj Cleaver)

# Make sure this file is included only once
get_filename_component(proj_filename ${CMAKE_CURRENT_LIST_FILE} NAME_WE)
if(${proj_filename}_proj)
  return()
endif()
set(${proj_filename}_proj ${proj})

# Sanity checks
if(DEFINED ${proj}_DIR AND NOT EXISTS ${${proj}_DIR})
  message(FATAL_ERROR "${proj}_DIR variable is defined but corresponds to non-existing directory")
endif()

# Set dependency list
set(${proj}_DEPENDENCIES "")

# Include dependent projects if any
SlicerMacroCheckExternalProjectDependency(${proj})

# Restore the proj variable
get_filename_component(proj_filename ${CMAKE_CURRENT_LIST_FILE} NAME_WE)
set(proj ${${proj_filename}_proj})

if(NOT DEFINED ${proj}_DIR)
  message(STATUS "${__indent}Adding project ${proj}")

  # Set CMake OSX variable to pass down the external project
  set(CMAKE_OSX_EXTERNAL_PROJECT_ARGS)
  if(APPLE)
    list(APPEND CMAKE_OSX_EXTERNAL_PROJECT_ARGS
      -DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}
      -DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}
      -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET})
  endif()

  set(${proj}_DIR ${CMAKE_BINARY_DIR}/${proj}-build)
  ExternalProject_Add(${proj}
    SOURCE_DIR ${CMAKE_BINARY_DIR}/${proj}
    BINARY_DIR ${${proj}_DIR}
    GIT_REPOSITORY "git://public.kitware.com/Bender/Cleaver.git"
    GIT_TAG "fdd780995cf9b3b250572648f7faad37dea71e95"
    UPDATE_COMMAND ""
    INSTALL_COMMAND ""
    CMAKE_GENERATOR ${gen}
    CMAKE_ARGS
      -DBUILD_CLEAVER_APP:BOOL=OFF
      -DBUILD_SHARED_LIBS:BOOL=OFF
      -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    DEPENDS
      ${${proj}_DEPENDENCIES}
    )

else()
  # The project is provided using ${proj}_DIR, nevertheless since other project may depend on ${proj},
  # let's add an 'empty' one
  SlicerMacroEmptyExternalProject(${proj} "${${proj}_DEPENDENCIES}")
endif()

# list(APPEND Bender_SUPERBUILD_EP_ARGS -D${proj}_DIR:PATH=${${proj}_DIR})

