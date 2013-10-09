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

// .NAME vtkBVHReader - Reads BVH files.
// .SECTION Description
// Reads a BVH (motion capture) file and outputs the corresponding polydata.
//
// Using an armature, the reader creates the rest position of the armature
// from the HIERARCHY part of the BVH. The reader assumes that there is
// only one root.
//
// Since an armature can only have one pose, the SetFrame property allow
// to choose from the different motion frame. The movement data is gathered
// under The MOTION part of the file. Upon reading, the animation information
// is stored. This provides faster look up when changing the frame.
//
// The polydata is obtained from the armature widget. To learn more about
// its structure, see the vtkArmatureWidget->GetPolyData().
//
// .SECTION See Also
// vtkBVHReader, vtkArmatureWidget,

#ifndef __vtkBVHReader_h
#define __vtkBVHReader_h

#include <vtkPolyDataAlgorithm.h>

#include "vtkBenderIOExport.h"
#include "vtkQuaternion.h"

#include <vector>

class vtkArmatureWidget;
class vtkBoneWidget;
class vtkTransform;

class VTK_BENDER_IO_EXPORT vtkBVHReader : public vtkPolyDataAlgorithm
{
public:
  vtkTypeMacro(vtkBVHReader, vtkPolyDataAlgorithm);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  static vtkBVHReader *New();

  // Description:
  // Set the motion capture file's filename to read.
  // Setting a new filename invalids the current armature (if any).
  void SetFileName(const char* filename);
  const char* GetFileName() const;

  // Description:
  // Set the desired frame.
  // Default is 0.
  vtkGetMacro(Frame, unsigned int);
  void SetFrame(unsigned int frame);

  // Description:
  // Once the file is read, returns the number of frames available.
  // Default is 0.
  vtkGetMacro(NumberOfFrames, unsigned int);

  // Description:
  // Once the file is read, this returns the number of frames available.
  // Default is 0.
  vtkGetMacro(FrameRate, double);

  // Description:
  // When linking to the first child, the first child of a bone will
  // always start from its parent tail.
  // When this option is off, if the parent has multiple child, the
  // parent's tail position will be given by the average position
  // of its children's head.
  // Default is false.
  void SetLinkToFirstChild(bool link);
  vtkGetMacro(LinkToFirstChild, bool);

  // Description:
  // Get the armature from which the polydata is obtained.
  vtkGetObjectMacro(Armature, vtkArmatureWidget);

  // Description:
  // A simple, non-exhaustive check to see if a file is a valid file.
  static int CanReadFile(const char *filename);

  // Description:
  // Apply the frame to the given armature. The armature must be the
  // exact same armature than the armature read.
  // This method is meant for exterior application to be able to drive
  // which pose the armature has. Return if the operation succeeded.
  bool ApplyFrameToArmature(vtkArmatureWidget* armature,  unsigned int frame);

  // Description:
  // Access method to the frame rotation data.
  // No check is performed on the frame nor the boneId for perfomance reasons.
  vtkQuaterniond GetParentToBoneRotation(
    unsigned int frame, unsigned int boneId);

  // Description:
  // Set/Get the initial rotation applied to the armature read.
  // This will transform the initially obtained data. If the file has already
  // been read, changing the transform will invalid the reader, causing a new
  // reading on update. This is useful as the BVH files usually have Y as up.
  // Only the rotation of the given transform will be used. Any translation
  // will be ignored.
  // Default rotation is 90 on X then 180 on Z.
  void SetInitialRotation(vtkTransform* transform);
  vtkTransform* GetInitialRotation() const;

protected:
  vtkBVHReader();
  ~vtkBVHReader();

  std::string FileName;
  bool RestArmatureIsValid;

  vtkArmatureWidget* Armature;
  unsigned int Frame;
  bool LinkToFirstChild;
  unsigned int NumberOfFrames;
  double FrameRate;
  vtkTransform* InitialRotation;

  virtual int RequestData(
    vtkInformation*, vtkInformationVector**, vtkInformationVector*);

  int Parse(std::ifstream& file);
  void ParseMotions(
    std::ifstream& file, std::vector< std::vector<std::string> >& channels);
  void ParseRestArmature(
    std::ifstream& file,
    size_t parentId,
    std::vector< std::vector<std::string> >& channels);
  void ParseEndSite(std::ifstream& file, size_t parentId);

  void LinkBonesToFirstChild();
  void UnlinkBonesFromFirstChild();

  typedef std::vector<vtkBoneWidget*> BonesList;
  BonesList Bones;

  typedef std::vector< std::vector<vtkQuaterniond> > FramesList;
  FramesList Frames;

  void InvalidReader();

  double* TransformPoint(double point[3]) const;
  void TransformPoint(double point[3], double newPoint[3]) const;

private:
  vtkBVHReader(const vtkBVHReader&);  // Not implemented.
  void operator=(const vtkBVHReader&);  // Not implemented.
};

#endif
