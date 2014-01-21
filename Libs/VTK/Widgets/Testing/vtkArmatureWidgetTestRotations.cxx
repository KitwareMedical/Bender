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

#include <vtkCollection.h>
#include <vtkCommand.h>
#include <vtkMath.h>
#include <vtkObjectFactory.h>
#include <vtkSmartPointer.h>

#include "vtkArmatureWidget.h"
#include "vtkBenderWidgetTestHelper.h"
#include "vtkBoneWidget.h"
#include "vtkCylinderBoneRepresentation.h"
#include "vtkDoubleConeBoneRepresentation.h"


#include <vtkAxesActor.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>

#include <vtkInteractorStyleTrackballCamera.h>

//----------------------------------------------------------------------------
namespace
{

bool CompareVector3(double v1[3], double v2[3])
{
  double diff[3];
  vtkMath::Subtract(v1, v2, diff);
  if (vtkMath::Dot(diff, diff) < 1e-6)
    {
    return true;
    }

  return false;
}

void PrintError(double angle, double axis[3],
                double tail[3], double expectedTail[3])
{
  std::cerr<<"Error, expected tail did not match tail for rotation:"
    <<std::endl<<"  - of axis: "<<axis[0]<<" "<<axis[1]<<" "<<axis[2]
    <<std::endl<<"  - of angle: "<<angle
    <<std::endl<<"Expected: "<<expectedTail[0]<<" "<<expectedTail[1]
    <<" "<<expectedTail[2]
    <<std::endl<<"Got:"<<tail[0]<<" "<<tail[1]<<" "<<tail[2]<<std::endl;
}

struct RotationTest
{
  std::string BoneName;
  double RotationAxis[3];
  double RotationAngle; // in degrees
  bool IsWorldRotation;
  bool ResetPose;
  double ExpectedTail[3];

  RotationTest(
    const std::string& name,
    double axisX, double axisY, double axisZ,
    double angle,
    bool worlRotation,
    bool reset,
    double tailX,
    double tailY,
    double tailZ)
    {
    BoneName = name;
    RotationAxis[0] = axisX; RotationAxis[1] = axisY; RotationAxis[2] = axisZ;
    RotationAngle = angle;
    IsWorldRotation = worlRotation;
    ResetPose = reset;
    ExpectedTail[0] = tailX; ExpectedTail[1] = tailY; ExpectedTail[2] = tailZ;
    }

  void Print(ostream& os)
    {
    os<<"BoneName: "<<BoneName
      <<std::endl<<"RotationAxis: "
        <<RotationAxis[0]<<" "<<RotationAxis[1]<<" "<<RotationAxis[2]
      <<std::endl<<"RotationAngle: "<<RotationAngle
      <<std::endl<<"IsWorldRotation: "<<IsWorldRotation
      <<std::endl<<"ResetPose: "<<ResetPose
      <<std::endl<<"ExpectedTail: "
        <<ExpectedTail[0]<<" "<<ExpectedTail[1]<<" "<<ExpectedTail[2]
      <<std::endl;
    }
};

const int NumberOfTestCases = 21;
const RotationTest TestCases[NumberOfTestCases] =
  {
  // No Rotation, just check tail positions
  RotationTest("Root",
               1.0, 0.0, 0.0,
               0.0,
               false,
               false,
               0.0, 20.0, 0.0),
  RotationTest("Middle",
               1.0, 0.0, 0.0,
               0.0,
               false,
               false,
               0.0, 20.0, 20.0),
  RotationTest("End",
               1.0, 0.0, 0.0,
               0.0,
               false,
               false,
               0.0, 20.0, 40.0),

  // Parent rotations
  // Rotation on root X
  RotationTest("Root",
               1.0, 0.0, 0.0,
               -90.0,
               false,
               false,
               0.0, 0.0, -20.0),
  RotationTest("Middle",
               1.0, 0.0, 0.0,
               0.0,
               false,
               false,
               0.0, 20.0, -20.0),
  RotationTest("End",
               1.0, 0.0, 0.0,
               0.0,
               false,
               false,
               0.0, 40.0, -20.0),

  // Rotation on middle Z
  RotationTest("Root",
               1.0, 0.0, 0.0,
               0.0,
               false,
               false,
               0.0, 0.0, -20.0),
  RotationTest("Middle",
               0.0, 0.0, 1.0,
               90.0,
               false,
               false,
               0.0, 20.0, -20.0),
  RotationTest("End",
               1.0, 0.0, 0.0,
               0.0,
               false,
               false,
               0.0, 40.0, -20.0),

  // Rotation on end Z
  RotationTest("Root",
               1.0, 0.0, 0.0,
               0.0,
               false,
               false,
               0.0, 0.0, -20.0),
  RotationTest("Middle",
               1.0, 0.0, 0.0,
               0.0,
               false,
               false,
               0.0, 20.0, -20.0),
  RotationTest("End",
               0.0, 0.0, 1.0,
               -90.0,
               false,
               false,
               0.0, 20.0, -40.0),

  // World rotations
  // Rotation on root Z
  RotationTest("Root",
               0.0, 0.0, 1.0,
               90.0,
               true,
               true,
               -20.0, 0.0, 0.0),
  RotationTest("Middle",
               1.0, 0.0, 0.0,
               0.0,
               true,
               false,
               -20.0, 0.0, 20.0),
  RotationTest("End",
               1.0, 0.0, 0.0,
               0.0,
               true,
               false,
               -20.0, 0.0, 40.0),

  // Rotation on middle X
  RotationTest("Root",
               1.0, 0.0, 0.0,
               0.0,
               true,
               false,
               -20.0, 0.0, 0.0),
  RotationTest("Middle",
               1.0, 0.0, 0.0,
               180.0,
               true,
               false,
               -20.0, 0.0, -20.0),
  RotationTest("End",
               1.0, 0.0, 0.0,
               0.0,
               true,
               false,
               -20.0, 0.0, -40.0),

  // Rotation on end Y
  RotationTest("Root",
               1.0, 0.0, 0.0,
               0.0,
               true,
               false,
               -20.0, 0.0, 0.0),
  RotationTest("Middle",
               1.0, 0.0, 0.0,
               0.0,
               true,
               false,
               -20.0, 0.0, -20.0),
  RotationTest("End",
               0.0, 1.0, 0.0,
               -90.0,
               true,
               false,
               0.0, 0.0, -20.0),

  };

} // end namespace

//----------------------------------------------------------------------------
int vtkArmatureWidgetTestRotations(int, char *[])
{
  int errors = 0;
  int sectionErrors = 0;

  // Create armature
  vtkSmartPointer<vtkArmatureWidget> threeBones =
    vtkSmartPointer<vtkArmatureWidget>::New();

  double head[3] = {0.0, 0.0, 0.0};
  double tail[3] = {0.0, 0.0, 0.0};

  // Create three bones
  vtkBoneWidget* root = threeBones->CreateBone(NULL, "Root");
  tail[0] = 0.0; tail[1] = 20.0; tail[2] = 0.0;
  root->SetWorldTailRest(tail);
  threeBones->AddBone(root, NULL);
  root->Delete();

  tail[0] = 0.0; tail[1] = 20.0; tail[2] = 20.0;
  vtkBoneWidget* middle = threeBones->CreateBone(root, tail, "Middle");
  threeBones->AddBone(middle, root);
  middle->Delete();

  tail[0] = 0.0; tail[1] = 20.0; tail[2] = 40.0;
  vtkBoneWidget* end = threeBones->CreateBone(middle, tail, "End");
  threeBones->AddBone(end, middle);
  end->Delete();

  threeBones->SetWidgetState(vtkArmatureWidget::Pose);

  // Test bones
  for (int i = 0; i < NumberOfTestCases; ++i)
    {
    RotationTest test = TestCases[i];

    if (test.ResetPose)
      {
      threeBones->ResetPoseToRest();
      }

    vtkBoneWidget* bone = threeBones->GetBoneByName(test.BoneName);
    if (!bone)
      {
      std::cerr<<"Error, cannot find bone !"<<std::endl;
      test.Print(std::cerr);
      return EXIT_FAILURE;
      }

    double radiansAngle = vtkMath::RadiansFromDegrees(test.RotationAngle);
    if (test.IsWorldRotation)
      {
      bone->RotateTailWithWorldWXYZ(radiansAngle, test.RotationAxis);
      }
    else
      {
      bone->RotateTailWithParentWXYZ(radiansAngle, test.RotationAxis);
      }

    double result[3];
    bone->GetCurrentWorldTail(result);
    if (! CompareVector3(result, test.ExpectedTail))
      {
      std::cerr<<"Error, did not find the right tail (iteration: "
        <<i<<")"<<std::endl;
      test.Print(std::cerr);
      std::cerr<<"  ---> Result was: "
        <<result[0]<<" "<<result[1]<<" "<<result[2]<<std::endl;
      //return EXIT_FAILURE;
      }
    }

  vtkSmartPointer<vtkRenderer> renderer =
    vtkSmartPointer<vtkRenderer>::New();
  vtkSmartPointer<vtkRenderWindow> renderWindow =
    vtkSmartPointer<vtkRenderWindow>::New();
  renderWindow->AddRenderer(renderer);

  // An interactor
  vtkSmartPointer<vtkRenderWindowInteractor> renderWindowInteractor =
    vtkSmartPointer<vtkRenderWindowInteractor>::New();
  renderWindowInteractor->SetRenderWindow(renderWindow);

  threeBones->SetInteractor(renderWindowInteractor);
  threeBones->SetCurrentRenderer(renderer);
  threeBones->CreateDefaultRepresentation();

  vtkSmartPointer<vtkInteractorStyleTrackballCamera> style =
    vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
  renderWindowInteractor->SetInteractorStyle(style);

  vtkSmartPointer<vtkAxesActor> world =
    vtkSmartPointer<vtkAxesActor>::New();
  renderer->AddActor(world);
  world->SetAxisLabels(0);
  world->SetTotalLength(10.0, 10.0, 10.0);

  renderWindow->Render();
  renderWindowInteractor->Initialize();
  renderWindow->Render();
  threeBones->On();
  threeBones->SetShowAxes(vtkBoneWidget::ShowPoseTransform);

  // Begin mouse interaction
  renderWindowInteractor->Start();


  return EXIT_SUCCESS;
}
