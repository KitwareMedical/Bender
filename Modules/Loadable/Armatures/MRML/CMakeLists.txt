#-----------------------------------------------------------------------------
set(BENDER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../..)
add_subdirectory(${BENDER_DIR} ${CMAKE_CURRENT_BINARY_DIR}/Bender)

add_subdirectory(Core)
add_subdirectory(DisplayableManagers)
