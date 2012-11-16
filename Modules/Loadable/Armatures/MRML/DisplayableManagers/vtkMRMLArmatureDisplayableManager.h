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

#ifndef __vtkMRMLArmatureDisplayableManager_h
#define __vtkMRMLArmatureDisplayableManager_h

// MRMLDisplayableManager includes
#include "vtkMRMLAnnotationDisplayableManager.h"
#include "vtkBenderArmaturesModuleMRMLDisplayableManagersExport.h"

/// \brief Displayable manager for bone armatures in 3D views.
///
/// Add and control a vtkBoneWidget for each bone node (vtkMRMLBoneNode) found
/// in an armature node (vtkMRMLArmatureNode).
class VTK_BENDER_ARMATURES_MRML_DISPLAYABLEMANAGERS_EXPORT vtkMRMLArmatureDisplayableManager
  : public vtkMRMLAnnotationDisplayableManager
{

public:
  static vtkMRMLArmatureDisplayableManager* New();
  vtkTypeRevisionMacro(vtkMRMLArmatureDisplayableManager,
                       vtkMRMLAnnotationDisplayableManager);
  void PrintSelf(ostream& os, vtkIndent indent);

protected:
  vtkMRMLArmatureDisplayableManager();
  virtual ~vtkMRMLArmatureDisplayableManager();

  /// Initialize the displayable manager based on its associated
  /// vtkMRMLSliceNode
  virtual void Create();

  /// WidgetCallback is a static function to relay modified events from the Logic
  virtual void ProcessWidgetsEvents(vtkObject *caller,
                                    unsigned long event,
                                    void *callData);

  /// WidgetCallback is a static function to relay modified events from the nodes
  virtual void ProcessMRMLNodesEvents(vtkObject *caller,
    unsigned long event, void *callData);

  virtual void UnobserveMRMLScene();
  virtual void UpdateFromMRMLScene();
  virtual void OnMRMLSceneNodeAdded(vtkMRMLNode* node);
  virtual void OnMRMLSceneNodeRemoved(vtkMRMLNode* node);
  virtual void OnMRMLNodeModified(vtkMRMLNode* node);

  /// Reimplemented to bypass vtkMRMLAnnotationDisplayableManager as it prevents
  /// OnMRMLNodeModified(vtkMRMLNode* node) from being called.
  /// \sa OnMRMLNodeModified()
  virtual void OnMRMLAnnotationNodeModifiedEvent(vtkMRMLNode* node);

  /// Reimplemented to support 2 types of node types: Armature and Bone.
  virtual bool IsManageable(vtkMRMLNode* node);
  /// Reimplemented to support 2 types of node types: Armature and Bone.
  virtual bool IsManageable(const char* nodeID);

  /// Callback for click in RenderWindow
  virtual void OnClickInRenderWindow(double x, double y,
                                     const char *associatedNodeID);

private:
  vtkMRMLArmatureDisplayableManager(const vtkMRMLArmatureDisplayableManager&);// Not implemented
  void operator=(const vtkMRMLArmatureDisplayableManager&);                   // Not Implemented

  class vtkInternal;
  vtkInternal* Internal;
};

#endif // __vtkMRMLArmatureDisplayableManager_h
