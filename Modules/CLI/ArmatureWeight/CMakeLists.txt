
#-----------------------------------------------------------------------------
set(MODULE_NAME ArmatureWeight) # Do not use 'project()'

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_MODULE_PATH})

find_package(ITK REQUIRED)
find_package(VTK REQUIRED)

set(MODULE_INCLUDE_DIRECTORIES
  ${Bender_SOURCE_DIR}/Libs/Common
  ${Bender_BINARY_DIR}/Libs/Common
  ${Bender_SOURCE_DIR}/Libs/EigenWrapper
  ${Bender_BINARY_DIR}/Libs/EigenWrapper
  ${EIGEN3_INCLUDE_DIR}
  )

set(MODULE_TARGET_LIBRARIES
  BenderEigenWrapper
  BenderCommon
  )

SEMMacroBuildCLI(
  NAME ${MODULE_NAME}
  LOGO_HEADER ${Slicer_SOURCE_DIR}/Resources/ITKLogo.h
  INCLUDE_DIRECTORIES ${MODULE_INCLUDE_DIRECTORIES}
  TARGET_LIBRARIES ${MODULE_TARGET_LIBRARIES} ${ITK_LIBRARIES} vtkIO vtkGraphics
  )