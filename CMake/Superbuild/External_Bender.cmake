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
# Slicer
#
set(proj Bender)

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
set(${proj}_DEPENDENCIES ${ITK_EXTERNAL_NAME} VTK)

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

  message(">>>>>>>>>>>>>>> ${Bender_SOURCE_DIR}")

  set(${proj}_DIR ${CMAKE_BINARY_DIR}/${proj}-build)
  ExternalProject_Add(${proj}
    SOURCE_DIR ${Bender_SOURCE_DIR}
    BINARY_DIR ${${proj}_DIR}
    DOWNLOAD_COMMAND ""
    CMAKE_GENERATOR ${gen}
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    CMAKE_ARGS
      ${ctk_superbuild_boolean_args}
      ${CMAKE_OSX_EXTERNAL_PROJECT_ARGS}
      -DBender_SUPERBUILD:BOOL=OFF
      -DDOCUMENTATION_ARCHIVES_OUTPUT_DIRECTORY:PATH=${DOCUMENTATION_ARCHIVES_OUTPUT_DIRECTORY}
      -DDOXYGEN_EXECUTABLE:FILEPATH=${DOXYGEN_EXECUTABLE}
      -DBender_SUPERBUILD_BINARY_DIR:PATH=${Bender_BINARY_DIR}
      -DBender_CMAKE_ARCHIVE_OUTPUT_DIRECTORY:PATH=${Bender_CMAKE_ARCHIVE_OUTPUT_DIRECTORY}
      -DBender_CMAKE_LIBRARY_OUTPUT_DIRECTORY:PATH=${Bender_CMAKE_LIBRARY_OUTPUT_DIRECTORY}
      -DBender_CMAKE_RUNTIME_OUTPUT_DIRECTORY:PATH=${Bender_CMAKE_RUNTIME_OUTPUT_DIRECTORY}
      -DBender_INSTALL_BIN_DIR:STRING=${Slicer_INSTALL_BIN_DIR}
      -DBender_INSTALL_LIB_DIR:STRING=${Slicer_INSTALL_LIB_DIR}
      -DBender_INSTALL_INCLUDE_DIR:STRING=${Bender_INSTALL_INCLUDE_DIR}
      -DBender_INSTALL_DOC_DIR:STRING=${Bender_INSTALL_DOC_DIR}
      -DBender_BUILD_SHARED_LIBS:BOOL=${Bender_BUILD_SHARED_LIBS}
      -DCMAKE_INSTALL_PREFIX:PATH=${ep_install_dir}
      -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
      -DCMAKE_CXX_FLAGS:STRING=${ep_common_cxx_flags}
      -DCMAKE_C_FLAGS:STRING=${ep_common_c_flags}
      -DBender_EXTERNAL_LIBRARY_DIRS:STRING=${Bender_EXTERNAL_LIBRARY_DIRS}
      -DQT_QMAKE_EXECUTABLE:FILEPATH=${QT_QMAKE_EXECUTABLE}
      -DGIT_EXECUTABLE:FILEPATH=${GIT_EXECUTABLE}
      -DVTK_DIR:PATH=${VTK_DIR}
      -DITK_DIR:PATH=${ITK_DIR}
      #${dependency_args}
    DEPENDS
      ${${proj}_DEPENDENCIES}
    )

else()
  # The project is provided using ${proj}_DIR, nevertheless since other project may depend on ${proj},
  # let's add an 'empty' one
  SlicerMacroEmptyExternalProject(${proj} "${${proj}_DEPENDENCIES}")
endif()

#list(APPEND Bender_SUPERBUILD_EP_ARGS -D${proj}_DIR:PATH=${${proj}_DIR})

