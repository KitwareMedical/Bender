/*=========================================================================

  Program: Bender

  Copyright (c) Kitware Inc.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

=========================================================================*/

#ifndef __vtkSlicerArmaturesLogic_h
#define __vtkSlicerArmaturesLogic_h

// Slicer includes
#include "vtkSlicerModuleLogic.h"
#include "vtkSlicerArmaturesModuleLogicExport.h"
class vtkSlicerModelsLogic;
class vtkSlicerAnnotationModuleLogic;

// MRML includes
class vtkMRMLModelNode;

// VTK includes
class vtkPolyData;
class vtkXMLDataElement;

// STD includes
#include <cstdlib>

/// \ingroup Bender_Logics
/// \brief Logic class for armature manipulation.
///
/// This class manages the logic associated with reading, saving,
/// and changing properties of armatures.
class VTK_SLICER_ARMATURES_MODULE_LOGIC_EXPORT vtkSlicerArmaturesLogic
  : public vtkSlicerModuleLogic
{
public:
  static vtkSlicerArmaturesLogic *New();
  vtkTypeMacro(vtkSlicerArmaturesLogic,vtkSlicerModuleLogic);
  void PrintSelf(ostream& os, vtkIndent indent);

  vtkMRMLModelNode* AddArmatureFile(const char* fileName);

  virtual void SetModelsLogic(vtkSlicerModelsLogic* modelsLogic);
  vtkGetObjectMacro(ModelsLogic, vtkSlicerModelsLogic);

  /// Set the Annotations module logic.
  virtual void SetAnnotationsLogic(vtkSlicerAnnotationModuleLogic* logic);
  vtkGetObjectMacro(AnnotationsLogic, vtkSlicerAnnotationModuleLogic);

  /// Register bone pose mode.
  /// \sa vtkMRMLSelectionNode::AddNewAnnotationIDToList()
  virtual void ObserveMRMLScene();

  /// Register armature and bone nodes.
  /// \sa vtkMRMLScene::RegisterNodeClass()
  virtual void RegisterNodes();

protected:
  vtkSlicerArmaturesLogic();
  virtual ~vtkSlicerArmaturesLogic();

  void ReadBone(vtkXMLDataElement* boneElement, vtkPolyData* polyData,
                const double origin[3], const double orientation[4]);
  void ReadPose(vtkXMLDataElement* poseElement, double orientation[4], bool invert = true);

  vtkSlicerModelsLogic* ModelsLogic;
  vtkSlicerAnnotationModuleLogic* AnnotationsLogic;
private:
  vtkSlicerArmaturesLogic(const vtkSlicerArmaturesLogic&); // Not implemented
  void operator=(const vtkSlicerArmaturesLogic&);               // Not implemented
};

#endif

