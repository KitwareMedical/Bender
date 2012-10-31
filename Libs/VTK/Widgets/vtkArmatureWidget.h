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

#ifndef __vtkArmatureWidget_h
#define __vtkArmatureWidget_h

// .NAME vtkArmatureWidget - Widget for holding bone widgets together.
// .SECTION Description
// The vtkArmatureWidget is designed as an interface for a collection
// of bones (vtkBoneWidget). Most importantly, it allows the user to create
// a skeleton of bones and manages all the necessaries callback to be able to
// animate it properly.
// Each bone is associated with an unique ID, a name, one parent and children.
// A given bone can have any number of children.
//
// .SECTION Options
// All the options applied to the armature are applied to all its bones.
// The bone can be accessed directly too through their names or the associated
// id.
//
// .SECTION See Also
// vtkArmatureRepresentation, vtkBoneWidget

// Bender includes
#include "vtkBenderWidgetsExport.h"

// VTK includes
#include <vtkAbstractWidget.h>
#include <vtkStdString.h>

#include <vector>

class ArmatureTreeNodeVectorType;
struct ArmatureTreeNode;

class vtkArmatureRepresentation;
class vtkArmatureWidgetCallback;
class vtkBoneWidget;
class vtkCollection;

class VTK_BENDER_WIDGETS_EXPORT vtkArmatureWidget : public vtkAbstractWidget
{
public:
  // Description:
  // Instantiate this class.
  static vtkArmatureWidget *New();

  // Description:
  // Standard methods for a VTK class.
  vtkTypeMacro(vtkArmatureWidget, vtkAbstractWidget);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // The method for activating and deactivating this widget. This method
  // must be overridden because it is a composite widget and does more than
  // its superclasses' vtkAbstractWidget::SetEnabled() method.
  // Note: this will update the bones to the representation defined by the
  // armature.
  virtual void SetEnabled(int);

  // Description:
  // Set the armature representation.
  // @sa vtkArmatureRepresentation CreateDefaultRepresentation()
  void SetRepresentation(vtkArmatureRepresentation* r);

  // Description:
  // Return the representation as a vtkArmatureRepresentation
  // @sa vtkArmatureRepresentation
  vtkArmatureRepresentation* GetArmatureRepresentation();

  // Description:
  // Create the default widget representation (vtkArmatureRepresentation)
  // if no one is set.
  // @sa vtkArmatureRepresentation SetRepresentation()
  virtual void CreateDefaultRepresentation();

  // Description:
  // Methods to change the whether the widget responds to interaction.
  // Overridden to pass the state to bone widgets.
  virtual void SetProcessEvents(int process);

  // Description:
  // Creates a bone and initializes it with all the options of the armature.
  // The bone can then be parsed to the armature with the Add methods.
  // Otherwise, the user is responsible for deleting the bone.
  // @sa AddBone() RemoveBone() HasBone()
  vtkBoneWidget* CreateBone(vtkBoneWidget* parent=0, const vtkStdString& name = "");

  // Description:
  // Create a bone to the armature with the given parent.
  // The new bone head will be set to its parent tail.
  // @sa CreateBone() AddBone() RemoveBone() HasBone() GetBoneParent()
  // @sa FindBoneChildren()
  // @sa GetBoneLinkedWithParent()
  vtkBoneWidget* CreateBone(
     vtkBoneWidget* parent,
     double tail[3], const vtkStdString& name = "");
  vtkBoneWidget* CreateBone(vtkBoneWidget* parent,
    double xTail, double yTail, double zTail,
    const vtkStdString& name = "");

  // Description:
  // Add a bone to the armature with the given parent. If the parent
  // is NULL, then the bone is considered to be root.
  // @sa CreateBone() AddBone() RemoveBone() HasBone() GetBoneParent()
  // @sa FindBoneChildren()
  void AddBone(vtkBoneWidget* bone,
    vtkBoneWidget* parent=0,
    bool linkedWithParent = true);

  // Description:
  // Delete the bone with the corresponding boneId. Return false
  // if no bone with the boneId is found or if the deletion failed.
  // When deleting a bone, its children (if any) are automatically
  // linked to its parent.
  // In the case of the root bone, the first child (if any) is chosen as the
  // new root. All the other children are linked to the new root.
  // @sa CreateBone() AddBone() RemoveBone() HasBone() GetBoneParent()
  // @sa FindBoneChildren()
  // @sa GetBoneLinkedWithParent()
  bool RemoveBone(vtkBoneWidget* bone);

  // Description:
  // Returns if the given bone belongs to this amrature or not.
  // @sa CreateBone() AddBone() RemoveBone()
  bool HasBone(vtkBoneWidget* bone);

  // Description:
  // Returns the bone's parent if the bone has one and it belongs
  // to the armature.
  // @sa CreateBone() AddBone() RemoveBone() HasBone() FindBoneChildren()
  vtkBoneWidget* GetBoneParent(vtkBoneWidget* bone);

  // Description:
  // Returns the bone's children if the bone has any and it belongs
  // to the armature.
  // The user is responsible for deleting the returned collection.
  // @sa CreateBone() AddBone() RemoveBone() HasBone() FindBoneChildren()
  vtkCollection* FindBoneChildren(vtkBoneWidget* parent);

  // Description:
  // Set the bone's name. Return false string if no bone is found.
  // @sa GetBoneName()
  bool SetBoneName(vtkBoneWidget* bone, const vtkStdString& name);

  // Description:
  // Get the bone's name. Return an empty string if no bone is found.
  // @sa SetBoneName()
  vtkStdString GetBoneName(vtkBoneWidget* bone);

  // Description:
  // Get a bone using its name. This will return the first bone with
  // the given name or null if no bone is found.
  // @sa GetBoneIdByName() SetBoneName() GetBoneName()
  vtkBoneWidget* GetBoneByName(const vtkStdString& name);

  // Description:
  // Get whether the bone is linked with its parent. If no bone
  // with boneId is found, returns -1.
  // @sa SetBoneLinkedWithParent()
  bool GetBoneLinkedWithParent(vtkBoneWidget* bone);

  // Description:
  // Set whether the bone is linked with its parent. If no bone
  // with boneId is found, if fails silently.
  // When a bone is linked to its parent, its head follows the movement
  // of its parent's tail.
  // @sa GetBoneLinkedWithParent()
  void SetBoneLinkedWithParent(vtkBoneWidget* bone, bool linked);

  // Description:
  // List of the differents representation possible for the armature's bones.
  // @sa SetBonesRepresentationType() GetBonesRepresentationType()
  // @sa vtkBoneWidgetRepresentation() vtkCylinderBoneRepresentation()
  // @sa vtkDoubleConeRepresentation()
  //BTX
  enum BonesRepresentationType{None = 0, Bone, Cylinder, DoubleCone};
  //ETX

  // Description:
  // Set/Get the representation type. If a new representation is chosen,
  // it will be applied to all the bones of the armature.
  // @sa vtkBoneWidgetRepresentation() vtkCylinderBoneRepresentation()
  // @sa vtkDoubleConeRepresentation()
  void SetBonesRepresentation(int representationType);
  vtkGetMacro(BonesRepresentationType, int);

  // Description:
  // Widget State of all the bones.
  //BTX
  enum WidgetStateType{Rest = 0, Pose};
  //ETX

  // Description:
  // Set/Get the bones widget state.
  // @sa vtkBoneWidget
  void SetWidgetState(int state);
  vtkGetMacro(WidgetState, int);

  // Description
  // Set/get if the debug axes are visible or not.
  // @sa vtkBoneWidget::AxesVisibilityType
  void SetAxesVisibility (int AxesVisibility);
  vtkGetMacro(AxesVisibility, int);

  // Description
  // Show/Hide the a line between the bones and their
  // origin. True by default.
  void SetShowParenthood(int parenthood);
  vtkGetMacro(ShowParenthood, int);

protected:
  vtkArmatureWidget();
  ~vtkArmatureWidget();

  // Callback class.
  //BTX
  class vtkArmatureWidgetCallback;
  //ETX
  vtkArmatureWidgetCallback* ArmatureWidgetCallback;

  // Bone Tree
  ArmatureTreeNodeVectorType* Bones;

  // Top level bone tree
  typedef std::vector<vtkBoneWidget*> BoneVectorType;
  typedef BoneVectorType::iterator BoneVectorIterator;
  std::vector<vtkBoneWidget*> TopLevelBones;

  // Bone Properties
  int BonesRepresentationType;
  int WidgetState;
  int AxesVisibility;
  int ShowParenthood;

  // Add all the necessaries observers to a bone
  void AddBoneObservers(vtkBoneWidget* bone);

  // Helper functions to propagate change
  void UpdateChildren(ArmatureTreeNode* parentNode);
  void UpdateChildrenWidgetStateToPose(ArmatureTreeNode* parentNode);
  void UpdateChildrenWidgetStateToRest(ArmatureTreeNode* parentNode);

  // Set the bone world to parent rest or pose transform correctly
  void SetBoneWorldToParentRestTransform(vtkBoneWidget* bone,
                                         vtkBoneWidget* parent);
  void SetBoneWorldToParentPoseTransform(vtkBoneWidget* bone,
                                         vtkBoneWidget* parent);

  ArmatureTreeNode* GetNode(vtkBoneWidget* bone);

  // Uodates a bone with all the current options of the vtkArmatureWidget.
  void UpdateBoneWithArmatureOptions(
    vtkBoneWidget* bone, vtkBoneWidget* parent);

  // Set the representation type for a given bone.
  void SetBoneRepresentation(vtkBoneWidget* bone, int representationType);

  // Create a new node
  ArmatureTreeNode* CreateNewMapElement(vtkBoneWidget* parent);

private:
  vtkArmatureWidget(const vtkArmatureWidget&);  //Not implemented
  void operator=(const vtkArmatureWidget&);  //Not implemented
};

#endif
