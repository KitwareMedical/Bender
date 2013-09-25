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

// Armatures includes
#include "vtkSlicerArmaturesModuleLogicExport.h"
class vtkMRMLArmatureNode;
class vtkMRMLBoneNode;

// Slicer includes
#include "vtkSlicerModuleLogic.h"
class vtkSlicerModelsLogic;
class vtkSlicerAnnotationModuleLogic;

// MRML includes
class vtkMRMLModelNode;

// VTK includes
class vtkCollection;
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

  /// Reads an armature from a model. Returns the armature node added.
  /// \sa CreateArmatureFromModel(vtkPolyData*)
  vtkMRMLArmatureNode* AddArmatureFile(const char* fileName);

  virtual void SetModelsLogic(vtkSlicerModelsLogic* modelsLogic);
  vtkGetObjectMacro(ModelsLogic, vtkSlicerModelsLogic);

  /// Set the Annotations module logic.
  virtual void SetAnnotationsLogic(vtkSlicerAnnotationModuleLogic* logic);
  vtkGetObjectMacro(AnnotationsLogic, vtkSlicerAnnotationModuleLogic);

  /// Set active bone.
  /// The active bone will be the parent of the future bones added into the
  /// scene. If there is no active bone (bone == 0), the Armature is
  /// considered active.
  void SetActiveArmature(vtkMRMLArmatureNode* armature);
  vtkMRMLArmatureNode* GetActiveArmature();

  /// Set the mode of the active armature.
  /// Do nothing if no armature active for the set or return -1.
  void SetActiveArmatureWidgetState(int mode);
  int GetActiveArmatureWidgetState();

  /// \tbd move to vtkMRMLArmatureNode ?
  /// Set active bone.
  /// The active bone will be the parent of the future bones added into the
  /// scene. If there is no active bone (bone == 0), the Armature is
  /// considered active.
  void SetActiveBone(vtkMRMLBoneNode* bone);
  vtkMRMLBoneNode* GetActiveBone();

  /// Return the armature the bone belongs to.
  /// \sa GetBoneParent()
  vtkMRMLArmatureNode* GetBoneArmature(vtkMRMLBoneNode* bone);
  /// Return the parent of the bone or 0 if the bone has no parent or its
  /// parent is an armature.
  /// \sa GetBoneArmature()
  vtkMRMLBoneNode* GetBoneParent(vtkMRMLBoneNode* bone);

  /// Observe MRML scene events
  virtual void SetMRMLSceneInternal(vtkMRMLScene * newScene);

  /// Register bone pose mode.
  /// \sa vtkMRMLSelectionNode::AddNewAnnotationIDToList()
  virtual void ObserveMRMLScene();

  /// Register armature and bone nodes.
  /// \sa vtkMRMLScene::RegisterNodeClass()
  virtual void RegisterNodes();

protected:
  vtkSlicerArmaturesLogic();
  virtual ~vtkSlicerArmaturesLogic();

  virtual void ProcessMRMLSceneEvents(vtkObject* caller,
                                      unsigned long event,
                                      void * callData);
  virtual void OnMRMLSceneNodeAdded(vtkMRMLNode* node);
  virtual void OnMRMLSceneNodeAboutToBeRemoved(vtkMRMLNode* node);

  virtual void ProcessMRMLLogicsEvents(vtkObject* caller, unsigned long event,
                                       void* callData);

  vtkSlicerModelsLogic* ModelsLogic;
  vtkSlicerAnnotationModuleLogic* AnnotationsLogic;

private:
  vtkSlicerArmaturesLogic(const vtkSlicerArmaturesLogic&); // Not implemented
  void operator=(const vtkSlicerArmaturesLogic&);               // Not implemented
};

#endif

