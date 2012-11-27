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
#include "vtkBoneRepresentation.h"
#include "vtkCylinderBoneRepresentation.h"
#include "vtkDoubleConeBoneRepresentation.h"

// VTK includes
#include <vtkCallbackCommand.h>
#include <vtkCommand.h>
#include <vtkCollection.h>
#include <vtkMath.h>
#include <vtkObjectFactory.h>
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
  // Init bones properties
  this->BonesRepresentation = 0;
  this->WidgetState = vtkArmatureWidget::Rest;
  this->ShowAxes = vtkBoneWidget::Hidden;
  this->ShowParenthood = true;
  this->ShouldResetPoseToRest = true;
  this->BonesAlwaysOnTop = 0;
}

//----------------------------------------------------------------------------
vtkArmatureWidget::~vtkArmatureWidget()
{
  // Delete all the bones in the map
  for (NodeIteratorType it = this->Bones->begin();
    it != this->Bones->end(); ++it)
    {
    this->RemoveBoneObservers((*it)->Bone);
    (*it)->Bone->Delete(); // Delete bone
    }

  if (this->BonesRepresentation)
    {
    this->BonesRepresentation->Delete();
    }

  this->ArmatureWidgetCallback->Delete();
}

//----------------------------------------------------------------------------
void vtkArmatureWidget::CreateDefaultRepresentation()
{
  vtkArmatureRepresentation* armatureRepresentation =
    this->GetArmatureRepresentation();
  if (!armatureRepresentation)
    {
    armatureRepresentation = vtkArmatureRepresentation::New();
    this->SetRepresentation(armatureRepresentation);
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
    if (!(*it)->Bone->GetBoneRepresentation())
      {
      this->SetBoneRepresentation((*it)->Bone);
      }
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

  this->BonesRepresentation = newRep;
  for (NodeIteratorType it = this->Bones->begin();
    it != this->Bones->end(); ++it)
    {
    this->SetBoneRepresentation((*it)->Bone);
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
    copiedRepresentation->ShallowCopy(this->BonesRepresentation);
    }

  bone->SetRepresentation(copiedRepresentation);
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
}

//----------------------------------------------------------------------------
void vtkArmatureWidget::RemoveBoneObservers(vtkBoneWidget* bone)
{
  bone->RemoveObservers(vtkBoneWidget::RestChangedEvent,
    this->ArmatureWidgetCallback);
  bone->RemoveObservers(vtkBoneWidget::PoseChangedEvent,
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
  if (!oldNode)
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

  // Remove old node
  this->RemoveNodeFromHierarchy(oldNode);

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
    tailBone->GetWorldTailRest(), newBoneName);
  ArmatureTreeNode* newNode = this->CreateAndAddNodeToHierarchy(newBone,
    headNode->Parent->Bone, headNode->HeadLinkedToParent);

  //Initialize new bone transforms
  this->SetBoneWorldToParentRestTransform(newBone, headNode->Parent->Bone);
  this->SetBoneWorldToParentPoseTransform(newBone, headNode->Parent->Bone);

  // Reparent tail node children
  newNode->AddChild(tailNode);

  // Then delete old bones. In the case of tailBone,
  // this automatically reparents its children to newNode.
  this->RemoveBone(tailBone);
  this->RemoveBone(headBone);

  // Add observers and invoke events.
  this->AddBoneObservers(newBone);
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
void vtkArmatureWidget::SetBonesAlwaysOnTop(int onTop)
{
  if (onTop == this->BonesAlwaysOnTop)
    {
    return;
    }
  this->BonesAlwaysOnTop = onTop;

  if (this->GetBonesRepresentation())
    {
    this->BonesRepresentation->SetAlwaysOnTop(this->BonesAlwaysOnTop);

    for (NodeIteratorType it = this->Bones->begin();
      it != this->Bones->end(); ++it)
      {
      (*it)->Bone->GetBoneRepresentation()->SetAlwaysOnTop(this->BonesAlwaysOnTop);
      }
    }

  this->Modified();
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
}

//----------------------------------------------------------------------------
void vtkArmatureWidget::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  os << indent << "Armature Widget " << this << "\n";
  // TO DO
}
