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
# Sofa
#
set(proj SOFA)

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

# FindCUDA
set(SOFA_CUDA_ARGS -DSOFA-PLUGIN_SOFACUDA:BOOL=OFF)
find_package(CUDA QUIET)
if(CUDA_FOUND AND NOT WIN32)
  set(SOFA_CUDA_ARGS 
    -DSOFA-PLUGIN_SOFACUDA:BOOL=ON
    -DCUDA_TOOLKIT_ROOT_DIR:PATH=${CUDA_TOOLKIT_ROOT_DIR}
  )
endif()

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
    # GCC segmentation faults when compiling with -O2(default).
    if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
      list(APPEND CMAKE_CXX_FLAGS_RELEASE
        -DCXX_OPTIMIZATION_FLAGS:STRING="-O0")
    endif()
  endif()

  set(step_targets)
  if (WIN32)
    set(step_targets ${step_targets} ${proj}_PatchWindows)
  else()
    find_package(GLEW REQUIRED)
  endif()


  set(${proj}_DIR ${CMAKE_BINARY_DIR}/${proj}-build)
  ExternalProject_Add(${proj}
    SOURCE_DIR ${CMAKE_BINARY_DIR}/${proj}
    BINARY_DIR ${${proj}_DIR}
    GIT_REPOSITORY "git://public.kitware.com/Bender/SOFA.git"
    GIT_TAG "3704279d21342969216a621b8c9b96a5cab33796"
    INSTALL_COMMAND ""
    CMAKE_GENERATOR ${gen}
    LIST_SEPARATOR &&
    CMAKE_ARGS
      -DCMAKE_INSTALL_PREFIX:PATH=${ep_install_dir}
      -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
      ${CMAKE_OSX_EXTERNAL_PROJECT_ARGS}
      -DBUILD_SHARED_LIBS:BOOL=${BUILD_SHARED_LIBS}
      -DCMAKE_INSTALL_PREFIX:PATH=${ep_install_dir}
      -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
      -DCMAKE_CXX_FLAGS:STRING=${ep_common_cxx_flags}
      -DCMAKE_C_FLAGS:STRING=${ep_common_c_flags}
      -DQT_QMAKE_EXECUTABLE:FILEPATH=${QT_QMAKE_EXECUTABLE}
      #-DSOFA-EXTERNAL_INCLUDE_DIR:FILEPATH=${${proj}_DIR}/include
      #-DSOFA-EXTERNAL_LIBRARY_DIR:FILEPATH=${${proj}_DIR}/lib
      -DPRECONFIGURE_DONE:BOOL=ON
      -DSOFA-LIB_SIMULATION_GRAPH_DAG:BOOL=ON
      ${SOFA_CUDA_ARGS}
      -DGLEW_INCLUDE_DIR:PATH=${GLEW_INCLUDE_DIR}
      -DGLEW_LIBRARY:FILEPATH=${GLEW_LIBRARY}
      -DGLEW_LIBRARIES:FILEPATH=${GLEW_LIBRARY}
	  -DSOFA-TUTORIAL_CHAIN_HYBRID:BOOL=OFF
	  -DSOFA-TUTORIAL_COMPOSITE_OBJECT:BOOL=OFF
	  -DSOFA-TUTORIAL_HOUSE_OF_CARDS:BOOL=OFF
	  -DSOFA-TUTORIAL_MIXED_PENDULUM:BOOL=OFF
	  -DSOFA-TUTORIAL_ONE_PARTICLE:BOOL=OFF
	  -DSOFA-TUTORIAL_ONE_TETRAHEDRON:BOOL=OFF
    DEPENDS
      ${${proj}_DEPENDENCIES}
    STEP_TARGETS ${step_targets}
    )

  if (WIN32)
    # On windows Sofa needs a dependency package in the build dir
    ExternalProject_Add_Step(${proj} ${proj}_PatchWindows
      COMMAND ${CMAKE_COMMAND} -P ${Bender_SOURCE_DIR}/CMake/SofaWindowsPatch.cmake
      DEPENDEES download
      DEPENDERS configure
      COMMENT "Patch SOFA with its windows dependencies"
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/${proj}
    )
  endif()

else()
  # The project is provided using ${proj}_DIR, nevertheless since other project may depend on ${proj},
  # let's add an 'empty' one
  SlicerMacroEmptyExternalProject(${proj} "${${proj}_DEPENDENCIES}")
endif()

#list(APPEND Bender_SUPERBUILD_EP_ARGS -D${proj}_DIR:PATH=${${proj}_DIR})

