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
static const int NumberOfFrames = 34;
static char* BoneNames[NumberOfBones] = {"Root",
                                         "IntermediateBone",
                                         "EndBone",
                                        };

// Bone World Position:
// 34 is number of frames
// For each frame, there are 3 bones (see the order of BoneNames)
// for each bone there is the head position and the tail position
static double BoneWorldPosition[NumberOfFrames][NumberOfBones*2][3] =
  {
    // On root
    { // no rotation
      {0.0, 0.0, 0.0}, {0.0, 10.0, 0.0},
      {0.0, 10.0, 0.0}, {0.0, 20.0, 0.0},
      {0.0, 20.0, 0.0}, {0.0, 30.0, 0.0},
    },
    { // 90 X
      {0.0, 0.0, 0.0}, {0.0, 0.0, 10.0},
      {0.0, 0.0, 10.0}, {0.0, 0.0, 20.0},
      {0.0, 0.0, 20.0}, {0.0, 0.0, 30.0},
    },
    { // 180 X
      {0.0, 0.0, 0.0}, {0.0, -10.0, 0.0},
      {0.0, -10.0, 0.0}, {0.0, -20.0, 0.0},
      {0.0, -20.0, 0.0}, {0.0, -30.0, 0.0},
    },
    { // 270 X
      {0.0, 0.0, 0.0}, {0.0, 0.0, -10.0},
      {0.0, 0.0, -10.0}, {0.0, 0.0, -20.0},
      {0.0, 0.0, -20.0}, {0.0, 0.0, -30.0},
    },
    { // 360 X
      {0.0, 0.0, 0.0}, {0.0, 10.0, 0.0},
      {0.0, 10.0, 0.0}, {0.0, 20.0, 0.0},
      {0.0, 20.0, 0.0}, {0.0, 30.0, 0.0},
    },
    { // 450 X
      {0.0, 0.0, 0.0}, {0.0, 0.0, 10.0},
      {0.0, 0.0, 10.0}, {0.0, 0.0, 20.0},
      {0.0, 0.0, 20.0}, {0.0, 0.0, 30.0},
    },
    { // -0.2 X
      {0.0, 0.0, 0.0}, {0.0, 9.99994, -0.0349065},
      {0.0, 9.99994, -0.0349065}, {0.0, 19.9999, -0.069813},
      {0.0, 19.9999, -0.069813}, {0.0, 29.9998, -0.10472},
    },
    { // 90 Y
      {0.0, 0.0, 0.0}, {0.0, 10.0, 0.0},
      {0.0, 10.0, 0.0}, {0.0, 20.0, 0.0},
      {0.0, 20.0, 0.0}, {0.0, 30.0, 0.0},
    },
    { // 180 Y
      {0.0, 0.0, 0.0}, {0.0, 10.0, 0.0},
      {0.0, 10.0, 0.0}, {0.0, 20.0, 0.0},
      {0.0, 20.0, 0.0}, {0.0, 30.0, 0.0},
    },
    { // 270 Y
      {0.0, 0.0, 0.0}, {0.0, 10.0, 0.0},
      {0.0, 10.0, 0.0}, {0.0, 20.0, 0.0},
      {0.0, 20.0, 0.0}, {0.0, 30.0, 0.0},
    },
    { // 360 Y
      {0.0, 0.0, 0.0}, {0.0, 10.0, 0.0},
      {0.0, 10.0, 0.0}, {0.0, 20.0, 0.0},
      {0.0, 20.0, 0.0}, {0.0, 30.0, 0.0},
    },
    { // 450 Y
      {0.0, 0.0, 0.0}, {0.0, 10.0, 0.0},
      {0.0, 10.0, 0.0}, {0.0, 20.0, 0.0},
      {0.0, 20.0, 0.0}, {0.0, 30.0, 0.0},
    },
    { // -0.2 Y
      {0.0, 0.0, 0.0}, {0.0, 10.0, 0.0},
      {0.0, 10.0, 0.0}, {0.0, 20.0, 0.0},
      {0.0, 20.0, 0.0}, {0.0, 30.0, 0.0},
    },
    { // 90 Z
      {0.0, 0.0, 0.0}, {-10.0, 0.0, 0.0},
      {-10.0, 0.0, 0.0}, {-20.0, 0.0, 0.0},
      {-20.0, 0.0, 0.0}, {-30.0, 0.0, 0.0},
    },
    { // 180 Z
      {0.0, 0.0, 0.0}, {0.0, -10.0, 0.0},
      {0.0, -10.0, 0.0}, {0.0, -20.0, 0.0},
      {0.0, -20.0, 0.0}, {0.0, -30.0, 0.0},
    },
    { // 270 Z
      {0.0, 0.0, 0.0}, {10.0, 0.0, 0.0},
      {10.0, 0.0, 0.0}, {20.0, 0.0, 0.0},
      {20.0, 0.0, 0.0}, {30.0, 0.0, 0.0},
    },
    { // 360 Z
      {0.0, 0.0, 0.0}, {0.0, 10.0, 0.0},
      {0.0, 10.0, 0.0}, {0.0, 20.0, 0.0},
      {0.0, 20.0, 0.0}, {0.0, 30.0, 0.0},
    },
    { // 450 Z
      {0.0, 0.0, 0.0}, {-10.0, 0.0, 0.0},
      {-10.0, 0.0, 0.0}, {-20.0, 0.0, 0.0},
      {-20.0, 0.0, 0.0}, {-30.0, 0.0, 0.0},
    },
    { // -0.2 Z
      {0.0, 0.0, 0.0}, {0.0349065, 9.99994, 0.0},
      {0.0349065, 9.99994, 0.0}, {0.069813, 19.9999, 0.0},
      {0.069813, 19.9999, 0.0}, {0.10472, 29.9998, 0.0},
    },

    // On Root X, Interm X
    { // Root: 90X, Interm 90X
      {0.0, 0.0, 0.0}, {0.0, 0.0, 10.0},
      {0.0, 0.0, 10.0}, {0.0, -10.0, 10.0},
      {0.0, -10.0, 10.0}, {0.0, -20.0, 10.0},
    },
    { // Root: 90X, Interm 270X
      {0.0, 0.0, 0.0}, {0.0, 0.0, 10.0},
      {0.0, 0.0, 10.0}, {0.0, 10.0, 10.0},
      {0.0, 10.0, 10.0}, {0.0, 20.0, 10.0},
    },
    { // Root: 90X, Interm -10.2X
      {0.0, 0.0, 0.0}, {0.0, 0.0, 10.0},
      {0.0, 0.0, 10.0}, {0.0, 1.77085, 19.842},
      {0.0, 1.77085, 19.842}, {0.0, 3.54169, 29.6839},
    },
    { // Root: 270X, Iterm 90X
      {0.0, 0.0, 0.0}, {0.0, 0.0, -10.0},
      {0.0, 0.0, -10.0}, {0.0, 10.0, -10.0},
      {0.0, 10.0, -10.0}, {0.0, 20.0, -10.0},
    },
    { // Root: 270X, Iterm 270X
      {0.0, 0.0, 0.0}, {0.0, 0.0, -10.0},
      {0.0, 0.0, -10.0}, {0.0, -10.0, -10.0},
      {0.0, -10.0, -10.0}, {0.0, -20.0, -10.0},
    },
    { // Root: 270X, Iterm -10.2X
      {0.0, 0.0, 0.0}, {0.0, 0.0, -10.0},
      {0.0, 0.0, -10.0}, {0.0, -1.77085, -19.842},
      {0.0, -1.77085, -19.842}, {0.0, -3.54169, -29.6839},
    },

    // On Root X, Interm X, End Z
    { // Root: 90X, Interm 270X, End 90Z
      {0.0, 0.0, 0.0}, {0.0, 0.0, 10.0},
      {0.0, 0.0, 10.0}, {0.0, 10.0, 10.0},
      {0.0, 10.0, 10.0}, {-10.0, 10.0, 10.0},
    },
    { // Root: 90X, Interm 270X, End 270Z
      {0.0, 0.0, 0.0}, {0.0, 0.0, 10.0},
      {0.0, 0.0, 10.0}, {0.0, 10.0, 10.0},
      {0.0, 10.0, 10.0}, {10.0, 10.0, 10.0},
    },
    { // Root: 90X, Interm 270X, End -10.2Z
      {0.0, 0.0, 0.0}, {0.0, 0.0, 10.0},
      {0.0, 0.0, 10.0}, {0.0, 10.0, 10.0},
      {0.0, 10.0, 10.0}, {1.77085, 19.842, 10.0},
    },

    // On Root Y, Interm Z, End Z
    { // Root: 270Y, Interm: 90Z, End: 90Z
      {0.0, 0.0, 0.0}, {0.0, 10.0, 0.0},
      {0.0, 10.0, 0.0}, {0.0, 10.0, -10.0},
      {0.0, 10.0, -10.0}, {0.0, 0.0, -10.0},
    },
    { // Root: 270Y, Interm: 90Z, End: 270Z
      {0.0, 0.0, 0.0}, {0.0, 10.0, 0.0},
      {0.0, 10.0, 0.0}, {0.0, 10.0, -10.0},
      {0.0, 10.0, -10.0}, {0.0, 20.0, -10.0},
    },
    { // Root: 270Y, Interm: 90Z, End: -10.2Z
      {0.0, 0.0, 0.0}, {0.0, 10.0, 0.0},
      {0.0, 10.0, 0.0}, {0.0, 10.0, -10.0},
      {0.0, 10.0, -10.0}, {0.0, 11.7708, -19.842},
    },

    // On Root Z, Interm Y, End X
    { // Root: -10.2Z, Interm: 270Y, End: 90X
      {0.0, 0.0, 0.0}, {1.77085, 9.84196, 0.0},
      {1.77085, 9.84196, 0.0}, {3.54169, 19.6839, 0.0},
      {3.54169, 19.6839, 0.0}, {-6.30026, 21.4548, 0.0},
    },
    { // Root: -10.2Z, Interm: 270Y, End: 270X
      {0.0, 0.0, 0.0}, {1.77085, 9.84196, 0.0},
      {1.77085, 9.84196, 0.0}, {3.54169, 19.6839, 0.0},
      {3.54169, 19.6839, 0.0}, {13.3837, 17.9131, 0.0},
    },
    { // Root: -10.2Z, Interm: 270Y, End: -10.2X
      {0.0, 0.0, 0.0}, {1.77085, 9.84196, 0.0},
      {1.77085, 9.84196, 0.0}, {3.54169, 19.6839, 0.0},
      {3.54169, 19.6839, 0.0}, {7.02742, 29.0567, 0.0},
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
int vtkBVHReaderTest(int argc, char* argv[])
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
  vtkNew<vtkTransform> identity;
  reader->SetInitialRotation(identity.GetPointer());

  reader->Update();
  if (!reader->GetOutput())
    {
    std::cout<<"No Polydata !"<<std::endl;
    return EXIT_FAILURE;
    }

  if (reader->GetNumberOfFrames() != NumberOfFrames)
    {
    std::cout<<"Number of frame incorrect: Got "
      <<reader->GetNumberOfFrames()<<" expected: "
      <<NumberOfFrames<<std::endl;
    return EXIT_FAILURE;
    }

  const double FrameRateExpected = 0.025;
  if (reader->GetFrameRate() != FrameRateExpected)
    {
    std::cout<<"Frame rate incorrect: Got "
      <<reader->GetFrameRate()<<" expected: "<<FrameRateExpected<<std::endl;
    return EXIT_FAILURE;
    }

  for (int i = 0; i < NumberOfFrames; ++i)
    {
    reader->SetFrame(i);
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
