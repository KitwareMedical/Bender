project(vtkBender${MODULE_NAME}ModuleMRMLDisplayableManagers)

set(KIT ${PROJECT_NAME})

set(${KIT}_EXPORT_DIRECTIVE
    "VTK_BENDER_${MODULE_NAME_UPPER}_MRML_DISPLAYABLEMANAGERS_EXPORT")

set(${KIT}_INCLUDE_DIRECTORIES
  ${vtkBenderWidgets_SOURCE_DIR}
  ${vtkBenderWidgets_BINARY_DIR}
  ${vtkSlicerAnnotationsModuleMRMLDisplayableManager_SOURCE_DIR}
  ${vtkSlicerAnnotationsModuleMRMLDisplayableManager_BINARY_DIR}
  ${vtkBenderCommonMath_SOURCE_DIR}
  )

set(displayable_manager_SRCS
  vtkMRMLArmatureDisplayableManager.cxx
  vtkMRMLArmatureDisplayableManager.h
  )

set(VTK_USE_INSTANTIATOR_NEW 1)
VTK_MAKE_INSTANTIATOR3("${MODULE_NAME}ModuleInstantiator"
  displayable_manager_instantiator_SRCS
  "${displayable_manager_SRCS}"
  "${${KIT}_EXPORT_DIRECTIVE}"
  ${CMAKE_CURRENT_BINARY_DIR}
  "${KIT}Export.h"
  )

set(${KIT}_SRCS
  ${displayable_manager_instantiator_SRCS}
  ${displayable_manager_SRCS}
  )

set(${KIT}_TARGET_LIBRARIES
  vtkBender${MODULE_NAME}ModuleMRMLCore
  vtkBenderWidgets
  vtkSlicerAnnotationsModuleMRMLDisplayableManager
  )


#-----------------------------------------------------------------------------
SlicerMacroBuildModuleLogic(
  NAME ${KIT}
  EXPORT_DIRECTIVE ${${KIT}_EXPORT_DIRECTIVE}
  INCLUDE_DIRECTORIES ${${KIT}_INCLUDE_DIRECTORIES}
  SRCS ${${KIT}_SRCS}
  TARGET_LIBRARIES ${${KIT}_TARGET_LIBRARIES}
  )
