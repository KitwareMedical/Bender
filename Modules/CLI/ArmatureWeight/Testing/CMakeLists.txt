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
set(BASELINE ${CMAKE_CURRENT_SOURCE_DIR}/../Data/Baseline)
set(INPUT ${CMAKE_CURRENT_SOURCE_DIR}/../Data/Input)
set(CLP ${MODULE_NAME})
set(TEMP "${Slicer_BINARY_DIR}/Testing/Temporary")

#-----------------------------------------------------------------------------
set(TESTINCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/../)
include_directories(${TESTINCLUDE})

#-----------------------------------------------------------------------------
add_executable(${CLP}Test ${CLP}Test.cxx)
target_link_libraries(${CLP}Test ${CLP}Lib)
set_target_properties(${CLP}Test PROPERTIES LABELS ${CLP})

function(add_module_test)
  set(testname ${CLP}Test)
  set(restLabelMap ${INPUT}/labelmap.mha)
  set(armature ${INPUT}/armature.vtk)
  set(weightDirectory ${TEMP})

  add_test(NAME ${testname} COMMAND ${Launcher_Command} $<TARGET_FILE:${CLP}Test>
    --compare ${BASELINE}/weight_0.mha
              ${TEMP}/weight_0.mha
    ModuleEntryPoint ${restLabelMap} ${armature} ${weightDirectory}
    )
  set_property(TEST ${testname} PROPERTY LABELS ${CLP})
endfunction()
add_module_test()

#-----------------------------------------------------------------------------
add_executable(${CLP}TestHeatDiffusion TestSimpleHeatDiffusion.cxx)
target_link_libraries(${CLP}TestHeatDiffusion ${CLP}Lib)
set_target_properties(${CLP}TestHeatDiffusion PROPERTIES LABELS ${CLP})

set(testname ${CLP}TestHeatDiffusion)
add_test(NAME ${testname} COMMAND ${Launcher_Command} $<TARGET_FILE:${CLP}TestHeatDiffusion>
  ModuleEntryPoint
  )
set_property(TEST ${testname} PROPERTY LABELS ${CLP})

