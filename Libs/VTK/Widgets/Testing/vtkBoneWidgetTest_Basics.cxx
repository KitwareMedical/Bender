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

#include <string.h>
#include <vector>

#include <vtkAxesActor.h>
#include <vtkCommand.h>
#include <vtkLineRepresentation.h>
#include <vtkMathUtilities.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkSmartPointer.h>
#include <vtkTransform.h>

#include "vtkBenderWidgetTestHelper.h"
#include "vtkBoneWidget.h"
#include "vtkBoneRepresentation.h"
#include "vtkCylinderBoneRepresentation.h"
#include "vtkDoubleConeBoneRepresentation.h"

int vtkBoneWidgetTest_Basics(int, char *[])
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
  // Representation:
  //

  spy->ClearEvents();
  bone->CreateDefaultRepresentation();
  sectionErrors += spy->CalledEvents[0] != vtkCommand::ModifiedEvent;
  sectionErrors += spy->CalledEvents.size() != 1;
  sectionErrors +=
    vtkBoneRepresentation::SafeDownCast(bone->GetRepresentation()) == 0;
  sectionErrors += vtkCylinderBoneRepresentation::SafeDownCast(
    bone->GetBoneRepresentation()) != 0;
  sectionErrors += vtkDoubleConeBoneRepresentation::SafeDownCast(
    bone->GetBoneRepresentation()) != 0;

  spy->ClearEvents();
  bone->SetRepresentation(0); // Set no representation.
  sectionErrors += spy->CalledEvents[0] != vtkCommand::ModifiedEvent;
  sectionErrors += spy->CalledEvents.size() != 1;

  if (sectionErrors > 0)
    {
    std::cout<<"There were "<<sectionErrors
      <<" while testing the bone widget representation."<<std::endl;
    }
  errors += sectionErrors;

  //
  // State:
  //

  // re-init events
  sectionErrors = 0;

  sectionErrors += bone->GetWidgetState() != vtkBoneWidget::PlaceHead;
  spy->ClearEvents();
  bone->SetWidgetState(vtkBoneWidget::Rest);
  sectionErrors += spy->CalledEvents[0] != vtkCommand::ModifiedEvent;
  sectionErrors += spy->CalledEvents.size() != 1;
  sectionErrors += bone->GetWidgetState() != vtkBoneWidget::Rest;

  spy->ClearEvents();
  bone->SetWidgetState(vtkBoneWidget::Pose);
  sectionErrors += spy->CalledEvents[0] != vtkBoneWidget::PoseChangedEvent;
  sectionErrors += spy->CalledEvents[1] != vtkCommand::ModifiedEvent;
  sectionErrors += spy->CalledEvents.size() != 2;
  sectionErrors += bone->GetWidgetState() != vtkBoneWidget::Pose;

  // Back to rest mode
  spy->ClearEvents();
  bone->SetWidgetState(vtkBoneWidget::Rest);
  sectionErrors += spy->CalledEvents[0] != vtkCommand::ModifiedEvent;
  sectionErrors += spy->CalledEvents.size() != 1;
  sectionErrors += bone->GetWidgetState() != vtkBoneWidget::Rest;

  if (sectionErrors > 0)
    {
    std::cout<<"There were "<<sectionErrors
      <<" while testing the bone widget states."<<std::endl;
    }
  errors += sectionErrors;

  // Quickly change the positions
  bone->SetWorldHeadRest(0.1, 0.0002, 42.0);
  bone->SetWorldTailRest(102.0, 0.0002, -35.0);

  // A renderer and render window
  vtkSmartPointer<vtkRenderer> renderer =
    vtkSmartPointer<vtkRenderer>::New();
  vtkSmartPointer<vtkRenderWindow> renderWindow =
    vtkSmartPointer<vtkRenderWindow>::New();
  renderWindow->AddRenderer(renderer);

  // An interactor
  vtkSmartPointer<vtkRenderWindowInteractor> renderWindowInteractor =
    vtkSmartPointer<vtkRenderWindowInteractor>::New();
  renderWindowInteractor->SetRenderWindow(renderWindow);

  bone->SetInteractor(renderWindowInteractor);
  bone->CreateDefaultRepresentation();
  bone->On();

  //
  // Axes:
  //

  // re-init events
  sectionErrors = 0;
  spy->ClearEvents();
  sectionErrors += bone->GetShowAxes() != vtkBoneWidget::Hidden;
  bone->SetShowAxes(vtkBoneWidget::ShowRestTransform);
  sectionErrors += spy->CalledEvents[0] != vtkCommand::ModifiedEvent;
  sectionErrors += spy->CalledEvents.size() != 1;
  sectionErrors += bone->GetShowAxes() != vtkBoneWidget::ShowRestTransform;

  vtkQuaterniond axesRotation;
  assert(bone->GetAxesActor());
  assert(bone->GetAxesActor()->GetUserTransform());
  vtkTransform* axesTransform =
    vtkTransform::SafeDownCast(bone->GetAxesActor()->GetUserTransform());
  double* wxyz = axesTransform->GetOrientationWXYZ();
  axesRotation.SetRotationAngleAndAxis(vtkMath::RadiansFromDegrees(wxyz[0]),
    wxyz[1], wxyz[2], wxyz[3]);

  sectionErrors += axesRotation.Compare(bone->GetWorldToBoneRestRotation(), 1e-4) != true;
  sectionErrors +=
    CompareVector3(bone->GetWorldTailRest(), axesTransform->GetPosition()) != true;

  spy->ClearEvents();
  bone->SetShowAxes(vtkBoneWidget::ShowPoseTransform);
  sectionErrors += spy->CalledEvents[0] != vtkCommand::ModifiedEvent;
  sectionErrors += spy->CalledEvents.size() != 1;
  sectionErrors += bone->GetShowAxes() != vtkBoneWidget::ShowPoseTransform;

  axesTransform =
    vtkTransform::SafeDownCast(bone->GetAxesActor()->GetUserTransform());
  wxyz = axesTransform->GetOrientationWXYZ();
  axesRotation.SetRotationAngleAndAxis(vtkMath::RadiansFromDegrees(wxyz[0]),
    wxyz[1], wxyz[2], wxyz[3]);

  sectionErrors += axesRotation.Compare(bone->GetWorldToBonePoseRotation(), 1e-4) != true;
  sectionErrors +=
    CompareVector3(bone->GetWorldTailRest(), axesTransform->GetPosition()) != true;

  if (sectionErrors > 0)
    {
    std::cout<<"There were "<<sectionErrors
      <<" while testing the bone widget axes."<<std::endl;
    }
  errors += sectionErrors;

  //
  // Parenthood:
  //

  // re-init events
  sectionErrors = 0;

  sectionErrors += bone->GetShowParenthood() != 1;

  vtkLineRepresentation* parenthood = bone->GetParenthoodRepresentation();
  sectionErrors += CompareVector3(bone->GetWorldToParentRestTranslation(),
    parenthood->GetPoint1WorldPosition()) != true;
  sectionErrors += CompareVector3(bone->GetWorldHeadRest(),
    parenthood->GetPoint2WorldPosition()) != true;

  spy->ClearEvents();
  bone->SetShowParenthood(0);
  sectionErrors += spy->CalledEvents[0] != vtkCommand::ModifiedEvent;
  sectionErrors += spy->CalledEvents.size() != 1;
  sectionErrors += bone->GetShowParenthood() != 0;

  if (sectionErrors > 0)
    {
    std::cout<<"There were "<<sectionErrors
      <<" while testing the bone widget axes."<<std::endl;
    }
  errors += sectionErrors;

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
    std::cout<<"Basic Widget test passed !"<<std::endl;
    }

  return EXIT_SUCCESS;
}
