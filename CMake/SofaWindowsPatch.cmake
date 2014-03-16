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
# Patch SOFA for windows
#
# Downloads the windows necessary dependencies and unzips them in the sofa
# build directory. This file is called as an extra external project on windows
# machines
#

if(NOT WIN32)
  return()
endif()

set(Sofa_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# Download patch file
set(sofa_patch_url "https://gforge.inria.fr/frs/download.php/33142/sofa-win-dependencies-21-11-2013.zip")
set(sofa_dependencies_file "${Sofa_SOURCE_DIR}/dependencies.zip")
file(DOWNLOAD ${sofa_patch_url} ${sofa_dependencies_file})

# Unpack it
execute_process (
  COMMAND ${CMAKE_COMMAND} -E tar xzf ${sofa_dependencies_file}
  WORKING_DIRECTORY ${Sofa_SOURCE_DIR}
  )

# Delete zip file
execute_process (
  COMMAND ${CMAKE_COMMAND} -E remove ${sofa_dependencies_file}
  WORKING_DIRECTORY ${Sofa_SOURCE_DIR}
  )
