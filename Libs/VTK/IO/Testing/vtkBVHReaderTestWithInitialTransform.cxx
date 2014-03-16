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

#include "vtkBVHReader.h"

// Widget includes
#include <vtkArmatureWidget.h>
#include <vtkBoneWidget.h>

// VTK includes
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkSmartPointer.h>
#include <vtkTransform.h>

//----------------------------------------------------------------------------
namespace
{

static const int NumberOfBones = 3;
static const int NumberOfTransforms = 4;
static char* BoneNames[NumberOfBones] = {"Root",
                                         "IntermediateBone",
                                         "EndBone",
                                        };

// Bone World Position:
// 34 is number of frames
// For each frame, there are 3 bones (see the order of BoneNames)
// for each bone there is the head position and the tail position
static double BoneWorldPosition[NumberOfTransforms][NumberOfBones*2][3] =
  {
    // translation:
    { // 10.0, -10.0, 10.0
      {0.0, 0.0, 0.0}, {0.0, 10.0, 0.0},
      {0.0, 10.0, 0.0}, {0.0, 20.0, 0.0},
      {0.0, 20.0, 0.0}, {0.0, 30.0, 0.0},
    },
    // Rotation:
    { // 90 X
      {0.0, 0.0, 0.0}, {0.0, 0.0, 10.0},
      {0.0, 0.0, 10.0}, {0.0, 0.0, 20.0},
      {0.0, 0.0, 20.0}, {0.0, 0.0, 30.0},
    },
    { // 90 Y
      {0.0, 0.0, 0.0}, {0.0, 10.0, 0.0},
      {0.0, 10.0, 0.0}, {0.0, 20.0, 0.0},
      {0.0, 20.0, 0.0}, {0.0, 30.0, 0.0},
    },
    { // 90 Z
      {0.0, 0.0, 0.0}, {-10.0, 0.0, 0.0},
      {-10.0, 0.0, 0.0}, {-20.0, 0.0, 0.0},
      {-20.0, 0.0, 0.0}, {-30.0, 0.0, 0.0},
    },
  };

//----------------------------------------------------------------------------
bool CompareVector3(const double* v1, const double* v2)
{
  double diff[3];
  vtkMath::Subtract(v1, v2, diff);
  if (vtkMath::Dot(diff, diff) < 1e-6)
    {
    return true;
    }

  return false;
}

//----------------------------------------------------------------------------
bool CompareBoneWorldPosePosition(vtkBoneWidget* bone, int frame, int boneId)
{
  int index = (NumberOfBones - 1) * boneId;
  double pos[3];
  bone->GetCurrentWorldHead(pos);
  if (! CompareVector3(pos, BoneWorldPosition[frame][index]))
    {
    std::cout<<"  Comparison failed for head. (Bone: "
      <<bone->GetName()<<" frame: "<<frame<<")"<<std::endl;
    std::cout<<"    Got : "<<pos[0]<<" "<<pos[1]<<" "<<pos[2]<<std::endl;
    std::cout<<"    Expected : "<<BoneWorldPosition[frame][index][0]<<" "
      <<BoneWorldPosition[frame][index][1]<<" "
      <<BoneWorldPosition[frame][index][2]<<std::endl;
    return false;
    }

  ++index;
  bone->GetCurrentWorldTail(pos);
  if (! CompareVector3(pos, BoneWorldPosition[frame][index]))
    {
    std::cout<<"  Comparison failed for tail. (Bone: "
      <<bone->GetName()<<" frame: "<<frame<<")"<<std::endl;
    std::cout<<"    Got : "<<pos[0]<<" "<<pos[1]<<" "<<pos[2]<<std::endl;
    std::cout<<"    Expected : "<<BoneWorldPosition[frame][index][0]<<" "
      <<BoneWorldPosition[frame][index][1]<<" "
      <<BoneWorldPosition[frame][index][2]<<std::endl;
    return false;
    }

  return true;
}

} // end namespace

//----------------------------------------------------------------------------
int vtkBVHReaderTestWithInitialTransform(int argc, char* argv[])
{
  vtkSmartPointer<vtkBVHReader> reader = vtkSmartPointer<vtkBVHReader>::New();

  std::string bvhFilename = argv[1];
  bvhFilename += "/SimpleBVH.bvh";

  if (! reader->CanReadFile(bvhFilename.c_str()))
    {
    std::cout<<"File format imcompatible !"<<std::endl;
    return EXIT_FAILURE;
    }

  reader->SetFileName(bvhFilename.c_str());
  reader->Update();
  vtkArmatureWidget* outputBeforeTransformChange = reader->GetArmature();
  if (!outputBeforeTransformChange)
    {
    std::cout<<"No Polydata !"<<std::endl;
    return EXIT_FAILURE;
    }

  // Stack a few transforms for testing
  std::vector< vtkSmartPointer<vtkTransform> > transforms;

  // Translation only:
  transforms.push_back(vtkSmartPointer<vtkTransform>::New());
  transforms.back()->Translate(10.0, -10.0, 10.0);

  // Rotation only:
  // 90 degrees on X
  transforms.push_back(vtkSmartPointer<vtkTransform>::New());
  transforms.back()->Translate(10.0, -10.0, 10.0);
  transforms.back()->RotateX(90.0);

  // 90 degrees on Y
  transforms.push_back(vtkSmartPointer<vtkTransform>::New());
  transforms.back()->Translate(10.0, -10.0, 10.0);
  transforms.back()->RotateY(90.0);

  // 90 degrees on Z
  transforms.push_back(vtkSmartPointer<vtkTransform>::New());
  transforms.back()->Translate(10.0, -10.0, 10.0);
  transforms.back()->RotateZ(90.0);

  // For developers:
  assert(NumberOfTransforms == static_cast<int>(transforms.size()));

  for (int i = 0; i < NumberOfTransforms; ++i)
    {
    outputBeforeTransformChange = reader->GetArmature();
    reader->SetInitialRotation(transforms[i]);
    reader->Update();

    if (reader->GetArmature() == outputBeforeTransformChange)
      {
      std::cout<<"Error, armature should have changed !"<<std::endl;
      return EXIT_FAILURE;
      }

    vtkArmatureWidget* armature = reader->GetArmature();

    for (int j = 0; j < NumberOfBones; ++j)
      {
      vtkBoneWidget* bone = armature->GetBoneByName(BoneNames[j]);
      if (! CompareBoneWorldPosePosition(bone, i, j))
        {
        std::cout<<"Incorrect bone position for bone named: "<<bone->GetName()
          <<" at frame "<<i<<std::endl;
        return EXIT_FAILURE;
        }
      }
    }

  return EXIT_SUCCESS;
}
