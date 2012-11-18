
#-----------------------------------------------------------------------------
set(MODULE_NAME EvalWeight) # Do not use 'project()'

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_MODULE_PATH})

find_package(ITK REQUIRED)
find_package(VTK REQUIRED)

set(MODULE_INCLUDE_DIRECTORIES
  ${BenderCommon_SOURCE_DIR}
  ${BenderCommon_BINARY_DIR}
  )

set(MODULE_TARGET_LIBRARIES
  BenderCommon
  )
SEMMacroBuildCLI(
  NAME ${MODULE_NAME}
  LOGO_HEADER ${Slicer_SOURCE_DIR}/Resources/ITKLogo.h
  INCLUDE_DIRECTORIES ${MODULE_INCLUDE_DIRECTORIES}
  TARGET_LIBRARIES ${ITK_LIBRARIES} vtkIO vtkGraphics ${MODULE_TARGET_LIBRARIES}
  )

# if(BUILD_TESTING)
#   add_subdirectory(Testing)
# endif()

