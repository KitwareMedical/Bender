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

#ifndef __vtkMRMLArmatureNodeHelper_h
#define __vtkMRMLArmatureNodeHelper_h

// VTK Includes
#include <vtkObject.h>

// STD includes
#include <vector>

// Armatures includes
#include "vtkBenderArmaturesModuleMRMLCoreExport.h"

class vtkArmatureWidget;
class vtkBoneWidget;
class vtkCollection;
class vtkMRMLArmatureNode;
class vtkMRMLBoneNode;

/// \ingroup Bender_MRML
/// \brief Armature helper functions
///

class VTK_BENDER_ARMATURES_MRML_CORE_EXPORT vtkMRMLArmatureNodeHelper
  : public vtkObject
{
public:
  static vtkMRMLArmatureNodeHelper *New();
  vtkTypeMacro(vtkMRMLArmatureNodeHelper,vtkObject);
  virtual void PrintSelf(ostream& os, vtkIndent indent);

  typedef std::pair<vtkMRMLBoneNode*, vtkBoneWidget*> CorrespondencePair;
  typedef std::vector<CorrespondencePair> CorrespondenceList;

  // Tries to emulate the pose of animation armature on the target armature.
  static bool AnimateArmature(
    vtkMRMLArmatureNode* targetArmature,
    vtkArmatureWidget* animationarmature);

  // Return a bone node size
  static double GetBoneSize(vtkMRMLBoneNode* bone);
  static double GetBoneSize(vtkBoneWidget* bone);

  // Resize the given armature rest position
  // First, the armature is translated to the given origin. Each bone of the
  // armature is then resized according to the bone's sizes.
  // The bone size is saved in oldDistances before the bone's resizing if that
  // paramater isn't NULL. Returns whether it succeeded.
  //
  // Assumes that bones is ordered hierarchically.
  static bool ResizeArmature(
    vtkMRMLArmatureNode* armature,
    vtkCollection* bones,
    std::vector<double>& sizes,
    double origin[3],
    std::vector<double>* oldSizes = NULL);

  // Given a collection of mrml bone nodes (target) and a collection of
  // bone widgets (animation), tries to reproduce the animation pose.
  // It is highly recommended for the bones have the same size.
  // Returns whether it succeeded.
  //
  // Assumes that the bones in the list are ordered hierarchically
  static bool AnimateBones(CorrespondenceList& correspondence);

  // Over-simple 1-to-1 correspondence based on name
  // Returns whether it succeeded.
  //
  // \todo do something more robust, or, you know, better.
  static bool GetCorrespondence(
    vtkCollection* targetBones,
    vtkArmatureWidget* animationArmature,
    CorrespondenceList& correspondence);

protected:
  vtkMRMLArmatureNodeHelper() {};
  ~vtkMRMLArmatureNodeHelper() {};

private:
  vtkMRMLArmatureNodeHelper(const vtkMRMLArmatureNodeHelper&);  // Not implemented.
  void operator=(const vtkMRMLArmatureNodeHelper&);  // Not implemented.

};
#endif
