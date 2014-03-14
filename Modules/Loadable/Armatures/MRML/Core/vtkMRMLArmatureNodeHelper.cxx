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
#include "vtkMRMLArmatureNode.h"
#include "vtkMRMLArmatureNodeHelper.h"
#include "vtkMRMLBoneNode.h"

#include <vtkQuaternion.h>

// Slicer includes
#include <vtkMRMLHierarchyNode.h>

// VTK includes
#include <vtkCollection.h>
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkTransform.h>

// STD includes

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkMRMLArmatureNodeHelper)

//----------------------------------------------------------------------------
double vtkMRMLArmatureNodeHelper::GetBoneSize(vtkMRMLBoneNode* bone)
{
  if (!bone)
    {
    return 0.0;
    }

  double localTail[3];
  bone->GetLocalTailRest(localTail);
  return vtkMath::Norm(localTail);
}

//----------------------------------------------------------------------------
double vtkMRMLArmatureNodeHelper::GetBoneSize(vtkBoneWidget* bone)
{
  if (!bone)
    {
    return 0.0;
    }

  double localTail[3];
  bone->GetLocalTailRest(localTail);
  return vtkMath::Norm(localTail);
}

//----------------------------------------------------------------------------
bool vtkMRMLArmatureNodeHelper::ResizeArmature(vtkMRMLArmatureNode* armature,
                                               vtkCollection* bones,
                                               std::vector<double>& sizes,
                                               double origin[3],
                                               std::vector<double>* oldSizes)
{
 // In the general case, bones are like this:
  // ParentTail  -  -  -  -  -  -  Head------Tail
  //           <------Offset------><---Length--->
  //           <--------------Size-------------->
  // We need to scale the target bone size to the size of the anim bone.
  if (!bones || bones->GetNumberOfItems() < 1)
    {
    std::cerr<<"There are no bones in the given armature !"<<std::endl;
    return false;
    }
  if (bones->GetNumberOfItems() != static_cast<int>(sizes.size()))
    {
    std::cerr<<"There should be as many sizes as bone nodes !"<<std::endl;
    return false;
    }

  vtkNew<vtkCollection> roots;
  armature->GetDirectChildren(roots.GetPointer());
  if (roots->GetNumberOfItems() != 1)
    {
    std::cerr<<"Cannot resize armature that don't have exactly one root."
      <<std::endl;
    return false;
    }

  int oldState = armature->SetWidgetState(vtkMRMLArmatureNode::Rest);

  // First move target root to anim root
  vtkMRMLBoneNode* root =
    vtkMRMLBoneNode::SafeDownCast(roots->GetItemAsObject(0));

  double rootHead[3], rootTranslation[3];
  root->GetWorldHeadRest(rootHead);
  vtkMath::Subtract(origin, rootHead, rootTranslation);
  armature->Translate(rootTranslation);

  for (int i = 0; i < bones->GetNumberOfItems(); ++i)
    {
    vtkMRMLBoneNode* bone =
      vtkMRMLBoneNode::SafeDownCast(bones->GetItemAsObject(i));

    // Before anything, save bone size and length
    double size = GetBoneSize(bone);
    if (oldSizes)
      {
      oldSizes->push_back(size);
      }

    // Get bone children
    vtkNew<vtkCollection> directBoneChildren;
    vtkMRMLAnnotationHierarchyNode* hierarchyNode =
      vtkMRMLAnnotationHierarchyNode::SafeDownCast(
        vtkMRMLHierarchyNode::GetAssociatedHierarchyNode(
          bone->GetScene(), bone->GetID()));
    hierarchyNode->GetDirectChildren(directBoneChildren.GetPointer());

    // Unlink from bone children
    std::vector<bool> wasLinked;
    for (int j = 0; j < directBoneChildren->GetNumberOfItems(); ++j)
      {
      vtkMRMLBoneNode* child =
        vtkMRMLBoneNode::SafeDownCast(directBoneChildren->GetItemAsObject(j));
      wasLinked.push_back(child->GetBoneLinkedWithParent());
      child->SetBoneLinkedWithParent(false);
      }

    // Save old tail position
    double oldTargetTail[3];
    bone->GetWorldTailRest(oldTargetTail);

    // Resize bone
    //\todo fixme, root isn't necessary the only top level bone
    bool isTopLevel = (bone == root);
    double newSize = sizes[i];
    if (bone->GetBoneLinkedWithParent() || isTopLevel)
      {
      // Easy case, the bone is linked to its parent (or root).
      // We can just change its length to the new size
      bone->SetLength(newSize);
      }
    else
      {
      // Harder case, the bone isn't linked to its parent (nor root).
      // We need to scale its length and its offset.

      // Get the line vector between the parent's head and head
      double lineVect[3], parentHead[3], head[3];
      bone->GetWorldToParentRestTranslation(parentHead);
      bone->GetWorldHeadRest(head);
      vtkMath::Subtract(head, parentHead, lineVect);
      vtkMath::Normalize(lineVect);

      // Change length
      bone->SetLength((newSize * bone->GetLength()) / size);

      // Change target offset
      vtkMath::MultiplyScalar(lineVect, newSize - bone->GetLength());
      vtkMath::Add(lineVect, parentHead, head);
      bone->SetWorldHeadRest(head);
      }

    // Get translation
    double newTail[3];
    bone->GetWorldTailRest(newTail);
    double translation[3];
    vtkMath::Subtract(newTail, oldTargetTail, translation);

    // Move Bone children
    vtkNew<vtkCollection> boneChildren;
    hierarchyNode->GetChildren(boneChildren.GetPointer(), -1);
    for (int j = 0; j < boneChildren->GetNumberOfItems(); ++j)
      {
      vtkMRMLBoneNode* child =
        vtkMRMLBoneNode::SafeDownCast(boneChildren->GetItemAsObject(j));
      child->Translate(translation);
      }

    // Re-link direct children
    for (int j = 0; j < directBoneChildren->GetNumberOfItems(); ++j)
      {
      vtkMRMLBoneNode* child =
        vtkMRMLBoneNode::SafeDownCast(directBoneChildren->GetItemAsObject(j));
      child->SetBoneLinkedWithParent(wasLinked[j]);
      }

    }

  armature->SetWidgetState(oldState);
  return true;
}

//----------------------------------------------------------------------------
bool vtkMRMLArmatureNodeHelper
::AnimateArmature(vtkMRMLArmatureNode* targetArmature,
                  vtkArmatureWidget* animationarmature)
{
  vtkNew<vtkCollection> roots;
  targetArmature->GetDirectChildren(roots.GetPointer());
  if (roots->GetNumberOfItems() != 1)
    {
    std::cerr<<"Cannot resize armature that don't have exactly one root."
      <<std::endl;
    return false;
    }

  vtkNew<vtkCollection> targetBones;
  targetArmature->GetAllBones(targetBones.GetPointer());

  // Get correspondence
  CorrespondenceList correspondence;
  if (!vtkMRMLArmatureNodeHelper::GetCorrespondence(
    targetBones.GetPointer(), animationarmature, correspondence))
    {
    return false;
    }

  targetArmature->ResetPoseMode();
  int oldState = targetArmature->SetWidgetState(vtkMRMLArmatureNode::Pose);
  // Prepare resizing
  std::vector<double> animationSizes;
  for (CorrespondenceList::iterator it = correspondence.begin();
    it < correspondence.end(); ++it)
    {
    animationSizes.push_back(
      vtkMRMLArmatureNodeHelper::GetBoneSize(it->second));
    }

  // Get anim and target origin

  // First move target root to anim root
  vtkMRMLBoneNode* targetRoot =
    vtkMRMLBoneNode::SafeDownCast(roots->GetItemAsObject(0));

  double targetRootHead[3], animRootHead[3];
  targetRoot->GetWorldHeadRest(targetRootHead);
  for (CorrespondenceList::iterator it = correspondence.begin();
    it < correspondence.end(); ++it)
    {
    if (it->first == targetRoot)
      {
      it->second->GetWorldHeadRest(animRootHead);
      break;
      }
    }

  // Scale to animation's size
  std::vector<double> targetDistances;
  bool resizingSuccess = 
    vtkMRMLArmatureNodeHelper::ResizeArmature(
      targetArmature, targetBones.GetPointer(),
      animationSizes, animRootHead, &targetDistances);
  if (!resizingSuccess)
    {
    return false;
    }

  // Animate
  if (!vtkMRMLArmatureNodeHelper::AnimateBones(correspondence))
    {
    return false;
    }

  // Resize the armature to its original size
  resizingSuccess =
    vtkMRMLArmatureNodeHelper::ResizeArmature(
      targetArmature, targetBones.GetPointer(),
      targetDistances, targetRootHead);
  if (!resizingSuccess)
    {
    return false;
    }

  targetArmature->SetWidgetState(oldState);

  return true;
}

//----------------------------------------------------------------------------
bool vtkMRMLArmatureNodeHelper
::AnimateBones(CorrespondenceList& correspondence)
{
  for (CorrespondenceList::iterator it = correspondence.begin();
    it < correspondence.end(); ++it)
    {
    vtkMRMLBoneNode* targetBone = it->first;
    vtkBoneWidget* animBone = it->second;

    if (!targetBone || !animBone)
      {
      std::cerr<<"All the bones of target bone are expected to be valid "
        <<" vtkMRMLBoneNodes and all the bones of target bone are expected "
        << "to be valid vtkBoneWidget."<<std::endl;
      return false;
      }

    double targetHead[3], targetTail[3];
    targetBone->GetWorldHeadPose(targetHead);
    targetBone->GetWorldTailPose(targetTail);
    double targetDirection[3];
    vtkMath::Subtract(targetTail, targetHead, targetDirection);
    vtkMath::Normalize(targetDirection);

    double animHead[3], animTail[3];
    animBone->GetWorldToParentPoseTranslation(animHead);
    animBone->GetWorldTailPose(animTail);
    double animDirection[3];
    vtkMath::Subtract(animTail, targetHead, animDirection);
    vtkMath::Normalize(animDirection);

    vtkQuaterniond animToTarget =
      vtkBoneWidget::RotationFromReferenceAxis(
        targetDirection, animDirection);

    double axis[3];
    double angle = animToTarget.GetRotationAngleAndAxis(axis);
    targetBone->RotateTailWithWorldWXYZ(angle, axis);

    // Roll ?
    double h[3], diff[3];
    animBone->GetWorldHeadPose(h);
    vtkMath::Subtract(animHead, h, diff);
    bool animBoneIsLinked = (vtkMath::Norm(diff) < 1e-6);
    if (animBoneIsLinked && targetBone->GetBoneLinkedWithParent())
      {
      targetBone->GetWorldHeadPose(targetHead);
      targetBone->GetWorldTailPose(targetTail);
      vtkMath::Subtract(targetTail, targetHead, targetDirection);
      vtkMath::Normalize(targetDirection);

      vtkQuaterniond rollRotation =
        animBone->GetWorldToBonePoseRotation() *
        targetBone->GetWorldToBonePoseRotation().Inverse();

      angle = rollRotation.GetRotationAngleAndAxis(axis);
      targetBone->RotateTailWithWorldWXYZ(angle, targetDirection);
      }
    }
  return true;
}

//----------------------------------------------------------------------------
bool vtkMRMLArmatureNodeHelper
::GetCorrespondence(vtkCollection* targetBones,
                    vtkArmatureWidget* animationArmature,
                    CorrespondenceList& correspondence)
{
  //Get all the bones
  for (int i = 0; i < targetBones->GetNumberOfItems(); ++i)
    {
    vtkMRMLBoneNode* targetBone =
      vtkMRMLBoneNode::SafeDownCast(targetBones->GetItemAsObject(i));
    if (!targetBone)
      {
      std::cerr<<"Could not find correspondence. Stopping."<<std::endl;
      return false;
      }
    vtkBoneWidget* correspondingBone =
      animationArmature->GetBoneByName(targetBone->GetName());
    if (!correspondingBone)
      {
      std::cerr<<"Could not find matching bone for "
        <<targetBone->GetName()<<". Stopping."<<std::endl;
      return false;
      }

    correspondence.push_back(std::make_pair(targetBone, correspondingBone));
    }
  return true;
}

//----------------------------------------------------------------------------
void vtkMRMLArmatureNodeHelper::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os, indent);
}
