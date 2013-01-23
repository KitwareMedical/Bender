# Usage:
# \code
# SIMPLE_TEST( <test_exec_name> <testname> [argument1 ...])
# \endcode
#
# test_exec_name variable usually matches the value of PROJECT_NAME.
#
# The macro also associates a label to the test based on the current value of test_exec_name (or CLP or EXTENSION_NAME).
#
# Optionnal test argument(s) can be passed after specifying the <testname>.

macro(SIMPLE_TEST test_exec_name testname)

  if("${test_exec_name}" STREQUAL "")
    message(FATAL_ERROR "error: test_exec_name variable is not set !")
  endif()

  if("${Bender_LAUNCH_COMMAND}" STREQUAL "")
    message(FATAL_ERROR "error: Bender_LAUNCH_COMMAND variable is not set !")
  endif()

  if(NOT TARGET ${test_exec_name})
    message(FATAL_ERROR "error: Target '${test_exec_name}' is not defined !")
  endif()

  add_test(NAME ${testname}
           COMMAND ${Bender_LAUNCH_COMMAND} $<TARGET_FILE:${test_exec_name}> ${testname} ${ARGN})
  set_property(TEST ${testname} PROPERTY LABELS ${test_exec_name})
endmacro()

