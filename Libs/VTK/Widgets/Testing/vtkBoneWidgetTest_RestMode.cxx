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

int vtkBoneWidgetTest_RestMode(int, char *[])
{
  int errors = 0;
  int sectionErrors = 0;

  // Create bone:
  vtkBoneWidget* bone = vtkBoneWidget::New();

  // Create spy
  vtkSpy* spy = vtkSpy::New();
  //spy->Verbose = true;
  bone->AddObserver(vtkCommand::AnyEvent, spy);

  //
  // No parent transforms
  //

  double head[3];
  double tail[3];
  sectionErrors += CompareVector3(bone->GetWorldHeadRest(), bone->GetCurrentWorldHead());
  sectionErrors += CompareVector3(bone->GetWorldTailRest(), bone->GetCurrentWorldTail());

  // (Nothing has moved yet)
  sectionErrors += CompareVector3(bone->GetWorldHeadRest(), bone->GetWorldHeadPose());
  sectionErrors += CompareVector3(bone->GetWorldTailRest(), bone->GetWorldTailPose());

  // Head
  head[0] = 10.0;   head[1] = 42.0;   head[2] = -100.0002;
  spy->ClearEvents();
  bone->SetWorldHeadRest(head);
  sectionErrors += spy->CalledEvents[0] != vtkBoneWidget::RestChangedEvent;
  sectionErrors += spy->CalledEvents[1] != vtkCommand::ModifiedEvent;
  sectionErrors += spy->CalledEvents.size() != 2;
  sectionErrors += CompareVector3(bone->GetWorldHeadRest(), head);

  spy->ClearEvents();
  bone->SetWorldHeadRest(head); // Try again and make sure it does not move
  sectionErrors += spy->CalledEvents.size() > 0;

  // Tail
  tail[0] = 26.0;   tail[1] = -300.;   tail[2] = -0.000008;
  spy->ClearEvents();
  bone->SetWorldTailRest(tail);
  sectionErrors += spy->CalledEvents[0] != vtkBoneWidget::RestChangedEvent;
  sectionErrors += spy->CalledEvents[1] != vtkCommand::ModifiedEvent;
  sectionErrors += spy->CalledEvents.size() != 2;
  sectionErrors += CompareVector3(bone->GetWorldTailRest(), tail);

  spy->ClearEvents();
  bone->SetWorldTailRest(tail); // Try again and make sure it does not move
  sectionErrors += spy->CalledEvents.size() > 0;

  // Move both and look at transforms
  head[0] = 200.0;   head[1] = 42.0;   head[2] = -100.0002;
  tail[0] = 220.0;   tail[1] = 42.5;   tail[2] = 100.0002;
  spy->ClearEvents();
  bone->SetWorldHeadAndTailRest(head, tail);
  sectionErrors += spy->CalledEvents[0] != vtkBoneWidget::RestChangedEvent;
  sectionErrors += spy->CalledEvents[1] != vtkCommand::ModifiedEvent;
  sectionErrors += spy->CalledEvents.size() != 2;

  // Check length
  double lineVect[3];
  vtkMath::Subtract(head, tail, lineVect);
  sectionErrors += fabs(vtkMath::Normalize(lineVect) - bone->GetLength()) > 1e-6;

  // Translations
  sectionErrors += CompareVector3(bone->GetWorldToBoneHeadRestTranslation(), head);
  sectionErrors += CompareVector3(bone->GetWorldToBoneTailRestTranslation(), tail);
  sectionErrors += CompareVector3(bone->GetParentToBoneRestTranslation(),
    bone->GetWorldToBoneHeadRestTranslation()); // (no parent transform)

  double origin [3] = {0.0, 0.0, 0.0};
  sectionErrors += CompareVector3(bone->GetWorldToParentRestTranslation(), origin);

  // Rotations
  vtkQuaterniond worldToBoneRest(0.707986, 0.702722, 0, -0.070272);
  sectionErrors += bone->GetWorldToBoneRestRotation().Compare(worldToBoneRest, 1e-4) != true;
  sectionErrors += bone->GetWorldToBoneRestRotation().Compare(
    bone->GetParentToBoneRestRotation(), 1e-4) != true; // (no parent transform)
  vtkQuaterniond identityRotation;
  sectionErrors += bone->GetWorldToParentRestRotation().Compare(identityRotation, 1e-4) != true;

  sectionErrors += bone->GetRestToPoseRotation().Compare(identityRotation, 1e-4) != true;

  if (sectionErrors > 0)
    {
    std::cout<<"There were "<<sectionErrors
      <<" while testing the bone widget transforms and "
      "positions with NO parent transform."<<std::endl;
    }
  errors += sectionErrors;

  //
  // With parent transform
  //

  // re-init events
  sectionErrors = 0;

  vtkQuaterniond quat(0.2, 1.0, 220.0, -3.0);
  quat.Normalize();
  double rotation[4];
  for (int i = 0; i < 4; ++i)
    {
    rotation[i] = quat[i];
    }
  double translation[3] = {10.0, -0.01, 22200};

  spy->ClearEvents();
  bone->SetWorldToParentRestRotationAndTranslation(rotation, translation);
  sectionErrors += spy->CalledEvents[0] != vtkBoneWidget::RestChangedEvent;
  sectionErrors += spy->CalledEvents[1] != vtkCommand::ModifiedEvent;
  sectionErrors += spy->CalledEvents.size() != 2;

  sectionErrors += CompareVector3(bone->GetWorldHeadRest(), head);
  sectionErrors += CompareVector3(bone->GetWorldTailRest(), tail);

  // World To Parent
  sectionErrors += quat.Compare(bone->GetWorldToParentRestRotation(), 1e-4) != true;
  sectionErrors += CompareVector3(translation, bone->GetWorldToParentRestTranslation());

  // Parent to bone
  vtkQuaterniond parentToBoneRest(0.000908997, 0.00454498, 0.999896, -0.0136349);
  sectionErrors += bone->GetWorldToParentRestRotation().Compare(parentToBoneRest, 1e-4) != true;

  // World to bone
  worldToBoneRest.Set(0.707986, 0.702722, 0, -0.070272);
  sectionErrors += bone->GetWorldToBoneRestRotation().Compare(worldToBoneRest, 1e-4) != true;

  // Locals
  double localHead[3] = {-146.31, 651.596, 22290.8};
  double localTail[3] = {-166.693, 646.826, 22090.9};

  sectionErrors += CompareVector3(localHead, bone->GetLocalHeadRest());
  sectionErrors += CompareVector3(localTail, bone->GetLocalTailRest());

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
    return EXIT_FAILURE;
    }
  else
    {
    std::cout<<"Rest Mode Widget test passed !"<<std::endl;
    }

  return EXIT_SUCCESS;
}
