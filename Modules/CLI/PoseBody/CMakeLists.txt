
#-----------------------------------------------------------------------------
set(MODULE_NAME PoseBody) # Do not use 'project()'

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_MODULE_PATH})

SEMMacroBuildCLI(
  NAME ${MODULE_NAME}
  LOGO_HEADER ${Slicer_SOURCE_DIR}/Resources/ITKLogo.h
  TARGET_LIBRARIES ${ITK_LIBRARIES} vtkIO vtkGraphics
  )

# if(BUILD_TESTING)
#   add_subdirectory(Testing)
# endif()

