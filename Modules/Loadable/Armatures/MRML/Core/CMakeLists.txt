project(vtkBender${MODULE_NAME}ModuleMRMLCore)

set(KIT ${PROJECT_NAME})

set(${KIT}_EXPORT_DIRECTIVE
    "VTK_BENDER_${MODULE_NAME_UPPER}_MRML_CORE_EXPORT")

set(${KIT}_INCLUDE_DIRECTORIES
  ${vtkSlicerAnnotationsModuleMRML_SOURCE_DIR}
  ${vtkSlicerAnnotationsModuleMRML_BINARY_DIR}
  ${vtkBenderWidgets_SOURCE_DIR}
  ${vtkBenderWidgets_BINARY_DIR}
  )

set(${KIT}_SRCS
  vtkMRMLArmatureNode.cxx
  vtkMRMLArmatureNode.h
  vtkMRMLBoneDisplayNode.cxx
  vtkMRMLBoneDisplayNode.h
  vtkMRMLBoneNode.cxx
  vtkMRMLBoneNode.h
  )

set(${KIT}_TARGET_LIBRARIES
  #${MRML_LIBRARIES}
  vtkBenderWidgets
  vtkSlicerAnnotationsModuleMRML
  )

#-----------------------------------------------------------------------------
SlicerMacroBuildModuleMRML(
  NAME ${KIT}
  EXPORT_DIRECTIVE ${${KIT}_EXPORT_DIRECTIVE}
  INCLUDE_DIRECTORIES ${${KIT}_INCLUDE_DIRECTORIES}
  SRCS ${${KIT}_SRCS}
  TARGET_LIBRARIES ${${KIT}_TARGET_LIBRARIES}
  )
