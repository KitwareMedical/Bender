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

#include <vector>
#include <string.h>

#include <vtkMathUtilities.h>
#include <vtkCommand.h>

#include "vtkBoneWidget.h"
#include "vtkBoneRepresentation.h"
#include "vtkCylinderBoneRepresentation.h"
#include "vtkDoubleConeBoneRepresentation.h"

namespace
{

int CompareVector3(const double* v1, const double* v2)
{
  double diff[3];
  vtkMath::Subtract(v1, v2, diff);
  if (vtkMath::Dot(diff, diff) < 1e-6)
    {
    return 0;
    }

  return 1;
}

class vtkSpy : public vtkCommand
{
public:
  vtkTypeMacro(vtkSpy, vtkCommand);
  static vtkSpy *New(){return new vtkSpy;}
  virtual void Execute(vtkObject *caller, unsigned long eventId,
                       void *callData);
  // List of node that should be updated when NodeAddedEvent is catched
  std::vector<unsigned long> CalledEvents;
  void ClearEvents() {this->CalledEvents.clear();};
  bool Verbose;
protected:
  vtkSpy():Verbose(false){}
  virtual ~vtkSpy(){}
};

//---------------------------------------------------------------------------
void vtkSpy::Execute(
  vtkObject *vtkcaller, unsigned long eid, void *vtkNotUsed(calldata))
{
  this->CalledEvents.push_back(eid);
  if (this->Verbose)
    {
    std::cout << "vtkSpy: event:" << eid
              << " (" << vtkCommand::GetStringFromEventId(eid) << ")"
              << " time: " << vtkTimeStamp()
              << std::endl;
    }
}

}// end namespace

int vtkBoneWidgetTest_PoseMode(int, char *[])
{
  int errors = 0;
  int sectionErrors = 0;
  // Create bone:
  vtkBoneWidget* bone = vtkBoneWidget::New();
  bone->SetWidgetStateToRest();

  // Create spy
  vtkSpy* spy = vtkSpy::New();
  //spy->Verbose = true;
  bone->AddObserver(vtkCommand::AnyEvent, spy);

  bone->SetWidgetState(vtkBoneWidget::Pose);
  sectionErrors += spy->CalledEvents[0] != vtkBoneWidget::PoseChangedEvent;
  sectionErrors += spy->CalledEvents[1] != vtkCommand::ModifiedEvent;
  sectionErrors += spy->CalledEvents.size() != 2;

  //
  // No parent transforms
  //

  // (Nothing has moved yet)
  sectionErrors += CompareVector3(bone->GetWorldHeadRest(), bone->GetWorldHeadPose());
  sectionErrors += CompareVector3(bone->GetWorldTailRest(), bone->GetWorldTailPose());

  // Head
  double axis[3];
  double angle = vtkMath::RadiansFromDegrees(42.0);

  axis[0] = 0.0;   axis[1] = 28.0;   axis[2] = -100.0002;
  vtkMath::Normalize(axis);
  spy->ClearEvents();
  bone->RotateTailWXYZ(angle, axis);
  sectionErrors += spy->CalledEvents[0] != vtkBoneWidget::PoseChangedEvent;
  sectionErrors += spy->CalledEvents[1] != vtkCommand::ModifiedEvent;
  sectionErrors += spy->CalledEvents.size() != 2;
  double tail[3] = {0.999918, -0.0123198, -0.00344953};
  sectionErrors += CompareVector3(bone->GetWorldHeadRest(), bone->GetWorldHeadPose());
  sectionErrors += CompareVector3(bone->GetWorldTailPose(), tail);

  vtkQuaterniond restToPoseRotation;
  restToPoseRotation.SetRotationAngleAndAxis(angle, axis);
  sectionErrors += bone->GetRestToPoseRotation().Compare(restToPoseRotation, 1e-4) != true;

  // Check length
  double restLineVect[3];
  vtkMath::Subtract(bone->GetWorldHeadRest(), bone->GetWorldTailRest(), restLineVect);
  double poseLineVect[3];
  vtkMath::Subtract(bone->GetWorldHeadPose(), bone->GetWorldTailPose(), poseLineVect);
  sectionErrors += fabs(vtkMath::Normalize(restLineVect) - vtkMath::Normalize(poseLineVect)) > 1e-6;

  // Translations
  sectionErrors += CompareVector3(bone->GetWorldToBoneHeadRestTranslation(),
    bone->GetWorldToBoneHeadPoseTranslation());
  sectionErrors += CompareVector3(bone->GetParentToBonePoseTranslation(),
    bone->GetWorldToBoneHeadPoseTranslation()); // (no parent transform)

  double origin [3] = {0.0, 0.0, 0.0};
  sectionErrors += CompareVector3(bone->GetWorldToParentPoseTranslation(), origin);

  sectionErrors += CompareVector3(bone->GetWorldToBoneTailPoseTranslation(),
    bone->GetLocalTailPose()); // (no parent transform)

  // Rotations
  vtkQuaterniond worldToBonePose;
  worldToBonePose.Set(0.416122, -0.0683253, 0.0683253, -0.90416);
  sectionErrors += bone->GetWorldToBonePoseRotation().Compare(worldToBonePose, 1e-4) != true;
  sectionErrors += bone->GetWorldToBonePoseRotation().Compare(
    bone->GetParentToBonePoseRotation(), 1e-4) != true; // (no parent transform)
  vtkQuaterniond identityRotation;
  sectionErrors += bone->GetWorldToParentPoseRotation().Compare(identityRotation, 1e-4) != true;

  if (sectionErrors > 0)
    {
    std::cout<<"There were "<<sectionErrors
      <<" while testing the bone widget transforms and "
      "positions with NO parent transform."<<std::endl;
    }
  errors += sectionErrors;
  sectionErrors = 0;

  //
  // With parent transform
  //

  // re-init events
  sectionErrors = 0;

  vtkQuaterniond quat(36.0, 5.0, -20.0, -0.0001);
  quat.Normalize();
  double rotation[4];
  for (int i = 0; i < 4; ++i)
    {
    rotation[i] = quat[i];
    }
  double translation[3] = {10.0, -0.01, 22200};

  spy->ClearEvents();
  bone->SetWorldToParentPoseRotationAndTranslation(rotation, translation);
  sectionErrors += spy->CalledEvents[0] != vtkBoneWidget::PoseChangedEvent;
  sectionErrors += spy->CalledEvents[1] != vtkCommand::ModifiedEvent;
  sectionErrors += spy->CalledEvents.size() != 2;

  // World Position
  tail[0] = 10.5394 ;   tail[1] = -0.137446;   tail[2] = 22200.8;
  sectionErrors += CompareVector3(bone->GetWorldToParentPoseTranslation(),
    bone->GetLocalHeadPose());
  sectionErrors += CompareVector3(tail, bone->GetWorldTailPose());

  // World To Parent
  sectionErrors += quat.Compare(bone->GetWorldToParentPoseRotation(), 1e-4) != true;
  sectionErrors += CompareVector3(translation, bone->GetWorldToParentPoseTranslation());

  // Parent to bone
  vtkQuaterniond parentToBonePose(0.402277, 0.42676, -0.032347, -0.809323);
  sectionErrors += bone->GetWorldToParentPoseRotation().Compare(parentToBonePose, 1e-4) != true;

  // World to bone
  worldToBonePose.Set(0.707986, 0.702722, 0, -0.070272);
  sectionErrors += bone->GetWorldToBonePoseRotation().Compare(worldToBonePose, 1e-4) != true;

  // Locals
  double localHead[3] = {0.0, 0.0, 0.0};
  double localTail[3] = {0.999918, -0.0123198, -0.00344953};

  sectionErrors += CompareVector3(localHead, bone->GetLocalHeadPose());
  sectionErrors += CompareVector3(localTail, bone->GetLocalTailPose());

  double lineVect[3];
  vtkMath::Subtract(localHead, localTail, lineVect);
  sectionErrors += fabs(vtkMath::Normalize(lineVect) - bone->GetLength()) > 1e-6;

  if (errors > 0)
    {
    std::cout<<"There were "<<errors
      <<" while testing the bone widget transforms and "
      "positions with parent transform."<<std::endl;
    }

  spy->Verbose = false;
  spy->Delete();
  bone->Delete();
  if (errors > 0)
    {
    std::cout<<"Test failed with "<<errors<<" errors."<<std::endl;
    //return EXIT_FAILURE;
    }
  else
    {
    std::cout<<"Pose Mode Widget test passed !"<<std::endl;
    }

  return EXIT_SUCCESS;
}
