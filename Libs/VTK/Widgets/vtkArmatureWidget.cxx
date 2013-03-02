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

// Bender includes
#include "vtkArmatureRepresentation.h"
#include "vtkArmatureWidget.h"
#include "vtkBoneEnvelopeRepresentation.h"
#include "vtkBoneRepresentation.h"
#include "vtkCylinderBoneRepresentation.h"
#include "vtkDoubleConeBoneRepresentation.h"

// VTK includes
#include <vtkCallbackCommand.h>
#include <vtkCellData.h>
#include <vtkCommand.h>
#include <vtkCollection.h>
#include <vtkDoubleArray.h>
#include <vtkIdTypeArray.h>
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkSmartPointer.h>
#include <vtkWidgetCallbackMapper.h>
#include <vtkWidgetEvent.h>

// STD includes
#include <algorithm>

vtkStandardNewMacro(vtkArmatureWidget);

struct ArmatureTreeNode
{
  vtkBoneWidget* Bone;
  std::vector<ArmatureTreeNode*> Children;
  typedef std::vector<ArmatureTreeNode*>::iterator ChildrenNodeIteratorType;
  ArmatureTreeNode* Parent;
  bool HeadLinkedToParent;

  ArmatureTreeNode()
    {
    this->Bone = 0;
    this->Children.clear();
    this->Parent = 0;
    this->HeadLinkedToParent = false;
    }

  // Add the child to the parent's vector
  // and add the parent to the child's reference
  void AddChild(ArmatureTreeNode* child)
    {
    this->Children.push_back(child);
    child->Parent = this;
    }

  // Link child's children to parent and vise versa
  // and then remove the child.
  void RemoveChild(ArmatureTreeNode* child)
    {
    // Rebuild linkage
    for (ChildrenNodeIteratorType it = child->Children.begin();
      it != child->Children.end(); ++it)
      {
      this->Children.push_back(*it);
      (*it)->Parent = this;
      }

    // Remove child from parent
    ChildrenNodeIteratorType childPosition =
      std::find(this->Children.begin(), this->Children.end(), child);
    this->Children.erase(childPosition);
    }

  // Find a new root (if any), update children and returns the new root
  ArmatureTreeNode* RemoveRoot()
    {
    ArmatureTreeNode* newRoot = NULL;

    for (ChildrenNodeIteratorType it = this->Children.begin();
      it != this->Children.end(); ++it)
      {
      if (!newRoot)
        {
        (*it)->Parent = NULL;
        newRoot = *it;
        }
      else
        {
        (*it)->Parent = newRoot;
        newRoot->Children.push_back(*it);
        }
      }

    return newRoot;
    }
};

class ArmatureTreeNodeVectorType
  : public std::vector<ArmatureTreeNode*> {};
typedef ArmatureTreeNodeVectorType::iterator NodeIteratorType;
typedef std::vector<ArmatureTreeNode*> ChildrenVectorType;
typedef ChildrenVectorType::iterator ChildrenNodeIteratorType;

class vtkArmatureWidget::vtkArmatureWidgetCallback : public vtkCommand
{
public:
  static vtkArmatureWidgetCallback *New()
    {
    return new vtkArmatureWidgetCallback;
    }
  vtkArmatureWidgetCallback() { this->ArmatureWidget = 0; }
  virtual void Execute(vtkObject* caller, unsigned long eventId, void*)
    {
    switch (eventId)
      {
      case vtkBoneWidget::RestChangedEvent:
        {
        // Assume the cast isn't null. The only elements that should send
        // rest changed event should be vtkBoneWidget
        vtkBoneWidget* bone = vtkBoneWidget::SafeDownCast(caller);

        ArmatureTreeNode* node = ArmatureWidget->GetNode(bone);
        if (node && node->HeadLinkedToParent && node->Parent)
          {
          if (node->Bone->GetBoneSelected() != vtkBoneWidget::LineSelected)
            {
            double parentTail[3];
            node->Parent->Bone->GetWorldTailRest(parentTail);
            node->Bone->SetWorldHeadRest(parentTail);
            }
          if (node->Bone->GetBoneSelected() == vtkBoneWidget::LineSelected)
            {
            double head[3];
            node->Bone->GetWorldHeadRest(head);
            node->Parent->Bone->SetWorldTailRest(head);
            }
          }

        ArmatureWidget->UpdateChildrenWidgetStateToRest(node);

        break;
        }
      case vtkBoneWidget::PoseChangedEvent:
        {
        // Assume the cast isn't null. The only elements that should send
        // pose changed event should be vtkBoneWidget
        vtkBoneWidget* bone = vtkBoneWidget::SafeDownCast(caller);

        ArmatureTreeNode* node = ArmatureWidget->GetNode(bone);
        ArmatureWidget->UpdateChildrenWidgetStateToPose(node);

        break;
        }
      case vtkBoneWidget::SelectedStateChangedEvent:
        {
        // Assume the cast isn't null. The only elements that should send
        // selected state changed event should be vtkBoneWidget
        vtkBoneWidget* bone = vtkBoneWidget::SafeDownCast(caller);
        int newState = bone->GetBoneSelected();

        if (bone->GetWidgetState() == vtkBoneWidget::Rest)
          {
          if (newState == vtkBoneWidget::HeadSelected
            || newState == vtkBoneWidget::LineSelected)
            {
            ArmatureWidget->HighlightLinkedParentAndParentChildren(bone, 1);
            }

          if (newState == vtkBoneWidget::TailSelected
            || newState == vtkBoneWidget::LineSelected)
            {
            ArmatureWidget->HighlightLinkedChildren(bone, 1);
            }

          if (newState == vtkBoneWidget::NotSelected)
            {
            ArmatureWidget->HighlightLinkedParentAndParentChildren(bone, 0);
            ArmatureWidget->HighlightLinkedChildren(bone, 0);
            }
          }
        else if (bone->GetWidgetState() == vtkBoneWidget::Pose)
          {
          ArmatureWidget->HighlightAllChildren(ArmatureWidget->GetNode(bone),
            newState == vtkBoneWidget::TailSelected
            || newState == vtkBoneWidget::LineSelected);
          }

        break;
        }
      case vtkCommand::ModifiedEvent:
        {
        vtkBoneRepresentation* boneRep =
          vtkBoneRepresentation::SafeDownCast(caller);
        if (boneRep && boneRep == ArmatureWidget->GetBonesRepresentation())
          {
          ArmatureWidget->UpdateBonesRepresentation();
          }

        break;
        }
      }
    }

  vtkArmatureWidget *ArmatureWidget;
};

//----------------------------------------------------------------------------
vtkArmatureWidget::vtkArmatureWidget()
{
  // Init callback
  this->ArmatureWidgetCallback =
    vtkArmatureWidget::vtkArmatureWidgetCallback::New();
  this->ArmatureWidgetCallback->ArmatureWidget = this;

  // Init map and root
  this->Bones = new ArmatureTreeNodeVectorType;
  this->PolyData = vtkPolyData::New();
  vtkNew<vtkPoints> points;
  points->SetDataTypeToDouble();
  this->PolyData->SetPoints(points.GetPointer());
  this->PolyData->Allocate(100);
  vtkNew<vtkDoubleArray> transforms;
  transforms->SetNumberOfComponents(12);
  transforms->SetName("Transforms");
  this->PolyData->GetCellData()->AddArray(transforms.GetPointer());
  vtkNew<vtkDoubleArray> envelopeRadiuses;
  envelopeRadiuses->SetNumberOfComponents(1);
  envelopeRadiuses->SetName("EnvelopeRadiuses");
  this->PolyData->GetCellData()->AddArray(envelopeRadiuses.GetPointer());
  vtkNew<vtkIdTypeArray> parenthood;
  parenthood->SetName("Parenthood");
  this->PolyData->GetCellData()->AddArray(parenthood.GetPointer());

  // Init bones properties
  vtkBoneRepresentation* defaultRep = vtkBoneRepresentation::New();
  defaultRep->SetAlwaysOnTop(0);
  this->BonesRepresentation = defaultRep;
  defaultRep->AddObserver(vtkCommand::ModifiedEvent,
    this->ArmatureWidgetCallback, this->Priority);

  this->WidgetState = vtkArmatureWidget::Rest;
  this->ShowAxes = vtkBoneWidget::Hidden;
  this->ShowParenthood = true;
  this->ShouldResetPoseToRest = true;
}

//----------------------------------------------------------------------------
vtkArmatureWidget::~vtkArmatureWidget()
{
  this->PolyData->Delete();

  // Delete all the bones in the map
  for (NodeIteratorType it = this->Bones->begin();
    it != this->Bones->end(); ++it)
    {
    this->RemoveBoneObservers((*it)->Bone);
    (*it)->Bone->Delete(); // Delete bone
    }

  this->BonesRepresentation->RemoveObservers(
    vtkCommand::ModifiedEvent, this->ArmatureWidgetCallback);
  this->BonesRepresentation->Delete();
  this->ArmatureWidgetCallback->Delete();
}

//----------------------------------------------------------------------------
void vtkArmatureWidget::CreateDefaultRepresentation()
{
  if (! this->GetArmatureRepresentation())
    {
    vtkSmartPointer<vtkArmatureRepresentation> rep =
      vtkSmartPointer<vtkArmatureRepresentation>::New();
    this->SetRepresentation(rep);
    }
}

//----------------------------------------------------------------------------
void vtkArmatureWidget::SetEnabled(int enabling)
{
  if (enabling)
    {
    // Set interactor and renderer to all the bones
    for (NodeIteratorType it = this->Bones->begin();
      it != this->Bones->end(); ++it)
      {
      (*it)->Bone->SetInteractor(this->Interactor);
      (*it)->Bone->SetCurrentRenderer(this->CurrentRenderer);
      }
    }

  if (this->WidgetRep)
    {
    this->WidgetRep->SetVisibility(enabling);
    }

  // Enabled all the bones
  for (NodeIteratorType it = this->Bones->begin();
    it != this->Bones->end(); ++it)
    {
    (*it)->Bone->SetEnabled(enabling);
    }

  this->Superclass::SetEnabled(enabling);
}

//----------------------------------------------------------------------------
void vtkArmatureWidget::SetRepresentation(vtkArmatureRepresentation* r)
{
  this->Superclass::SetWidgetRepresentation(r);
}

//----------------------------------------------------------------------------
vtkArmatureRepresentation* vtkArmatureWidget::GetArmatureRepresentation()
{
  return vtkArmatureRepresentation::SafeDownCast(this->WidgetRep);
}

//----------------------------------------------------------------------------
void vtkArmatureWidget::SetProcessEvents(int pe)
{
  this->Superclass::SetProcessEvents(pe);

  for (NodeIteratorType it = this->Bones->begin();
    it != this->Bones->end(); ++it)
    {
    (*it)->Bone->SetProcessEvents(pe);
    }
}

//----------------------------------------------------------------------------
vtkBoneWidget* vtkArmatureWidget
::CreateBone(vtkBoneWidget* parent, const vtkStdString& name)
{
  vtkBoneWidget* newBone = vtkBoneWidget::New();
  newBone->SetName(name);
  this->UpdateBoneWithArmatureOptions(newBone, parent);
  return newBone;
}

//----------------------------------------------------------------------------
vtkBoneWidget* vtkArmatureWidget::CreateBone(vtkBoneWidget* parent,
                                             double tail[3],
                                             const vtkStdString& name)
{
  if (!parent)
    {
    vtkErrorMacro("The bone inserted with this method must have a parent.");
    return NULL;
    }

  vtkBoneWidget* newBone = vtkBoneWidget::New();
  newBone->SetName(name);
  this->UpdateBoneWithArmatureOptions(newBone, parent);
  newBone->SetWorldHeadRest(parent->GetWorldTailRest());
  newBone->SetWorldTailRest(tail);

  return newBone;
}

//----------------------------------------------------------------------------
vtkBoneWidget* vtkArmatureWidget::CreateBone(vtkBoneWidget* parent,
                                             double xTail,
                                             double yTail,
                                             double zTail,
                                             const vtkStdString& name)
{
  double tail[3];
  tail [0] = xTail;
  tail [1] = yTail;
  tail [2] = zTail;
  return this->CreateBone(parent, tail, name);
}

//----------------------------------------------------------------------------
void vtkArmatureWidget
::AddBone(vtkBoneWidget* bone, vtkBoneWidget* parent, bool linkedWithParent)
{
  bone->Register(this);

  // Create bone
  ArmatureTreeNode* newNode
    = this->CreateAndAddNodeToHierarchy(bone, parent, linkedWithParent);
  if (!newNode)
    {
    vtkErrorMacro("Failed to add the bone named "
      << bone->GetName() << " to the armature.");
    bone->Delete();
    return;
    }

  this->AddBoneObservers(bone);

  this->InvokeEvent(vtkArmatureWidget::BoneAdded, bone);
  this->Modified();
}

//----------------------------------------------------------------------------
ArmatureTreeNode* vtkArmatureWidget::CreateNewMapElement(vtkBoneWidget* parent)
{
  // Create new bone map element
  ArmatureTreeNode* newNode = new ArmatureTreeNode();

  // Take care of parent stuff
  newNode->Parent = this->GetNode(parent);

  if (newNode->Parent) // Add this to the parent children list
    {
    newNode->Parent->AddChild(newNode);
    }

  return newNode;
}

//----------------------------------------------------------------------------
void vtkArmatureWidget::SetWidgetState(int state)
{
  if (state == this->WidgetState)
    {
    return;
    }

  this->WidgetState = state;
  if (this->TopLevelBones.empty())
    {
    vtkErrorMacro("Could not find any root element !"
      " Cannot set armature state.");
    return;
    }

  switch (this->WidgetState)
    {
    case vtkArmatureWidget::Rest:
      {
      // No need for any recursivity here !
      for (NodeIteratorType it = this->Bones->begin();
        it != this->Bones->end(); ++it)
        {
        (*it)->Bone->SetWidgetStateToRest();
        }
      break;
      }
    case vtkArmatureWidget::Pose:
      {
      // could be smart and check if something has changed since the last
      // state switch and only udpate if it's the case
      for (BoneVectorIterator it = this->TopLevelBones.begin();
        it != this->TopLevelBones.end(); ++it)
        {
        this->SetBoneWorldToParentPoseTransform(*it, NULL);
        }

      for (NodeIteratorType it = this->Bones->begin();
        it != this->Bones->end(); ++it)
        {
        (*it)->Bone->SetWidgetStateToPose();
        }

      this->ShouldResetPoseToRest = false;
      break;
      }
    default:
      {
      vtkErrorMacro("Unknown state: "
        <<this->WidgetState<<"/n  ->State unchanged");
      break;
      }
    }

  this->Modified();
}

//----------------------------------------------------------------------------
void vtkArmatureWidget::SetShowAxes(int show)
{
  if (show == this->ShowAxes)
    {
    return;
    }

  this->ShowAxes = show;
  for (NodeIteratorType it = this->Bones->begin();
    it != this->Bones->end(); ++it)
    {
    (*it)->Bone->SetShowAxes(this->ShowAxes);
    }

  this->Modified();
}

//----------------------------------------------------------------------------
void vtkArmatureWidget::SetShowParenthood(int parenthood)
{
  if (this->ShowParenthood == parenthood)
    {
    return;
    }

  this->ShowParenthood = parenthood;
  for (NodeIteratorType it = this->Bones->begin();
    it != this->Bones->end(); ++it)
    {
    (*it)->Bone->SetShowParenthood(this->ShowParenthood);
    }

  this->Modified();
}

//----------------------------------------------------------------------------
void vtkArmatureWidget::SetBonesRepresentation(vtkBoneRepresentation* newRep)
{
  if (!newRep || newRep == this->BonesRepresentation)
    {
    return;
    }

  if (newRep->GetClassName() == this->BonesRepresentation->GetClassName())
    {
    this->BonesRepresentation->DeepCopyRepresentationOnly(newRep);
    }
  else
    {
    this->BonesRepresentation->RemoveObservers(
      vtkCommand::ModifiedEvent, this->ArmatureWidgetCallback);
    this->BonesRepresentation->Delete();

    this->BonesRepresentation = newRep;
    this->BonesRepresentation->Register(this);
    this->BonesRepresentation->AddObserver(vtkCommand::ModifiedEvent,
      this->ArmatureWidgetCallback, this->Priority);

    this->UpdateBonesRepresentation();
    }

  this->Modified();
}

//----------------------------------------------------------------------------
vtkBoneRepresentation* vtkArmatureWidget::GetBonesRepresentation()
{
  return this->BonesRepresentation;
}

//----------------------------------------------------------------------------
void vtkArmatureWidget::SetBoneRepresentation(vtkBoneWidget* bone)
{
  vtkBoneRepresentation* copiedRepresentation = 0;
  if (this->BonesRepresentation)
    {
    copiedRepresentation = this->BonesRepresentation->NewInstance();
    copiedRepresentation->DeepCopyRepresentationOnly(
      this->BonesRepresentation);
    }

  bone->SetRepresentation(copiedRepresentation);
  if (copiedRepresentation)
    {
    copiedRepresentation->Delete();
    }
}

//----------------------------------------------------------------------------
void vtkArmatureWidget::UpdateBonesRepresentation()
{
  if (!this->BonesRepresentation)
    {
    return;
    }

  for (NodeIteratorType it = this->Bones->begin();
    it != this->Bones->end(); ++it)
    {
    vtkBoneRepresentation* boneRep = (*it)->Bone->GetBoneRepresentation();
    if (!boneRep
      || boneRep->GetClassName() != this->BonesRepresentation->GetClassName())
      {
      // The bone does not have a rep yet
      // or it's a different kind
      this->SetBoneRepresentation((*it)->Bone);
      }
    else
      {
      boneRep->DeepCopyRepresentationOnly(this->BonesRepresentation);
      }
    }
}

//----------------------------------------------------------------------------
bool vtkArmatureWidget::HasBone(vtkBoneWidget* bone)
{
  return this->GetNode(bone) != 0;
}

//----------------------------------------------------------------------------
vtkBoneWidget* vtkArmatureWidget::GetBoneParent(vtkBoneWidget* bone)
{
  ArmatureTreeNode* node = this->GetNode(bone);
  if (node && node->Parent)
    {
    return node->Parent->Bone;
    }

  return NULL;
}

//----------------------------------------------------------------------------
bool vtkArmatureWidget
::IsBoneDirectParent(vtkBoneWidget* bone, vtkBoneWidget* parent)
{
  return this->GetBoneParent(bone) == parent;
}

//----------------------------------------------------------------------------
bool vtkArmatureWidget
::IsBoneParent(vtkBoneWidget* bone, vtkBoneWidget* parent)
{
  if (!parent)
    {
    return true; // A bone should always have a root parent
    }

  ArmatureTreeNode* node = this->GetNode(bone);
  if (!node || !node->Parent)
    {
    return false;
    }

  while (node)
    {
    if (node->Parent->Bone == parent)
      {
      return true;
      }
    node = node->Parent;
    }

  return false;
}

//----------------------------------------------------------------------------
vtkCollection* vtkArmatureWidget::FindBoneChildren(vtkBoneWidget* parent)
{
  vtkCollection* children = vtkCollection::New();

  ArmatureTreeNode* node = this->GetNode(parent);
  if (node)
    {
    for (NodeIteratorType it = node->Children.begin();
      it != node->Children.end(); ++it)
      {
      children->AddItem((*it)->Bone);
      }
    }

  return children;
}

//----------------------------------------------------------------------------
bool vtkArmatureWidget::RemoveBone(vtkBoneWidget* bone)
{
  for (NodeIteratorType it = this->Bones->begin();
    it != this->Bones->end(); ++it)
    {
    if ((*it)->Bone == bone)
      {
      this->RemoveNodeFromHierarchy(it - this->Bones->begin());

      this->RemoveBoneObservers((*it)->Bone);
      (*it)->Bone->Delete();
      this->Bones->erase(it);

      this->InvokeEvent(vtkArmatureWidget::BoneRemoved, bone);
      this->Modified();
      return true;
      }
    }

  return false;
}

//----------------------------------------------------------------------------
vtkBoneWidget* vtkArmatureWidget::GetBoneByName(const vtkStdString& name)
{
  for (NodeIteratorType it = this->Bones->begin();
    it != this->Bones->end(); ++it)
    {
    if ((*it)->Bone->GetName() == name)
      {
      return (*it)->Bone;
      }
    }
  return NULL;
}

//----------------------------------------------------------------------------
bool vtkArmatureWidget::GetBoneLinkedWithParent(vtkBoneWidget* bone)
{
  ArmatureTreeNode* node = this->GetNode(bone);
  if (node)
    {
    return node->HeadLinkedToParent;
    }
  return false;
}

//----------------------------------------------------------------------------
void vtkArmatureWidget
::SetBoneLinkedWithParent(vtkBoneWidget* bone, bool linked)
{
  ArmatureTreeNode* node = this->GetNode(bone);
  if (node && node->HeadLinkedToParent != linked)
    {
    node->HeadLinkedToParent = linked;
    if (node->Parent && linked)
      {
      node->Bone->SetWorldHeadRest(node->Parent->Bone->GetWorldTailRest());
      }
    }

  this->Modified();
}

//----------------------------------------------------------------------------
void vtkArmatureWidget::AddBoneObservers(vtkBoneWidget* bone)
{
  bone->AddObserver(vtkBoneWidget::RestChangedEvent,
    this->ArmatureWidgetCallback, this->Priority);
  bone->AddObserver(vtkBoneWidget::PoseChangedEvent,
    this->ArmatureWidgetCallback, this->Priority);
  bone->AddObserver(vtkBoneWidget::SelectedStateChangedEvent,
    this->ArmatureWidgetCallback, this->Priority);
}

//----------------------------------------------------------------------------
void vtkArmatureWidget::RemoveBoneObservers(vtkBoneWidget* bone)
{
  bone->RemoveObservers(vtkBoneWidget::RestChangedEvent,
    this->ArmatureWidgetCallback);
  bone->RemoveObservers(vtkBoneWidget::PoseChangedEvent,
    this->ArmatureWidgetCallback);
  bone->AddObserver(vtkBoneWidget::SelectedStateChangedEvent,
    this->ArmatureWidgetCallback);
}

//----------------------------------------------------------------------------
void vtkArmatureWidget
::UpdateBoneWithArmatureOptions(vtkBoneWidget* bone, vtkBoneWidget* parent)
{
  this->SetBoneRepresentation(bone);
  bone->SetShowAxes(this->ShowAxes);
  bone->SetShowParenthood(this->ShowParenthood);

  if (this->WidgetState == vtkArmatureWidget::Rest)
    {
    this->SetBoneWorldToParentRestTransform(bone, parent);
    bone->SetWidgetStateToRest();
    }
  else
    {
    if (this->ShouldResetPoseToRest)
      {
      bone->ResetPoseToRest();
      }

    this->SetBoneWorldToParentPoseTransform(bone, parent);
    bone->SetWidgetStateToPose();
    }

}

//----------------------------------------------------------------------------
void vtkArmatureWidget
::ReparentBone(vtkBoneWidget* bone, vtkBoneWidget* newParent)
{
  if (!bone)
    {
    return;
    }

  ArmatureTreeNode* oldNode = this->GetNode(bone);

  // Reparent if:
  // - The bone belongs to the armature
  bool shouldReparent = oldNode != 0;
  // - The new parent belongs to the armature or is null (reparent to root)
  shouldReparent &= newParent ? this->HasBone(newParent) : true;
  // - The bone's parent is different than the new parent
  shouldReparent &= oldNode->Parent ?
    oldNode->Parent->Bone != newParent : newParent != 0;
  if (!shouldReparent)
    {
    return;
    }

  // Create new node with bone and NEW parent
  ArmatureTreeNode* newNode = this->CreateAndAddNodeToHierarchy(
    bone, newParent, oldNode->HeadLinkedToParent);
  if (!newNode)
    {
    vtkErrorMacro("Failed to reparent the bone named "<< bone->GetName());
    return;
    }

  // Add the children of the old node to he new node
  for (ChildrenNodeIteratorType it = oldNode->Children.begin();
    it != oldNode->Children.end(); ++it)
    {
    newNode->AddChild(*it);
    }
  // Clear chidren
  oldNode->Children.clear();

  // Remove old node
  this->RemoveNodeFromHierarchy(oldNode);
  this->Bones->erase(
    std::find(this->Bones->begin(), this->Bones->end(), oldNode));

  // Update bone
  this->SetBoneWorldToParentRestTransform(bone, newParent);
  this->SetBoneWorldToParentPoseTransform(bone, newParent);
  this->InvokeEvent(vtkArmatureWidget::BoneReparented, bone);
  this->Modified();
}

//----------------------------------------------------------------------------
vtkBoneWidget* vtkArmatureWidget
::MergeBones(vtkBoneWidget* headBone, vtkBoneWidget* tailBone)
{
  if (!headBone || !tailBone)
    {
    return NULL;
    }

  ArmatureTreeNode* headNode = this->GetNode(headBone);
  ArmatureTreeNode* tailNode = this->GetNode(tailBone);

  if (!headNode || !tailNode)
    {
    return NULL;
    }

  if (! this->IsBoneParent(tailBone, headBone))
    {
    vtkErrorMacro("Cannot merge bones that are not parented");
    return NULL;
    }

  std::string newBoneName = headBone->GetName();
  newBoneName += " + ";
  newBoneName += tailBone->GetName();

  vtkBoneWidget* newBone = this->CreateBone(headNode->Parent->Bone,
    headBone->GetWorldTailRest(), newBoneName);
  ArmatureTreeNode* newNode = this->CreateAndAddNodeToHierarchy(newBone,
    headNode->Parent->Bone, headNode->HeadLinkedToParent);

  //Initialize new bone transforms
  this->SetBoneWorldToParentRestTransform(newBone, headNode->Parent->Bone);
  this->SetBoneWorldToParentPoseTransform(newBone, headNode->Parent->Bone);

  // Reparent tail node children
  this->ReparentBone(tailBone, newBone);

  // Then delete old bones. In the case of tailBone,
  // this automatically reparents its children to newNode.
  this->RemoveBone(tailBone);
  this->RemoveBone(headBone);

  // Add observers
  this->AddBoneObservers(newBone);
  // Adjust postion
  newBone->SetWorldTailRest(tailBone->GetWorldTailRest());
  // Invoke events.
  this->InvokeEvent(vtkArmatureWidget::BoneMerged, newBone);
  this->Modified();

  return newBone;
}

//----------------------------------------------------------------------------
void vtkArmatureWidget::ResetPoseToRest()
{
  int oldState = this->WidgetState;
  this->ShouldResetPoseToRest = true;

  if (this->WidgetState == vtkArmatureWidget::Pose)
    {
    this->SetWidgetState(vtkArmatureWidget::Rest);
    }

  this->SetWidgetState(vtkArmatureWidget::Pose);

  if(oldState == vtkArmatureWidget::Rest)
    {
    this->SetWidgetState(vtkArmatureWidget::Rest);
    }
}

//----------------------------------------------------------------------------
void vtkArmatureWidget
::SetBoneWorldToParentTransform(vtkBoneWidget* bone, vtkBoneWidget* parent)
{
  if (bone->GetWidgetState() == vtkArmatureWidget::Rest)
    {
    this->SetBoneWorldToParentRestTransform(bone, parent);
    }
  else if (bone->GetWidgetState()== vtkBoneWidget::Pose)
    {
    this->SetBoneWorldToParentPoseTransform(bone, parent);
    }
}

//----------------------------------------------------------------------------
void vtkArmatureWidget
::SetBoneWorldToParentRestTransform(vtkBoneWidget* bone, vtkBoneWidget* parent)
{
  double parentWorldToBoneRestRotation[4] = {1.0, 0.0, 0.0, 0.0};
  double parentWorldToBoneRestTranslation[3] = {0.0, 0.0, 0.0};
  if (parent) // For all non-root elements
    {
    parent->GetWorldToBoneRestRotation(
      parentWorldToBoneRestRotation);
    parent->GetWorldToBoneTailRestTranslation(
      parentWorldToBoneRestTranslation);
    }

  bone->SetWorldToParentRestRotationAndTranslation(
    parentWorldToBoneRestRotation, parentWorldToBoneRestTranslation);
}

//----------------------------------------------------------------------------
void vtkArmatureWidget
::SetBoneWorldToParentPoseTransform(vtkBoneWidget* bone, vtkBoneWidget* parent)
{
  if (this->ShouldResetPoseToRest)
    {
    bone->ResetPoseToRest();
    return;
    }

  double parentWorldToBonePoseRotation[4] = {1.0, 0.0, 0.0, 0.0};
  double parentWorldToBonePoseTranslation[3] = {0.0, 0.0, 0.0};
  if (parent) // For all non-root elements
    {
    parent->GetWorldToBonePoseRotation(
      parentWorldToBonePoseRotation);
    parent->GetWorldToBoneTailPoseTranslation(
      parentWorldToBonePoseTranslation);
    }

  bone->SetWorldToParentPoseRotationAndTranslation(
    parentWorldToBonePoseRotation, parentWorldToBonePoseTranslation);
}

//----------------------------------------------------------------------------
ArmatureTreeNode* vtkArmatureWidget::CreateAndAddNodeToHierarchy(
  vtkBoneWidget* bone,
  vtkBoneWidget* newParent,
  bool linkedWithParent)
{
  ArmatureTreeNode* newNode = this->CreateNewMapElement(newParent);
  newNode->Bone = bone;
  if (! newParent)
    {
    this->TopLevelBones.push_back(bone);
    }
  this->Bones->push_back(newNode);

  bool shouldLinkWithParent = (newParent && linkedWithParent);
  newNode->HeadLinkedToParent = shouldLinkWithParent;
  if (shouldLinkWithParent)
    {
    bone->SetWorldHeadRest(newParent->GetWorldTailRest());
    }

  return newNode;
}

//----------------------------------------------------------------------------
void vtkArmatureWidget
::RemoveNodeFromHierarchy(int nodePosition)
{
  if (nodePosition < 0
    || nodePosition > static_cast<int>(this->Bones->size()))
    {
    return;
    }

  this->RemoveNodeFromHierarchy(*(this->Bones->begin() + nodePosition));
}

//----------------------------------------------------------------------------
void vtkArmatureWidget
::RemoveNodeFromHierarchy(ArmatureTreeNode* node)
{
  if (! node)
    {
    return;
    }

  if (node->Parent) // Stitch children to the father
    {
    node->Parent->RemoveChild(node);
    this->UpdateChildren(node->Parent);
    }
  else // It was root
    {
    ArmatureTreeNode* replacementRoot = node->RemoveRoot();
    if (replacementRoot)
      {
      this->TopLevelBones.push_back(replacementRoot->Bone);
      this->UpdateChildren(replacementRoot);
      }

    BoneVectorIterator rootsIterator
      = std::find(this->TopLevelBones.begin(),
        this->TopLevelBones.end(), node->Bone);
    if (rootsIterator != this->TopLevelBones.end())
      {
      this->TopLevelBones.erase(rootsIterator);
      }
    }
}

//----------------------------------------------------------------------------
ArmatureTreeNode* vtkArmatureWidget::GetNode(vtkBoneWidget* bone)
{
  if (!bone) // More Effecient
    {
    return NULL;
    }

  for (NodeIteratorType it = this->Bones->begin();
    it != this->Bones->end(); ++it)
    {
    if ((*it)->Bone == bone)
      {
      return *it;
      }
    }
  return NULL;
}

//----------------------------------------------------------------------------
void vtkArmatureWidget::UpdatePolyData()
{
  this->PolyData->GetPoints()->Reset();
  vtkDoubleArray* transforms = vtkDoubleArray::SafeDownCast(
    this->PolyData->GetCellData()->GetArray("Transforms"));
  transforms->Reset();
  vtkDoubleArray* envelopeRadiuses = vtkDoubleArray::SafeDownCast(
    this->PolyData->GetCellData()->GetArray("EnvelopeRadiuses"));
  envelopeRadiuses->Reset();
  vtkIdTypeArray* parenthood = vtkIdTypeArray::SafeDownCast(
    this->PolyData->GetCellData()->GetArray("Parenthood"));
  parenthood->Reset();
  this->PolyData->Reset();
  for (NodeIteratorType it = this->Bones->begin();
    it != this->Bones->end(); ++it)
    {
    // Transforms
    vtkIdType indexes[2];
    indexes[0] = this->PolyData->GetPoints()->InsertNextPoint(
      (*it)->Bone->GetWorldHeadRest());
    indexes[1] = this->PolyData->GetPoints()->InsertNextPoint(
      (*it)->Bone->GetWorldTailRest());
    this->PolyData->InsertNextCell(VTK_LINE, 2, indexes);

    double translation[3];
    vtkMath::Subtract((*it)->Bone->GetWorldHeadPose(), (*it)->Bone->GetWorldHeadRest(), translation);
    double localTailRest[3];
    vtkMath::Subtract((*it)->Bone->GetWorldTailRest(), (*it)->Bone->GetWorldHeadRest(), localTailRest);
    double localTailPose[3];
    vtkMath::Subtract((*it)->Bone->GetWorldTailPose(), (*it)->Bone->GetWorldHeadPose(), localTailPose);
    vtkMath::Subtract((*it)->Bone->GetWorldHeadPose(), (*it)->Bone->GetWorldHeadRest(), translation);

    double rotation[3][3];
    this->ComputeTransform(localTailRest, localTailPose, rotation);
    double transform[4][3];
    memcpy(transform, rotation, 3*3*sizeof(double));
    memcpy(transform[3], translation, 3*sizeof(double));
    transforms->InsertNextTuple(transform[0]);

    // Envelope radiuses
    double radius = 0.0;
    vtkBoneRepresentation* boneRep = (*it)->Bone->GetBoneRepresentation();
    if (boneRep)
      {
      radius = boneRep->GetEnvelope()->GetRadius();
      }
    envelopeRadiuses->InsertNextValue(radius);

    // Parenthood
    if ((*it)->Parent)
      {
      for (NodeIteratorType parentIt = this->Bones->begin();
        parentIt != this->Bones->end(); ++parentIt)
        {
        if ((*parentIt)->Bone == (*it)->Parent->Bone)
          {
          parenthood->InsertNextValue(parentIt - this->Bones->begin());
          break;
          }
        }
      }
    else // Root
      {
      parenthood->InsertNextValue(-1);
      }
    }

  this->PolyData->Modified();
}

//----------------------------------------------------------------------------
void vtkArmatureWidget::ComputeTransform(double start[3], double end[3], double mat[3][3])
{
  double startNormalized[3] = {start[0], start[1], start[2]};
  double startNorm = vtkMath::Normalize(startNormalized);
  double endNormalized[3] = {end[0], end[1], end[2]};
  double endNorm = vtkMath::Normalize(endNormalized);

  double rotationAxis[3] = {0., 0., 0.};
  vtkMath::Cross(startNormalized, endNormalized, rotationAxis);
  if (rotationAxis[0] == 0. && rotationAxis[1] == 0. && rotationAxis[2] == 0.)
    {
    double dummy[3];
    vtkMath::Perpendiculars(startNormalized, rotationAxis, dummy, 0.);
    }
  if (rotationAxis[0] == 0. && rotationAxis[1] == 0. && rotationAxis[2] == 0.)
    {
    vtkMath::Identity3x3(mat);
    }
  else
    {
    vtkMath::Normalize(rotationAxis);
    double angle = vtkArmatureWidget::ComputeAngle(startNormalized, endNormalized);
    vtkArmatureWidget::ComputeAxisAngleMatrix(rotationAxis, angle, mat);
    }
  double scaleMatrix[3][3] = {1.,0.,0.,0.,1.,0.,0.,0.,1.};
  scaleMatrix[0][0] = scaleMatrix[1][1] = scaleMatrix[2][2] = endNorm / startNorm;
  vtkMath::Multiply3x3(mat, scaleMatrix, mat);
  //vtkMath::Multiply3x3(scaleMatrix, transform, transform);
}

//-----------------------------------------------------------------------------
double vtkArmatureWidget::ComputeAngle(double v1[3], double v2[3])
{
  double dot = vtkMath::Dot(v1, v2);
  dot = std::min(dot, 1.);
  double angle = acos(dot);
  return angle;
}

//-----------------------------------------------------------------------------
void vtkArmatureWidget::ComputeAxisAngleMatrix(double axis[3], double angle, double mat[3][3])
{
  /* rotation of angle radials around axis */
  double vx, vx2, vy, vy2, vz, vz2, co, si;

  vx = axis[0];
  vy = axis[1];
  vz = axis[2];
  vx2 = vx * vx;
  vy2 = vy * vy;
  vz2 = vz * vz;
  co = cos(angle);
  si = sin(angle);

  mat[0][0] = vx2 + co * (1.0f - vx2);
  mat[0][1] = vx * vy * (1.0f - co) + vz * si;
  mat[0][2] = vz * vx * (1.0f - co) - vy * si;
  mat[1][0] = vx * vy * (1.0f - co) - vz * si;
  mat[1][1] = vy2 + co * (1.0f - vy2);
  mat[1][2] = vy * vz * (1.0f - co) + vx * si;
  mat[2][0] = vz * vx * (1.0f - co) + vy * si;
  mat[2][1] = vy * vz * (1.0f - co) - vx * si;
  mat[2][2] = vz2 + co * (1.0f - vz2);
}

//----------------------------------------------------------------------------
void vtkArmatureWidget::UpdateChildren(ArmatureTreeNode* parentNode)
{
  if (!parentNode)
    {
    return;
    }

  if (parentNode->Bone->GetWidgetState() == vtkBoneWidget::Rest)
    {
    this->UpdateChildrenWidgetStateToRest(parentNode);
    }
  else if (parentNode->Bone->GetWidgetState()== vtkBoneWidget::Pose)
    {
    this->UpdateChildrenWidgetStateToPose(parentNode);
    }
}

//----------------------------------------------------------------------------
void vtkArmatureWidget
::UpdateChildrenWidgetStateToRest(ArmatureTreeNode* parentNode)
{
  if (!parentNode)
    {
    return;
    }

  for (ChildrenNodeIteratorType it = parentNode->Children.begin();
    it != parentNode->Children.end(); ++it)
    {
    this->SetBoneWorldToParentRestTransform((*it)->Bone, parentNode->Bone);

    if ((*it)->HeadLinkedToParent)
      {
      (*it)->Bone->SetWorldHeadRest(parentNode->Bone->GetWorldTailRest());
      }
    }
  this->UpdatePolyData();
}

//----------------------------------------------------------------------------
void vtkArmatureWidget
::UpdateChildrenWidgetStateToPose(ArmatureTreeNode* parentNode)
{
  if (!parentNode)
    {
    return;
    }

  for (ChildrenNodeIteratorType it = parentNode->Children.begin();
    it != parentNode->Children.end(); ++it)
    {
    this->SetBoneWorldToParentPoseTransform((*it)->Bone, parentNode->Bone);
    }
  this->UpdatePolyData();
}

//----------------------------------------------------------------------------
void vtkArmatureWidget
::HighlightLinkedParentAndParentChildren(vtkBoneWidget* bone, int highlight)
{
  ArmatureTreeNode* node = this->GetNode(bone);
  if (!node || !node->Parent)
    {
    return;
    }

  if (node->HeadLinkedToParent)
    {
    if (node->Parent->Bone->GetBoneRepresentation())
      {
      node->Parent->Bone->GetBoneRepresentation()->Highlight(highlight);
      }
    this->HighlightLinkedChildren(node->Parent, highlight);
    }
}

//----------------------------------------------------------------------------
void vtkArmatureWidget
::HighlightLinkedChildren(vtkBoneWidget* bone, int highlight)
{
  this->HighlightLinkedChildren(this->GetNode(bone), highlight);
}

//----------------------------------------------------------------------------
void vtkArmatureWidget
::HighlightLinkedChildren(ArmatureTreeNode* node, int highlight)
{
  if (! node)
    {
    return;
    }

  for (NodeIteratorType it = node->Children.begin();
    it != node->Children.end(); ++it)
    {
    if ((*it)->HeadLinkedToParent && (*it)->Bone->GetBoneRepresentation())
      {
      (*it)->Bone->GetBoneRepresentation()->Highlight(highlight);
      }
    }
}

//----------------------------------------------------------------------------
void vtkArmatureWidget
::HighlightAllChildren(ArmatureTreeNode* node, int highlight)
{
  if (!node)
    {
    return;
    }

  for (NodeIteratorType it = node->Children.begin();
    it != node->Children.end(); ++it)
    {
    if ((*it)->Bone->GetBoneRepresentation())
      {
      (*it)->Bone->GetBoneRepresentation()->Highlight(highlight);
      }

    this->HighlightAllChildren(*it, highlight);
    }
}

//----------------------------------------------------------------------------
void vtkArmatureWidget::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  os << indent << "Armature Widget " << this << "\n";
  // TO DO
}

//----------------------------------------------------------------------------
void vtkArmatureWidget::Modified()
{
  this->UpdatePolyData();
  this->Superclass::Modified();
}
