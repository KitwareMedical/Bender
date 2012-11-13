
#-----------------------------------------------------------------------------
set(MODULE_NAME ArmatureWeight) # Do not use 'project()'

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_MODULE_PATH})

FIND_PACKAGE(Eigen3 REQUIRED) #requires FindEigen3.cmake to be in the source directory
include_directories(${EIGEN3_INCLUDE_DIR})

find_path(EIGENSPARSESOLVE_DIR REQUIRED)
set(EIGENSPARSESOLVE_INCLUDES ${EIGENSPARSESOLVE_DIR}/src)
set(EIGENSPARSESOLVE_LIBRARIES ${EIGENSPARSESOLVE_DIR}/build/libEigenSparseSolve.so)
find_library(EIGENSPARSESOLVE_LIBRARIES REQUIRED)

include_directories(${EIGENSPARSESOLVE_INCLUDES})


SEMMacroBuildCLI(
  NAME ${MODULE_NAME}
  LOGO_HEADER ${Slicer_SOURCE_DIR}/Resources/ITKLogo.h
  TARGET_LIBRARIES ${EIGENSPARSESOLVE_LIBRARIES} ${ITK_LIBRARIES} vtkIO vtkGraphics
  )


# if(BUILD_TESTING)
#   add_subdirectory(Testing)
# endif()

