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

set(Bender_DEPENDENCIES Slicer)

#-----------------------------------------------------------------------------
# WARNING - No change should be required after this comment
#           when you are adding a new external project dependency.
#-----------------------------------------------------------------------------

#-----------------------------------------------------------------------------
# Declare Bender_EXTERNAL_LIBRARY_DIRS variable - This variable stores
# the library output directory associated with the different external project
# It's then used in Utilities/LastConfigureStep/BenderGenerateBenderConfig.cmake to
# configure BenderConfig.cmake.in
# This variable would then be exposed to project building against Bender
#SET(Bender_EXTERNAL_LIBRARY_DIRS)

#-----------------------------------------------------------------------------
# Git protocole option
#
option(Bender_USE_GIT_PROTOCOL "If behind a firewall turn this OFF to use http instead." ON)

set(git_protocol "git")
if(NOT Bender_USE_GIT_PROTOCOL)
  set(git_protocol "http")
endif()

#-----------------------------------------------------------------------------
# Enable and setup External project global properties
#
include(ExternalProject)
include(BenderEmptyExternalProject)
include(BenderCheckExternalProjectDependency)

#SET(ep_base "${CMAKE_BINARY_DIR}/SuperBuild")
#SET_PROPERTY(DIRECTORY PROPERTY EP_BASE ${ep_base})

set(ep_install_dir ${CMAKE_BINARY_DIR}/SuperBuild/Install)
set(ep_suffix      "-cmake")

set(ep_common_c_flags "${CMAKE_C_FLAGS_INIT} ${ADDITIONAL_C_FLAGS}")
set(ep_common_cxx_flags "${CMAKE_CXX_FLAGS_INIT} ${ADDITIONAL_CXX_FLAGS}")

# Compute -G arg for configuring external projects with the same CMake generator:
if(CMAKE_EXTRA_GENERATOR)
  set(gen "${CMAKE_EXTRA_GENERATOR} - ${CMAKE_GENERATOR}")
else()
  set(gen "${CMAKE_GENERATOR}")
endif()

# Use this value where semi-colons are needed in ep_add args:
set(sep "^^")

#-----------------------------------------------------------------------------
# Include external projects
#

# This variable will contain the list of CMake variable specific to each external project
# that should passed to Bender.
# The item of this list should have the following form: -D<EP>_DIR:PATH=${<EP>_DIR}
# where '<EP>' is an external project name.
set(Bender_SUPERBUILD_EP_ARGS)

#-----------------------------------------------------------------------------
# Makes sure ${Bender_BINARY_DIR}/Bender-build/bin exists
#IF(NOT EXISTS ${Bender_BINARY_DIR}/Bender-build/bin)
#  FILE(MAKE_DIRECTORY ${Bender_BINARY_DIR}/Bender-build/bin)
#ENDIF()

#-----------------------------------------------------------------------------
# Set superbuild boolean args
#

set(bender_cmake_boolean_args
  BUILD_TESTING
  Bender_USE_KWSTYLE
  )

set(bender_superbuild_boolean_args)
foreach(bender_cmake_arg ${bender_cmake_boolean_args})
  list(APPEND bender_superbuild_boolean_args -D${bender_cmake_arg}:BOOL=${${bender_cmake_arg}})
endforeach()

#-----------------------------------------------------------------------------
# Set CMake OSX variable to pass down the external project
set(CMAKE_OSX_EXTERNAL_PROJECT_ARGS)
if(APPLE)
  list(APPEND CMAKE_OSX_EXTERNAL_PROJECT_ARGS
    -DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}
    -DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}
    -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET})
endif()

bender_check_external_project_dependency(Bender)

#-----------------------------------------------------------------------------
# Bender Configure
#
set(proj Slicer)

if(CMAKE_GENERATOR MATCHES ".*Makefiles.*")
  set(bender_build_cmd "$(MAKE)")
else()
  set(bender_build_cmd ${CMAKE_COMMAND} --build ${Bender_BINARY_DIR}/${proj}-build --config ${CMAKE_CFG_INTDIR})
endif()

#-----------------------------------------------------------------------------
# Bender
#
#MESSAGE(STATUS SUPERBUILD_EXCLUDE_BenderBUILD_TARGET:${SUPERBUILD_EXCLUDE_BenderBUILD_TARGET})
if(NOT DEFINED SUPERBUILD_EXCLUDE_Bender_BUILD_TARGET
    OR NOT SUPERBUILD_EXCLUDE_Bender_BUILD_TARGET)
  set(Bender_BUILD_TARGET_ALL_OPTION "ALL")
else()
  set(Bender_BUILD_TARGET_ALL_OPTION "")
endif()

add_custom_target(Bender-build ${Bender_BUILD_TARGET_ALL_OPTION}
  COMMAND ${bender_build_cmd}
  WORKING_DIRECTORY ${Bender_BINARY_DIR}/${proj}-build
  )
add_dependencies(Bender-build ${proj})

#-----------------------------------------------------------------------------
# Custom target allowing to drive the build of Bender project itself
#
#add_custom_target(Bender
#  COMMAND ${bender_build_cmd}
#  WORKING_DIRECTORY ${Bender_BINARY_DIR}/${proj}-build
#  )
