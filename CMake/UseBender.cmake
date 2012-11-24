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
# This module is provided as Bender_USE_FILE by BenderConfig.cmake. It can
# be INCLUDED in a project to load the needed compiler and linker
# settings to use Bender.
#

if(NOT Bender_USE_FILE_INCLUDED)
  set(Bender_USE_FILE_INCLUDED 1)

  set(CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH}
    ${Bender_CMAKE_DIR})
  # Add include directories needed to use Bender.
  include_directories(${Bender_INCLUDE_DIRS})

  # Add link directories needed to use Bender.
  link_directories(${Bender_LIBRARY_DIRS} ${Bender_EXTERNAL_LIBRARY_DIRS})

  if (NOT DEFINED QT_QMAKE_EXECUTABLE AND DEFINED Bender_QT_QMAKE_EXECUTABLE)
    set(QT_QMAKE_EXECUTABLE ${Bender_QT_QMAKE_EXECUTABLE})
  endif()

  # TODO: Make it more robust to Eigen name (EIGEN, Eigen3...)
  if (Eigen3_DIR)
    string(REPLACE "Eigen3-build" "Eigen3" eigen3_include_dir ${Eigen3_DIR})
    set(EIGEN3_INCLUDE_DIR_INTERNAL ${eigen3_include_dir} CACHE INTERNAL  "eigen3 include dir")
  endif()
  set(EIGEN3_INCLUDE_DIR ${EIGEN3_INCLUDE_DIR_INTERNAL})
  find_package(Eigen3)
  include_directories(${EIGEN3_INCLUDE_DIR})

endif()
