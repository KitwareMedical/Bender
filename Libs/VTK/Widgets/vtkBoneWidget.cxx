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
#include "vtkBoneWidget.h"

#include "vtkBoneRepresentation.h"

// VTK Includes
#include <vtkAxesActor.h>
#include <vtkCallbackCommand.h>
#include <vtkCamera.h>
#include <vtkCaptionActor2D.h>
#include <vtkCommand.h>
#include <vtkHandleRepresentation.h>
#include <vtkHandleWidget.h>
#include <vtkLineRepresentation.h>
#include <vtkLineWidget2.h>
#include <vtkMath.h>
#include <vtkMatrix3x3.h>
#include <vtkObjectFactory.h>
#include <vtkPointHandleRepresentation3D.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkSmartPointer.h>
#include <vtkTransform.h>
#include <vtkWidgetCallbackMapper.h>
#include <vtkWidgetEvent.h>

vtkStandardNewMacro(vtkBoneWidget);

// Static declaration of the world coordinates
static const double X[3] = {1.0, 0.0, 0.0};
static const double Y[3] = {0.0, 1.0, 0.0};
static const double Z[3] = {0.0, 0.0, 1.0};

namespace
{

void InitializeVector3(double* vec)
{
  vec[0] = 0.0;
  vec[1] = 0.0;
  vec[2] = 0.0;
}

void CopyVector3(const double* vec, double* copyVec)
{
  copyVec[0] = vec[0];
  copyVec[1] = vec[1];
  copyVec[2] = vec[2];
}

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

bool CompareVector2(const double* v1, const double* v2)
{
  double diff[2];
  diff[0] = v1[0] - v2[0];
  diff[1] = v1[1] - v2[1];
  if (diff[0]*diff[0] + diff[1]*diff[1] < 1e-6)
    {
    return true;
    }

  return false;
}

}// End namespace

//----------------------------------------------------------------------------
class vtkBoneWidgetCallback : public vtkCommand
{
public:
  static vtkBoneWidgetCallback *New()
    { return new vtkBoneWidgetCallback; }
  vtkBoneWidgetCallback()
    { this->BoneWidget = 0; }
  virtual void Execute(vtkObject* caller, unsigned long eventId, void* data)
    {
    vtkNotUsed(data);
    switch (eventId)
      {
      case vtkCommand::StartInteractionEvent:
        {
        this->BoneWidget->StartBoneInteraction();
        break;
        }
      case vtkCommand::EndInteractionEvent:
        {
        this->BoneWidget->EndBoneInteraction();
        break;
        }
      }
    }

  vtkBoneWidget *BoneWidget;
};

//----------------------------------------------------------------------------
vtkBoneWidget::vtkBoneWidget()
{
  // Name
  this->Name = "";

  // Widget interaction init.
  this->WidgetState = vtkBoneWidget::PlaceHead;
  this->BoneSelected = vtkBoneWidget::NotSelected;

  // The widgets for moving the end points. They observe this widget (i.e.,
  // this widget is the parent to the handles).
  this->HeadWidget = vtkHandleWidget::New();
  this->HeadWidget->SetPriority(this->Priority-0.01);
  this->HeadWidget->SetParent(this);
  this->HeadWidget->ManagesCursorOff();

  this->TailWidget = vtkHandleWidget::New();
  this->TailWidget->SetPriority(this->Priority-0.01);
  this->TailWidget->SetParent(this);
  this->TailWidget->ManagesCursorOff();

  // Set up the callbacks on the two handles.
  this->HeadWidgetCallback = vtkBoneWidgetCallback::New();
  this->HeadWidgetCallback->BoneWidget = this;
  this->HeadWidget->AddObserver(vtkCommand::StartInteractionEvent,
    this->HeadWidgetCallback, this->Priority);
  this->HeadWidget->AddObserver(vtkCommand::EndInteractionEvent,
    this->HeadWidgetCallback, this->Priority);

  this->TailWidgetCallback = vtkBoneWidgetCallback::New();
  this->TailWidgetCallback->BoneWidget = this;
  this->TailWidget->AddObserver(vtkCommand::StartInteractionEvent,
    this->TailWidgetCallback, this->Priority);
  this->TailWidget->AddObserver(vtkCommand::EndInteractionEvent,
    this->TailWidgetCallback, this->Priority);

  // These are the event callbacks supported by this widget.
  this->CallbackMapper->SetCallbackMethod(vtkCommand::LeftButtonPressEvent,
    vtkWidgetEvent::AddPoint, this, vtkBoneWidget::AddPointAction);
  this->CallbackMapper->SetCallbackMethod(vtkCommand::MouseMoveEvent,
     vtkWidgetEvent::Move, this, vtkBoneWidget::MoveAction);
  this->CallbackMapper->SetCallbackMethod(vtkCommand::LeftButtonReleaseEvent,
    vtkWidgetEvent::EndSelect, this, vtkBoneWidget::EndSelectAction);

  // Bone widget essentials.
  // World positions:
  // - Rest:
  InitializeVector3(this->WorldHeadRest);
  this->WorldTailRest[0] = 1.0;
  this->WorldTailRest[1] = 0.0;
  this->WorldTailRest[2] = 0.0;
  // - Pose:
  CopyVector3(this->WorldHeadRest, this->WorldHeadPose);
  CopyVector3(this->WorldTailRest, this->WorldTailPose);

  // Local Positions:
  // - Rest:
  InitializeVector3(this->LocalHeadRest);
  InitializeVector3(this->LocalTailRest);
  // - Pose:
  InitializeVector3(this->LocalHeadPose);
  InitializeVector3(this->LocalTailPose);
  // Roll Angle:
  this->Roll = 0.0;

  //
  // Transform inits
  //
  // - Rest Transforms:
  //   * Parent To Bone:
  //InitializeQuaternion(this->ParentToBoneRestRotation);
  InitializeVector3(this->ParentToBoneRestTranslation);
  //   * World To Parent:
  InitializeVector3(this->WorldToParentRestTranslation);
  //   * World To Bone:
  InitializeVector3(this->WorldToBoneHeadRestTranslation);
  InitializeVector3(this->WorldToBoneTailRestTranslation);

  // - Pose Transforms:
  //   * Rest To Pose (<-> Rotate Tail around Head):
  //   * Parent To Bone:
  InitializeVector3(this->ParentToBonePoseTranslation);
  //   * World To Parent:
  InitializeVector3(this->WorldToParentPoseTranslation);
  //    * World To Bone:
  InitializeVector3(this->WorldToBoneHeadPoseTranslation);
  InitializeVector3(this->WorldToBoneTailPoseTranslation);

  InitializeVector3(this->InteractionWorldHeadPose);
  InitializeVector3(this->InteractionWorldTailPose);

  // Debug axes init.
  this->AxesVisibility = vtkBoneWidget::Hidden;
  this->AxesActor = vtkAxesActor::New();
  this->AxesActor->SetAxisLabels(0);
  this->AxesSize = 0.4;

  this->ShowParenthood = 1;
  this->ParenthoodLink = vtkLineWidget2::New();

  this->ShouldInitializePoseMode = true;

  this->UpdateAxesVisibility();
  this->UpdateParenthoodLinkVisibility();
}

//----------------------------------------------------------------------------
vtkBoneWidget::~vtkBoneWidget()
{
  if(this->CurrentRenderer)
    {
    this->CurrentRenderer->RemoveActor(this->AxesActor);
    }
  this->AxesActor->Delete();
  this->ParenthoodLink->Delete();

  this->HeadWidget->RemoveObserver(this->HeadWidgetCallback);
  this->HeadWidget->Delete();
  this->HeadWidgetCallback->Delete();

  this->TailWidget->RemoveObserver(this->TailWidgetCallback);
  this->TailWidget->Delete();
  this->TailWidgetCallback->Delete();
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetEnabled(int enabling)
{
  // The handle widgets are not actually enabled until they are placed.
  // The handle widgets take their representation
  // from the vtkBoneRepresentation.
  if ( enabling )
    {
    if ( this->WidgetState == vtkBoneWidget::PlaceHead )
      {
      if (this->WidgetRep)
        {
        this->WidgetRep->SetVisibility(0);
        }

      this->HeadWidget->GetRepresentation()->SetVisibility(0);
      this->TailWidget->GetRepresentation()->SetVisibility(0);
      }
    else
      {
      if (this->WidgetRep)
        {
        this->WidgetRep->SetVisibility(1);
        }

      this->HeadWidget->GetRepresentation()->SetVisibility(1);
      this->TailWidget->GetRepresentation()->SetVisibility(1);
      }

    this->HeadWidget->SetRepresentation(
      vtkBoneRepresentation::SafeDownCast
      (this->WidgetRep)->GetHeadRepresentation());
    this->HeadWidget->SetInteractor(this->Interactor);
    this->HeadWidget->GetRepresentation()->SetRenderer(
      this->CurrentRenderer);

    this->TailWidget->SetRepresentation(
      vtkBoneRepresentation::SafeDownCast
      (this->WidgetRep)->GetTailRepresentation());
    this->TailWidget->SetInteractor(this->Interactor);
    this->TailWidget->GetRepresentation()->SetRenderer(
      this->CurrentRenderer);

    this->ParenthoodLink->SetInteractor(this->Interactor);
    this->ParenthoodLink->SetCurrentRenderer(this->CurrentRenderer);
    }

  this->HeadWidget->SetEnabled(enabling);
  this->TailWidget->SetEnabled(enabling);
  this->Superclass::SetEnabled(enabling);

  this->ParenthoodLink->SetEnabled(enabling);
  this->UpdateParenthoodLinkVisibility();

  // Add/Remove the actor.
  // This needs to be done after enabling the superclass
  // otherwise there isn't a renderer ready.
  if (this->CurrentRenderer)
    {
    if (enabling)
      {
      this->CurrentRenderer->AddActor(this->AxesActor);
      }
    else
      {
      this->CurrentRenderer->RemoveActor(this->AxesActor);
      }
    this->UpdateAxesVisibility();
    }
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetRepresentation(vtkBoneRepresentation* representation)
{
  if (representation && this->WidgetState != vtkBoneWidget::PlaceHead)
    {
    if (this->WidgetState == vtkBoneWidget::Rest)
      {
      representation->SetWorldHeadPosition(this->WorldHeadRest);
      representation->SetWorldTailPosition(this->WorldTailRest);
      }
    else if (this->WidgetState == vtkBoneWidget::Pose)
      {
      representation->SetWorldHeadPosition(this->WorldHeadPose);
      representation->SetWorldTailPosition(this->WorldTailPose);
      }
    else if (this->WidgetState == vtkBoneWidget::PlaceTail)
      {
      representation->SetWorldHeadPosition(this->WorldHeadRest);
      }

    this->InstantiateParenthoodLink();
    }

  this->Superclass::SetWidgetRepresentation(representation);
}

//----------------------------------------------------------------------------
vtkBoneRepresentation* vtkBoneWidget::GetBoneRepresentation()
{
  return vtkBoneRepresentation::SafeDownCast(this->WidgetRep);
}

//----------------------------------------------------------------------------
void vtkBoneWidget::CreateDefaultRepresentation()
{
  // Init the bone.
  if ( ! this->WidgetRep )
    {
    this->WidgetRep = vtkBoneRepresentation::New();
    }

  vtkBoneRepresentation::SafeDownCast(this->WidgetRep)->
    InstantiateHandleRepresentation();
  this->InstantiateParenthoodLink();

  this->UpdateRepresentation();
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetProcessEvents(int pe)
{
  this->Superclass::SetProcessEvents(pe);

  this->HeadWidget->SetProcessEvents(pe);
  this->TailWidget->SetProcessEvents(pe);
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetWidgetState(int state)
{
  state = std::min(3, std::max(state, 0));
  if (state == vtkBoneWidget::PlaceHead || state == vtkBoneWidget::PlaceTail)
    {
    return;
    }
  this->BoneSelected = vtkBoneWidget::NotSelected;
  this->WidgetState = state;

  this->UpdateDisplay();
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetWidgetStateToPose()
{
  this->SetWidgetState(vtkBoneWidget::Pose);
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetWidgetStateToRest()
{
  this->SetWidgetState(vtkBoneWidget::Rest);
}

//----------------------------------------------------------------------------
void vtkBoneWidget::GetCurrentWorldHead(double head[3]) const
{
  const double* currentHead = this->GetCurrentWorldHead();
  if (currentHead)
    {
    CopyVector3(currentHead, head);
    }
  else
    {
    InitializeVector3(head);
    }
}

//----------------------------------------------------------------------------
const double* vtkBoneWidget::GetCurrentWorldHead() const
{
  if (this->WidgetState == vtkBoneWidget::Pose)
    {
    return this->WorldHeadPose;
    }
  else if (this->WidgetState == vtkBoneWidget::Rest
    || this->WidgetState == vtkBoneWidget::PlaceTail)
    {
    return this->WorldHeadRest;
    }

  return NULL;
}

//----------------------------------------------------------------------------
void vtkBoneWidget::GetCurrentWorldTail(double tail[3]) const
{
  const double* currentTail = this->GetCurrentWorldTail();
  if (currentTail)
    {
    CopyVector3(currentTail, tail);
    }
  else
    {
    InitializeVector3(tail);
    }
}

//----------------------------------------------------------------------------
const double* vtkBoneWidget::GetCurrentWorldTail() const
{
  if (this->WidgetState == vtkBoneWidget::Pose)
    {
    return this->WorldTailPose;
    }
  else if (this->WidgetState == vtkBoneWidget::Rest)
    {
    return this->WorldTailRest;
    }

  return NULL;
}

//----------------------------------------------------------------------------
void vtkBoneWidget
::SetWorldToParentRestRotationAndTranslation(double quat[4],
                                             double translate[3])
{
  this->WorldToParentRestRotation.Set(quat);
  CopyVector3(translate, this->WorldToParentRestTranslation);
  this->UpdateRestMode();
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetWorldToParentRestRotation(double quat[4])
{
  this->WorldToParentRestRotation.Set(quat);
  this->UpdateRestMode();
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetWorldToParentRestTranslation(double translate[3])
{
  CopyVector3(translate, this->WorldToParentRestTranslation);
  this->UpdateRestMode(); //probably recomputing rotations for nothing.
}

//----------------------------------------------------------------------------
vtkSmartPointer<vtkTransform>
  vtkBoneWidget::CreateWorldToParentRestTransform() const
{
  vtkSmartPointer<vtkTransform> worldToParentRestTransform =
    vtkSmartPointer<vtkTransform>::New();
  // Translate.
  worldToParentRestTransform->Translate(this->WorldToParentRestTranslation);
  // Rotate.
  worldToParentRestTransform->Concatenate(
    this->CreateWorldToParentRestRotation());

  return worldToParentRestTransform;
}

//----------------------------------------------------------------------------
vtkSmartPointer<vtkTransform>
  vtkBoneWidget::CreateWorldToParentRestRotation() const
{
  vtkSmartPointer<vtkTransform> worldToParentRotation
    = vtkSmartPointer<vtkTransform>::New();

  double axis[3];
  double angle = this->WorldToParentRestRotation.GetRotationAngleAndAxis(axis);
  worldToParentRotation->RotateWXYZ(
    vtkMath::DegreesFromRadians(angle), axis);

  return worldToParentRotation;
}

//----------------------------------------------------------------------------
vtkSmartPointer<vtkTransform>
  vtkBoneWidget::CreateParentToBoneRestTransform() const
{
  vtkSmartPointer<vtkTransform> parentToBoneRestTransform =
    vtkSmartPointer<vtkTransform>::New();
  // Translate.
  parentToBoneRestTransform->Translate(
    this->ParentToBoneRestTranslation);
  // Rotate.
  parentToBoneRestTransform->Concatenate(
    this->CreateParentToBoneRestRotation());

  return parentToBoneRestTransform;
}

//----------------------------------------------------------------------------
vtkSmartPointer<vtkTransform>
  vtkBoneWidget::CreateParentToBoneRestRotation() const
{
  vtkSmartPointer<vtkTransform> parentToBoneRotationTransform
    = vtkSmartPointer<vtkTransform>::New();

  double axis[3];
  double angle = this->ParentToBoneRestRotation.GetRotationAngleAndAxis(axis);
  parentToBoneRotationTransform->RotateWXYZ(
    vtkMath::DegreesFromRadians(angle), axis);

  return parentToBoneRotationTransform;
}

//----------------------------------------------------------------------------
vtkSmartPointer<vtkTransform>
  vtkBoneWidget::CreateWorldToBoneRestTransform() const
{
  vtkSmartPointer<vtkTransform> worldToBoneRestTransform =
    vtkSmartPointer<vtkTransform>::New();
  // Translate.
  worldToBoneRestTransform->Translate(this->WorldToBoneHeadRestTranslation);
  // Rotate.
  worldToBoneRestTransform->Concatenate(
    this->CreateWorldToBoneRestRotation());

  return worldToBoneRestTransform;
}

//----------------------------------------------------------------------------
vtkSmartPointer<vtkTransform>
  vtkBoneWidget::CreateWorldToBoneRestRotation() const
{
  vtkSmartPointer<vtkTransform> worldToBoneRotationTransform
    = vtkSmartPointer<vtkTransform>::New();

  double axis[3];
  double angle = this->WorldToBoneRestRotation.GetRotationAngleAndAxis(axis);
  worldToBoneRotationTransform->RotateWXYZ(
    vtkMath::DegreesFromRadians(angle), axis);

  return worldToBoneRotationTransform;
}

//----------------------------------------------------------------------------
void vtkBoneWidget
::SetWorldToParentPoseRotationAndTranslation(double quat[4],
                                             double translate[3])
{
  this->WorldToParentPoseRotation.Set(quat);
  this->SetWorldToParentPoseTranslation(translate);
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetWorldToParentPoseRotation(double quat[4])
{
  this->WorldToParentPoseRotation.Set(quat);
  this->UpdateWorldPosePositions();
  this->UpdatePoseMode();
}

///----------------------------------------------------------------------------
void vtkBoneWidget::SetWorldToParentPoseTranslation(double translate[3])
{
  CopyVector3(translate, this->WorldToParentPoseTranslation);
  this->UpdateWorldPosePositions();
  this->UpdatePoseMode();
}

//----------------------------------------------------------------------------
vtkSmartPointer<vtkTransform>
  vtkBoneWidget::CreateWorldToBonePoseTransform() const
{
  vtkSmartPointer<vtkTransform> worldToBonePoseTransform =
    vtkSmartPointer<vtkTransform>::New();
  // Translate.
  worldToBonePoseTransform->Translate(this->WorldToBoneHeadPoseTranslation);
  // Rotate.
  worldToBonePoseTransform->Concatenate(
    this->CreateWorldToBonePoseRotation());

  return worldToBonePoseTransform;
}

//----------------------------------------------------------------------------
vtkSmartPointer<vtkTransform>
  vtkBoneWidget::CreateWorldToBonePoseRotation() const
{
  vtkSmartPointer<vtkTransform> worldToBoneRotationTransform
    = vtkSmartPointer<vtkTransform>::New();

  double axis[3];
  double angle = this->WorldToBonePoseRotation.GetRotationAngleAndAxis(axis);
  worldToBoneRotationTransform->RotateWXYZ(
    vtkMath::DegreesFromRadians(angle), axis);

  return worldToBoneRotationTransform;
}

//----------------------------------------------------------------------------
vtkSmartPointer<vtkTransform>
  vtkBoneWidget::CreateWorldToParentPoseTransform() const
{
  vtkSmartPointer<vtkTransform> worldToParentPoseTransform =
    vtkSmartPointer<vtkTransform>::New();
  // Translate.
  worldToParentPoseTransform->Translate(this->WorldToParentPoseTranslation);
  // Rotate.
  worldToParentPoseTransform->Concatenate(
    this->CreateWorldToParentPoseRotation());

  return worldToParentPoseTransform;
}

//----------------------------------------------------------------------------
vtkSmartPointer<vtkTransform>
  vtkBoneWidget::CreateWorldToParentPoseRotation() const
{
  vtkSmartPointer<vtkTransform> worldToParentRotationTransform
    = vtkSmartPointer<vtkTransform>::New();

  double axis[3];
  double angle = this->WorldToParentPoseRotation.GetRotationAngleAndAxis(axis);
  worldToParentRotationTransform->RotateWXYZ(
    vtkMath::DegreesFromRadians(angle), axis);

  return worldToParentRotationTransform;
}

//----------------------------------------------------------------------------
vtkSmartPointer<vtkTransform>
  vtkBoneWidget::CreateParentToBonePoseTransform() const
{
  vtkSmartPointer<vtkTransform> parentToBonePoseTransform =
    vtkSmartPointer<vtkTransform>::New();
  // Translate.
  parentToBonePoseTransform->Translate(this->ParentToBoneRestTranslation);
  // Rotate.
  parentToBonePoseTransform->Concatenate(
    this->CreateParentToBonePoseRotation());

  return parentToBonePoseTransform;
}

//----------------------------------------------------------------------------
vtkSmartPointer<vtkTransform>
  vtkBoneWidget::CreateParentToBonePoseRotation() const
{
  vtkSmartPointer<vtkTransform> parentToBoneRotationTransform
    = vtkSmartPointer<vtkTransform>::New();

  double axis[3];
  double angle = this->ParentToBonePoseRotation.GetRotationAngleAndAxis(axis);
  parentToBoneRotationTransform->RotateWXYZ(
    vtkMath::DegreesFromRadians(angle), axis);

  return parentToBoneRotationTransform;
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetWorldHeadAndTailRest(double head[3], double tail[3])
{
  if (! CompareVector3(this->WorldHeadRest, head))
    {
    CopyVector3(head, this->WorldHeadRest);
    }

  if (! CompareVector3(this->WorldTailRest, tail))
    {
    CopyVector3(tail, this->WorldTailRest);
    }

  this->UpdateRestMode();
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetWorldHeadRest(double x, double y, double z)
{
  double head[3];
  head[0] = x;
  head[1] = y;
  head[2] = z;
  this->SetWorldHeadRest(head);
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetWorldHeadRest(double head[3])
{
  if (CompareVector3(this->WorldHeadRest, head))
    {
    return;
    }

  CopyVector3(head, this->WorldHeadRest);
  this->UpdateRestMode();
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetWorldTailRest(double x, double y, double z)
{
  double tail[3];
  tail[0] = x;
  tail[1] = y;
  tail[2] = z;
  this->SetWorldTailRest(tail);
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetWorldTailRest(double tail[3])
{
  if (CompareVector3(this->WorldTailRest, tail))
    {
    return;
    }

  CopyVector3(tail, this->WorldTailRest);
  this->UpdateRestMode();
}


//----------------------------------------------------------------------------
void vtkBoneWidget::SetDisplayHeadRestPosition(double x, double y)
{
  double displayHead[2];
  displayHead[0] = x;
  displayHead[1] = y;
  this->SetDisplayHeadRestPosition(displayHead);
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetDisplayHeadRestPosition(double displayHead[2])
{
  if (CompareVector2(
      this->GetBoneRepresentation()->GetDisplayHeadPosition(), displayHead))
    {
    return;
    }

  this->GetBoneRepresentation()->SetDisplayHeadPosition(displayHead);
  CopyVector3(
    this->GetBoneRepresentation()->GetWorldHeadPosition(),
    this->WorldHeadRest);

  this->UpdateRestMode();
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetDisplayTailRestPosition(double x, double y)
{
  double displayTail[2];
  displayTail[0] = x;
  displayTail[1] = y;
  this->SetDisplayTailRestPosition(displayTail);
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetDisplayTailRestPosition(double displayTail[2])
{
  if (CompareVector2(
      this->GetBoneRepresentation()->GetDisplayTailPosition(), displayTail))
    {
    return;
    }

  this->GetBoneRepresentation()->SetDisplayTailPosition(displayTail);
  CopyVector3(
    this->GetBoneRepresentation()->GetWorldTailPosition(),
    this->WorldTailRest);

  this->UpdateRestMode();
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetLocalHeadAndTailRest(double head[3], double tail[3])
{
  if (! CompareVector3(this->LocalHeadRest, head))
    {
    CopyVector3(head, this->LocalHeadRest);
    }

  if (! CompareVector3(this->LocalTailRest, tail))
    {
    CopyVector3(tail, this->LocalTailRest);
    }

  this->UpdateRestMode();
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetLocalHeadRest(double x, double y, double z)
{
  double head[3];
  head[0] = x;
  head[1] = y;
  head[2] = z;
  this->SetLocalHeadRest(head);
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetLocalHeadRest(double head[3])
{
  if (CompareVector3(this->LocalHeadRest, head))
    {
    return;
    }

  CopyVector3(head, this->LocalHeadRest);
  this->UpdateWorldRestPositions();
  this->UpdateRestMode(); // Rebuild local points again :(.
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetLocalTailRest(double x, double y, double z)
{
  double tail[3];
  tail[0] = x;
  tail[1] = y;
  tail[2] = z;
  this->SetLocalTailRest(tail);
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetLocalTailRest(double tail[3])
{
  if (CompareVector3(this->LocalTailRest, tail))
    {
    return;
    }

  CopyVector3(tail, this->LocalTailRest);
  this->UpdateWorldRestPositions();
  this->UpdateRestMode(); // Rebuild local points again :(.
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetAxesVisibility(int visibility)
{
  if (this->AxesVisibility == visibility)
    {
    return;
    }

  this->AxesVisibility = visibility;
  this->UpdateAxesVisibility();
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkBoneWidget::RotateTailX(double angle)
{
  this->RotateTailWXYZ(angle, 1.0, 0.0, 0.0);
}

//----------------------------------------------------------------------------
void vtkBoneWidget::RotateTailY(double angle)
{
  this->RotateTailWXYZ(angle, 0.0, 1.0, 0.0);
}

//----------------------------------------------------------------------------
void vtkBoneWidget::RotateTailZ(double angle)
{
  this->RotateTailWXYZ(angle, 0.0, 0.0, 1.0);
}

//----------------------------------------------------------------------------
void vtkBoneWidget::RotateTailWXYZ(double angle, double x, double y, double z)
{
  double axis[3];
  axis[0] = x;
  axis[1] = y;
  axis[2] = z;
  this->RotateTailWXYZ(angle, axis);
}

//----------------------------------------------------------------------------
void vtkBoneWidget::RotateTailWXYZ(double angle, double axis[3])
{
  double newTail[3];
  this->RotateTail(angle, axis, newTail);

  if (this->WidgetState == vtkBoneWidget::Pose)
    {
    CopyVector3(newTail, this->WorldTailPose);

    // Update local pose tail to new position.
    this->RebuildLocalTailPose();

    vtkQuaterniond rotation;
    rotation.SetRotationAngleAndAxis(angle, axis);
    rotation.Normalize();
    this->ParentToBonePoseRotation = rotation * this->ParentToBonePoseRotation;
    this->ParentToBonePoseRotation.Normalize();

    this->UpdatePoseMode();
    }
  else
    {
    this->SetWorldTailRest(newTail);
    }
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetShowParenthood(int parenthood)
{
  if (this->ShowParenthood == parenthood)
    {
    return;
    }

  this->ShowParenthood = parenthood;
  this->UpdateParenthoodLinkVisibility();

  this->Modified();
}

//----------------------------------------------------------------------------
void vtkBoneWidget::ResetPoseToRest()
{
  this->ShouldInitializePoseMode = true;
  this->UpdatePoseMode();
}

//----------------------------------------------------------------------------
void vtkBoneWidget::AddPointAction(vtkAbstractWidget *w)
{
  vtkBoneWidget *self = vtkBoneWidget::SafeDownCast(w);

  int X = self->Interactor->GetEventPosition()[0];
  int Y = self->Interactor->GetEventPosition()[1];
  double e[2];
  e[0] = static_cast<double>(X);
  e[1] = static_cast<double>(Y);

  // If we are placing the first point it's easy.
  if ( self->WidgetState == vtkBoneWidget::PlaceHead )
    {
    self->GrabFocus(self->EventCallbackCommand);
    self->WidgetState = vtkBoneWidget::PlaceTail;
    self->InvokeEvent(vtkCommand::StartInteractionEvent,NULL);

    // Place Point yourself.
    vtkBoneRepresentation::SafeDownCast(self->WidgetRep)->SetDisplayHeadPosition(e);
    CopyVector3(self->GetBoneRepresentation()->GetWorldHeadPosition(), self->WorldHeadRest);
    CopyVector3(self->WorldHeadRest, self->WorldHeadPose);
    self->HeadWidget->SetEnabled(1);
    self->Modified();
    }

  // If defining we are placing the second or third point.
  else if (self->WidgetState == vtkBoneWidget::PlaceTail)
    {
    // Place Point.
    self->WidgetState = vtkBoneWidget::Rest;

    self->TailWidget->SetEnabled(1);
    self->TailWidget->GetRepresentation()->SetVisibility(1);
    self->WidgetRep->SetVisibility(1);

    self->SetDisplayTailRestPosition(e);
    CopyVector3(self->WorldTailRest, self->WorldTailPose);
    }

  else if ( self->WidgetState == vtkBoneWidget::Rest 
            || self->WidgetState == vtkBoneWidget::Pose )
    {
    self->BoneSelected = vtkBoneWidget::NotSelected;

    int modifier =
      self->Interactor->GetShiftKey() | self->Interactor->GetControlKey();
    int state = self->WidgetRep->ComputeInteractionState(X,Y,modifier);
    if ( state == vtkBoneRepresentation::Outside )
      {
      return;
      }

    self->GrabFocus(self->EventCallbackCommand);
    if (state == vtkBoneRepresentation::OnHead)
      {
      self->SetWidgetSelectedState(vtkBoneWidget::HeadSelected);
      self->InvokeEvent(vtkCommand::LeftButtonPressEvent,NULL);
      }
    else if (state == vtkBoneRepresentation::OnTail)
      {
      self->SetWidgetSelectedState(vtkBoneWidget::TailSelected);
      self->InvokeEvent(vtkCommand::LeftButtonPressEvent,NULL);
      }
    else if (state == vtkBoneRepresentation::OnLine)
      {
      if (self->WidgetState == vtkBoneWidget::Rest)
        {
        self->SetWidgetSelectedState(vtkBoneWidget::LineSelected);
        self->WidgetRep->StartWidgetInteraction(e);
        self->InvokeEvent(vtkCommand::LeftButtonPressEvent,NULL);
        }
      }
    }

  self->EventCallbackCommand->SetAbortFlag(1);
  self->Render();
}

//----------------------------------------------------------------------------
void vtkBoneWidget::MoveAction(vtkAbstractWidget *w)
{
  vtkBoneWidget *self = vtkBoneWidget::SafeDownCast(w);

  // Do nothing if outside.
  if ( self->WidgetState == vtkBoneWidget::PlaceHead )
    {
    return;
    }

  // Delegate the event consistent with the state.
  int X = self->Interactor->GetEventPosition()[0];
  int Y = self->Interactor->GetEventPosition()[1];
  double e[2];
  e[0] = static_cast<double>(X);
  e[1] = static_cast<double>(Y);

  if ( self->WidgetState == vtkBoneWidget::PlaceTail )
    {
    self->InvokeEvent(vtkCommand::InteractionEvent,NULL);
    self->EventCallbackCommand->SetAbortFlag(1);
    }

  else if (self->WidgetState == vtkBoneWidget::Rest
    && self->BoneSelected != vtkBoneWidget::NotSelected)
    {
    if (self->BoneSelected == vtkBoneWidget::HeadSelected)
      {
      self->SetDisplayHeadRestPosition(e);
      }
    else if (self->BoneSelected == vtkBoneWidget::TailSelected)
      {
      self->SetDisplayTailRestPosition(e);
      }

    else if (self->BoneSelected == vtkBoneWidget::LineSelected)
      {
      self->GetBoneRepresentation()
        ->GetLineHandleRepresentation()->SetDisplayPosition(e);
      self->GetBoneRepresentation()->WidgetInteraction(e);

      self->SetWorldHeadAndTailRest(
        self->GetBoneRepresentation()->GetWorldHeadPosition(),
        self->GetBoneRepresentation()->GetWorldTailPosition());
      }

    self->InvokeEvent(vtkCommand::InteractionEvent,NULL);
    }
  else if (self->WidgetState == vtkBoneWidget::Pose
    && self->BoneSelected == vtkBoneWidget::TailSelected)
    {
    //
    // Make rotation in camera view plane center on Head.
    //

    // Get display positions
    double e1[2];
    self->GetBoneRepresentation()->GetDisplayHeadPosition(e1);

    // Get the current line (-> the line between Head and the event)
    // in display coordinates.
    double currentLine[2], oldLine[2];
    currentLine[0] = e[0] - e1[0];
    currentLine[1] = e[1] - e1[1];
    vtkMath::Normalize2D(currentLine);

    // Get the old line (-> the line between Head and the LAST event)
    // in display coordinates.
    int lastX = self->Interactor->GetLastEventPosition()[0];
    int lastY = self->Interactor->GetLastEventPosition()[1];
    double lastE[2];
    lastE[0] = static_cast<double>(lastX);
    lastE[1] = static_cast<double>(lastY);
    oldLine[0] = lastE[0] - e1[0];
    oldLine[1] = lastE[1] - e1[1];
    vtkMath::Normalize2D(oldLine);

    // Get the angle between those two lines.
    double angle = vtkMath::DegreesFromRadians(
                     acos(vtkMath::Dot2D(currentLine, oldLine)));

    //Get the camera vector.
    double cameraVec[3];
    if (!self->GetCurrentRenderer()
        || !self->GetCurrentRenderer()->GetActiveCamera())
      {
      vtkErrorWithObjectMacro(self,
        "There should be a renderer and a camera. Make sure to set these !"
        "\n ->Cannot move Tail in pose mode");
      return;
      }
    self->GetCurrentRenderer()->GetActiveCamera()
      ->GetDirectionOfProjection(cameraVec);

    // Need to figure if the rotation is clockwise or counterclowise.
    double spaceCurrentLine[3], spaceOldLine[3];
    spaceCurrentLine[0] = currentLine[0];
    spaceCurrentLine[1] = currentLine[1];
    spaceCurrentLine[2] = 0.0;

    spaceOldLine[0] = oldLine[0];
    spaceOldLine[1] = oldLine[1];
    spaceOldLine[2] = 0.0;

    double handenessVec[3];
    vtkMath::Cross(spaceOldLine, spaceCurrentLine, handenessVec);

    // Handeness is opposite beacuse camera is toward the focal point.
    double handeness = vtkMath::Dot(handenessVec, Z) > 0 ? -1.0: 1.0;
    angle *= handeness;

    // Finally rotate tail
    // \TO DO vvvvvvvvvvvvvvvv POSSIBLE REFACTORING vvvvvvvvvvvvvvvv
    // The tranform inside is ParentToBone isn't it ?!?
    self->RotateTail(angle, cameraVec, self->WorldTailPose);
    // \TO DO ^^^^^^^^^^^^^^^^ POSSIBLE REFACTORING ^^^^^^^^^^^^^^^^
    self->RebuildLocalTailPose();

    self->RebuildWorldToBonePoseRotationInteraction();

    // Update translations:
    self->RebuildWorldToBonePoseTranslations();

    // Finaly update representation and propagate
    self->UpdateDisplay();

    self->InvokeEvent(vtkBoneWidget::PoseChangedEvent, NULL);
    self->InvokeEvent(vtkCommand::InteractionEvent,NULL);
    self->Modified();
    }

  self->WidgetRep->BuildRepresentation();
  self->Render();
}

//----------------------------------------------------------------------------
void vtkBoneWidget::EndSelectAction(vtkAbstractWidget *w)
{
  vtkBoneWidget *self = vtkBoneWidget::SafeDownCast(w);

  if (self->BoneSelected == vtkBoneWidget::NotSelected)
    {
    return;
    }

  // Do nothing if outside.
  if ( self->WidgetState == vtkBoneWidget::PlaceHead ||
       self->WidgetState == vtkBoneWidget::PlaceTail )
    {
    return;
    }

  self->SetWidgetSelectedState(vtkBoneWidget::NotSelected);
  self->ReleaseFocus();
  self->WidgetRep->BuildRepresentation();
  int state = self->WidgetRep->GetInteractionState();
  if ( state == vtkBoneRepresentation::OnHead ||
       state == vtkBoneRepresentation::OnTail ||
       state == vtkBoneRepresentation::OnLine )
    {
    self->InvokeEvent(vtkCommand::LeftButtonReleaseEvent,NULL);
    }
  else
    {
    self->EndBoneInteraction();
    }

  self->EventCallbackCommand->SetAbortFlag(1);
  self->Render();
}

//----------------------------------------------------------------------------
void vtkBoneWidget::RebuildAxes()
{
  // Only update axes if they are visible to prevent unecessary computation.
  if (! this->AxesActor->GetVisibility())
    {
    return;
    }

  double distance = 
   vtkMath::Distance2BetweenPoints(this->WorldHeadRest, this->WorldTailRest)
     * this->AxesSize; // Rest because distance shouldn't change in pose mode
                       // anyway
  this->AxesActor->SetTotalLength(distance, distance, distance);

  vtkSmartPointer<vtkTransform> transform =
    vtkSmartPointer<vtkTransform>::New();
  transform->Translate(this->GetCurrentWorldTail());

  if (this->AxesVisibility == vtkBoneWidget::ShowRestTransform)
    {
    transform->Concatenate(this->CreateWorldToBoneRestRotation());
    }
  else if (this->AxesVisibility == vtkBoneWidget::ShowPoseTransform)
    {
    transform->Concatenate(this->CreateWorldToBonePoseRotation());
    }

  this->AxesActor->SetUserTransform(transform);
}

//----------------------------------------------------------------------------
void vtkBoneWidget::UpdateAxesVisibility()
{
  if (this->AxesVisibility == vtkBoneWidget::Hidden
      || this->WidgetState == vtkBoneWidget::PlaceHead
      || this->WidgetState == vtkBoneWidget::PlaceTail
      || this->Enabled == 0)
    {
    this->AxesActor->SetVisibility(0);
    }
  else
    {
    this->AxesActor->SetVisibility(1);
    }

  this->RebuildAxes();
}

//----------------------------------------------------------------------------
void vtkBoneWidget::UpdateParenthoodLinkVisibility()
{
  if (this->ParenthoodLink->GetLineRepresentation())
    {
    if (this->ShowParenthood
      && this->Enabled
      && this->GetBoneRepresentation()
      && (this->WidgetState == vtkBoneWidget::Rest
        || this->WidgetState == vtkBoneWidget::Pose))
      {
      this->ParenthoodLink->GetLineRepresentation()->SetVisibility(1);
      }
    else
      {
      this->ParenthoodLink->GetLineRepresentation()->SetVisibility(0);
      }

    this->RebuildParenthoodLink();
    }
}

//----------------------------------------------------------------------------
void vtkBoneWidget::RebuildParenthoodLink()
{
  if (this->ParenthoodLink->GetLineRepresentation()
    && this->ParenthoodLink->GetLineRepresentation()->GetVisibility())
    {
    if (this->WidgetState == vtkBoneWidget::Rest)
      {
      this->ParenthoodLink->GetLineRepresentation()->SetPoint1WorldPosition(
        this->GetWorldToParentRestTranslation());
      this->ParenthoodLink->GetLineRepresentation()->SetPoint2WorldPosition(
        this->WorldHeadRest);
      }
    else if (this->WidgetState == vtkBoneWidget::Pose)
      {
      this->ParenthoodLink->GetLineRepresentation()->SetPoint1WorldPosition(
          this->GetWorldToParentPoseTranslation());
      this->ParenthoodLink->GetLineRepresentation()->SetPoint2WorldPosition(
          this->WorldHeadPose);
      }
    }
}

//----------------------------------------------------------------------------
void vtkBoneWidget::InstantiateParenthoodLink()
{
  // The parent line.
  this->ParenthoodLink->SetInteractor(this->Interactor);
  this->ParenthoodLink->SetCurrentRenderer(this->CurrentRenderer);
  this->ParenthoodLink->CreateDefaultRepresentation();

  // Dotted line.
  this->ParenthoodLink->GetLineRepresentation()
    ->GetLineProperty()->SetLineStipplePattern(0x000f);
  this->ParenthoodLink->SetProcessEvents(0);
  this->UpdateParenthoodLinkVisibility();
}

//----------------------------------------------------------------------------
void vtkBoneWidget::RebuildParentToBoneRestRotation()
{
  // We always have WorldToBone = WorldToParent * ParentToBone
  // then ParentToBone = WorldToParent^(-1) * WorldToBone
  // Plus, inverting a quaternion isn't so bad (conjugation + normalization)
  vtkQuaterniond parentToWorldRestRotation
    = this->WorldToParentRestRotation.Inverse();

  this->ParentToBoneRestRotation
    = parentToWorldRestRotation * this->WorldToBoneRestRotation;
  this->ParentToBoneRestRotation.Normalize();
}

//----------------------------------------------------------------------------
void vtkBoneWidget::RebuildWorldToBoneRestRotation()
{
  this->WorldToBoneRestRotation = this->ComputeRotationFromReferenceAxis(Y);
}

//----------------------------------------------------------------------------
void vtkBoneWidget::RebuildParentToBoneRestTranslation()
{
  CopyVector3(this->LocalHeadRest, this->ParentToBoneRestTranslation);
}


//----------------------------------------------------------------------------
void vtkBoneWidget::RebuildWorldToBoneRestTranslations()
{
  CopyVector3(this->WorldHeadRest, this->WorldToBoneHeadRestTranslation);
  CopyVector3(this->WorldTailRest, this->WorldToBoneTailRestTranslation);
}

//----------------------------------------------------------------------------
void vtkBoneWidget::RebuildParentToBonePoseTranslation()
{
  CopyVector3(this->LocalHeadPose, this->ParentToBonePoseTranslation);
}

//----------------------------------------------------------------------------
void vtkBoneWidget::RebuildWorldToBonePoseTranslations()
{
  CopyVector3(this->WorldHeadPose, this->WorldToBoneHeadPoseTranslation);
  CopyVector3(this->WorldTailPose, this->WorldToBoneTailPoseTranslation);
}

//----------------------------------------------------------------------------
vtkQuaterniond vtkBoneWidget
::ComputeRotationFromReferenceAxis(const double* axis)
{
  vtkQuaterniond newOrientation;
  // Code greatly inspired by: http://www.fastgraph.com/makegames/3drotation/ .

  double viewOut[3]; // The View or "new Z" vector.
  double viewUp[3]; // The Up or "new Y" vector.
  double viewRight[3]; // The Right or "new X" vector.

  double upMagnitude; // For normalizing the Up vector.
  double upProjection; // Magnitude of projection of View Vector on World UP.

  // First: calculate and normalize the view vector.
  vtkMath::Subtract(this->WorldTailRest, this->WorldHeadRest, viewOut);

  // Normalize. This is the unit vector in the "new Z" direction.
  if (vtkMath::Normalize(viewOut) < 0.000001)
    {
    vtkErrorMacro("Tail and Head are not enough apart,"
      " could not rebuild rest Transform");
    return newOrientation;
    }

  // Now the hard part: The ViewUp or "new Y" vector.

  // The dot product of ViewOut vector and World Up vector gives projection of
  // of ViewOut on WorldUp.
  upProjection = vtkMath::Dot(viewOut, axis);

  // First try at making a View Up vector: use World Up.
  viewUp[0] = Y[0] - upProjection*viewOut[0];
  viewUp[1] = Y[1] - upProjection*viewOut[1];
  viewUp[2] = Y[2] - upProjection*viewOut[2];

  // Check for validity:
  upMagnitude = vtkMath::Norm(viewUp);

  if (upMagnitude < 0.0000001)
    {
    // Second try at making a View Up vector: Use Y axis default  (0,1,0).
    viewUp[0] = -viewOut[1]*viewOut[0];
    viewUp[1] = 1-viewOut[1]*viewOut[1];
    viewUp[2] = -viewOut[1]*viewOut[2];

    // Check for validity:
    upMagnitude = vtkMath::Norm(viewUp);

    if (upMagnitude < 0.0000001)
      {
      // Final try at making a View Up vector: Use Z axis default  (0,0,1).
      viewUp[0] = -viewOut[2]*viewOut[0];
      viewUp[1] = -viewOut[2]*viewOut[1];
      viewUp[2] = 1-viewOut[2]*viewOut[2];

      // Check for validity:
      upMagnitude = vtkMath::Norm(viewUp);

      if (upMagnitude < 0.0000001)
        {
        vtkErrorMacro("Could not fin a vector perpendiculare to the bone,"
          " check the bone values. This should not be happening.");
        return newOrientation;
        }
      }
    }

  // Normalize the Up Vector.
  upMagnitude = vtkMath::Normalize(viewUp);

  // Calculate the Right Vector. Use cross product of Out and Up.
  vtkMath::Cross(viewUp, viewOut,  viewRight);
  vtkMath::Normalize(viewRight); //Let's be paranoid about the normalization.

  // Get the rest transform matrix.
  newOrientation.SetRotationAngleAndAxis(acos(upProjection), viewRight);
  newOrientation.Normalize();

  if (this->Roll != 0.0)
    {
    // Get the roll matrix.
    vtkQuaterniond rollQuad;
    rollQuad.SetRotationAngleAndAxis(this->Roll, viewOut);
    rollQuad.Normalize();

    // Get final matrix.
    newOrientation = rollQuad * newOrientation;
    newOrientation.Normalize();
    }

  return newOrientation;
}

//----------------------------------------------------------------------------
void vtkBoneWidget::RebuildLocalRestPoints()
{
  vtkSmartPointer<vtkTransform> transform =
    this->CreateWorldToParentRestTransform();
  transform->Inverse();

  double* head = transform->TransformDoublePoint(this->WorldHeadRest);
  CopyVector3(head, this->LocalHeadRest);

  double* tail = transform->TransformDoublePoint(this->WorldTailRest);
  CopyVector3(tail, this->LocalTailRest);
}

//----------------------------------------------------------------------------
void vtkBoneWidget::RebuildLocalPosePoints()
{
  vtkSmartPointer<vtkTransform> transform =
    this->CreateWorldToParentPoseTransform();
  transform->Inverse();

  double* head = transform->TransformDoublePoint(this->WorldHeadPose);
  CopyVector3(head, this->LocalHeadPose);

  double* tail = transform->TransformDoublePoint(this->WorldTailPose);
  CopyVector3(tail, this->LocalTailPose);
}

//----------------------------------------------------------------------------
void vtkBoneWidget::RebuildLocalTailPose()
{
  // Update local pose tail to new position
  vtkSmartPointer<vtkTransform> transform =
    this->CreateWorldToParentPoseTransform();
  transform->Inverse();

  double* tail = transform->TransformDoublePoint(this->WorldTailPose);
  CopyVector3(tail, this->LocalTailPose);
}

//----------------------------------------------------------------------------
void vtkBoneWidget::RebuildParentToBonePoseRotation()
{
  // We always have WorldToBone = WorldToParent * ParentToBone
  // then ParentToBone = WorldToParent^(-1) * WorldToBone
  // Plus, inverting a quaternion isn't so bad (conjugation + normalization)

  vtkQuaterniond parentToWorldPoseRotation
    = this->WorldToParentPoseRotation.Inverse();

  this->ParentToBonePoseRotation
    = parentToWorldPoseRotation * this->WorldToBonePoseRotation;
  this->ParentToBonePoseRotation.Normalize();
}

//----------------------------------------------------------------------------
void vtkBoneWidget::RebuildWorldToBonePoseRotationFromParent()
{
  this->WorldToBonePoseRotation =
    this->WorldToParentPoseRotation * this->ParentToBonePoseRotation;
  this->WorldToBonePoseRotation.Normalize();
}

//----------------------------------------------------------------------------
bool vtkBoneWidget
::ShouldUseCameraAxisForPoseTransform(const double* vec1, const double* vec2)
{
  if (! (this->GetCurrentRenderer()
      && this->GetCurrentRenderer()->GetActiveCamera()))
    {
    return false;
    }

  // If either head or tail is selected, that means we are in an
  // interaction state. This means that the bone is only moving within
  // the camera plane (Interaction is made so).
  if (this->BoneSelected == vtkBoneWidget::HeadSelected
      || this->BoneSelected == vtkBoneWidget::TailSelected)
    {
    return true;
    }

  double cameraView[3];
  this->GetCurrentRenderer()->GetActiveCamera()
    ->GetDirectionOfProjection(cameraView);
  vtkMath::Normalize(cameraView);

  if (fabs( vtkMath::Dot(cameraView, vec1) ) < 1e-6
    && fabs( vtkMath::Dot(cameraView, vec2) ) < 1e-6)
    {
    return true;
    }

  return false;
}

//----------------------------------------------------------------------------
void vtkBoneWidget::
RotateTail(double angle, double axis[3], double newTail[3])
{
  vtkSmartPointer<vtkTransform> transformTail
    = vtkSmartPointer<vtkTransform>::New();
  transformTail->Translate(this->GetCurrentWorldHead());
  transformTail->RotateWXYZ(angle, axis);
  double minusHead[3];
  CopyVector3(this->GetCurrentWorldHead(), minusHead);
  vtkMath::MultiplyScalar(minusHead, -1.0);
  transformTail->Translate(minusHead);

  CopyVector3(
    transformTail->TransformDoublePoint(this->GetCurrentWorldTail()),
    newTail);
}

//----------------------------------------------------------------------------
void vtkBoneWidget::RebuildWorldToBonePoseRotationInteraction()
{
  // Cumulative technique is simple but causes drift :(
  // That is why we need to recompute each time.
  // The old pose transform represents the sum of all the other
  // previous transformations.

  double previousLineVect[3], newLineVect[3];
  double rotationAxis[3], poseAngle;

  // 2- Get the previous line directionnal vector
  vtkMath::Subtract(this->InteractionWorldTailPose,
    this->InteractionWorldHeadPose, previousLineVect);
  vtkMath::Normalize(previousLineVect);

  // 3- Get the new line vector
  vtkMath::Subtract(this->WorldTailPose, this->WorldHeadPose, newLineVect);
  vtkMath::Normalize(newLineVect);

  // We want to use the camrea as a rotation axis if we can.
  if (this->ShouldUseCameraAxisForPoseTransform(newLineVect, previousLineVect))
    {
    // 4- Compute Rotation Axis
    this->GetCurrentRenderer()->GetActiveCamera()
      ->GetDirectionOfProjection(rotationAxis);
    vtkMath::Normalize(rotationAxis);// Let's be paranoid about normalization.

    // 4- Compute Angle
    double rotationPlaneAxis1[3], rotationPlaneAxis2[3];
    vtkMath::Perpendiculars(rotationAxis,
      rotationPlaneAxis1, rotationPlaneAxis2, 0.0);
    
    //Let's be paranoid about normalization
    vtkMath::Normalize(rotationPlaneAxis1);
    vtkMath::Normalize(rotationPlaneAxis2);

    // The angle is the difference between the old angle and the new angle.
    // Doing this difference enables us to not care about the possible roll
    // of the camera.
    double newVectAngle =
      atan2(vtkMath::Dot(newLineVect, rotationPlaneAxis2),
        vtkMath::Dot(newLineVect, rotationPlaneAxis1));
    double previousVectAngle =
      atan2(vtkMath::Dot(previousLineVect, rotationPlaneAxis2),
        vtkMath::Dot(previousLineVect, rotationPlaneAxis1));
    poseAngle = newVectAngle - previousVectAngle;
    }
  else // This else is here for now but maybe we should just
       // output an error message ?
       // I do not think this code would work properly.
    {
    // 4- Compute Rotation Axis
    vtkMath::Cross(previousLineVect, newLineVect, rotationAxis);
    vtkMath::Normalize(rotationAxis);

    // 4- Compute Angle
    poseAngle = acos(vtkMath::Dot(newLineVect, previousLineVect));
    }

  // PoseTransform is the sum of the transform applied to the bone in
  // pose mode. The previous transforms are stored in StartPoseTransform.
  vtkQuaterniond quad;
  quad.SetRotationAngleAndAxis(poseAngle, rotationAxis);
  quad.Normalize();

  this->WorldToBonePoseRotation = quad * this->StartPoseRotation;
  this->WorldToBonePoseRotation.Normalize();

  this->RebuildParentToBonePoseRotation();
}

//----------------------------------------------------------------------------
void vtkBoneWidget::StartBoneInteraction()
{
  this->Superclass::StartInteraction();

  if (this->WidgetState == vtkBoneWidget::Pose)
    {
    this->UpdatePoseIntercationsVariables();
    }

  this->InvokeEvent(vtkCommand::StartInteractionEvent,NULL);
}

//----------------------------------------------------------------------------
void vtkBoneWidget::EndBoneInteraction()
{
  this->Superclass::EndInteraction();

  if (this->WidgetState == vtkBoneWidget::Pose)
    {
    this->UpdatePoseIntercationsVariables();
    }

  this->InvokeEvent(vtkCommand::EndInteractionEvent,NULL);
}

//----------------------------------------------------------------------------
void vtkBoneWidget::UpdateRestMode()
{
  // Update Rotations:
  // Little weird here: Since we know that our world to bone computations is
  // robust and works well, we compute the world to bone transform first
  // and then deduce the parent to bone transform from it:
  this->RebuildWorldToBoneRestRotation();
  this->RebuildParentToBoneRestRotation();

  // Finally recompute local points.
  this->RebuildLocalRestPoints();

  // Update translations:
  this->RebuildWorldToBoneRestTranslations();
  this->RebuildParentToBoneRestTranslation();

  this->UpdateDisplay();

  this->InvokeEvent(vtkBoneWidget::RestChangedEvent, NULL);
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkBoneWidget::UpdatePoseMode()
{
  if (this->ShouldInitializePoseMode)
    {
    this->InitializePoseMode();
    this->ShouldInitializePoseMode = false;
    }

  this->RebuildWorldToBonePoseRotationFromParent();

  if (this->WidgetState == vtkBoneWidget::Rest)
    {
    this->RebuildWorldPosePointsFromWorldRestPoints();
    this->RebuildLocalPosePoints();
    }

  // Update translations:
  this->RebuildWorldToBonePoseTranslations();
  this->RebuildParentToBonePoseTranslation();

  // Finaly update representation and propagate
  this->UpdateDisplay();

  this->InvokeEvent(vtkBoneWidget::PoseChangedEvent, NULL);
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkBoneWidget::UpdateWorldRestPositions()
{
  vtkSmartPointer<vtkTransform> transform =
    this->CreateWorldToParentRestTransform();

  double* newHead = transform->TransformDoublePoint(this->LocalHeadRest);
  CopyVector3(newHead, this->WorldHeadRest);

  double* newTail = transform->TransformDoublePoint(this->LocalTailRest);
  CopyVector3(newTail, this->WorldTailRest);
}

//----------------------------------------------------------------------------
void vtkBoneWidget::UpdateWorldPosePositions()
{
  vtkSmartPointer<vtkTransform> transform =
    this->CreateWorldToParentPoseTransform();

  double* newHead = transform->TransformDoublePoint(this->LocalHeadPose);
  CopyVector3(newHead, this->WorldHeadPose);

  double* newTail = transform->TransformDoublePoint(this->LocalTailPose);
  CopyVector3(newTail, this->WorldTailPose);
}

//----------------------------------------------------------------------------
void vtkBoneWidget::UpdatePoseIntercationsVariables()
{
  CopyVector3(this->WorldHeadPose, this->InteractionWorldHeadPose);
  CopyVector3(this->WorldTailPose, this->InteractionWorldTailPose);
  this->StartPoseRotation = this->WorldToBonePoseRotation;
}

//----------------------------------------------------------------------------
void vtkBoneWidget::RebuildWorldPosePointsFromWorldRestPoints()
{
  // Need to rebuild the pose mode as a function of the the rest mode.
  double lineVect[3];
  vtkMath::Subtract(this->WorldTailRest, this->WorldHeadRest, lineVect);
  double distance = vtkMath::Normalize(lineVect);

  // Head is given by the position of the local rest in the parent
  // coordinates sytem (pose+rest !).
  vtkSmartPointer<vtkTransform> headTransform =
    this->CreateWorldToParentPoseTransform();
  double* newHead = headTransform->TransformDoublePoint(this->LocalHeadRest);
  CopyVector3(newHead, this->WorldHeadPose);

  // Let's create the Y directionnal vector of the bone.
  // Create transform.
  vtkSmartPointer<vtkTransform> rotateWorldY =
    this->CreateWorldToBonePoseRotation();

  // Rotate Y.
  double* newY = rotateWorldY->TransformDoubleVector(Y);

  // The tail is given by the new Y direction (scaled to the correct
  // distance of course) and added to the new head position.
  double newTail[3];

  vtkMath::MultiplyScalar(newY, distance);
  vtkMath::Add(newHead, newY, newTail);
  CopyVector3(newTail, this->WorldTailPose);
}

//----------------------------------------------------------------------------
void vtkBoneWidget::UpdateRepresentation()
{
  vtkBoneRepresentation* rep = this->GetBoneRepresentation();
  if (rep)
    {
    double head[3];
    this->GetCurrentWorldHead(head);
    rep->SetWorldHeadPosition(head);

    double tail[3];
    this->GetCurrentWorldTail(tail);
    rep->SetWorldTailPosition(tail);
    }
}

//----------------------------------------------------------------------------
void vtkBoneWidget::UpdateDisplay()
{
  this->UpdateRepresentation();
  this->RebuildAxes();
  this->RebuildParenthoodLink();
}

//----------------------------------------------------------------------------
void vtkBoneWidget::InitializePoseMode()
{
  // World to Parent.
  this->WorldToParentPoseRotation = this->WorldToParentRestRotation;
  CopyVector3(this->WorldToParentRestTranslation,
    this->WorldToParentPoseTranslation);

  // Parent to Bone.
  this->ParentToBonePoseRotation = this->ParentToBoneRestRotation;
  CopyVector3(this->ParentToBoneRestTranslation,
    this->ParentToBonePoseTranslation);

  // World to Bone.
  this->WorldToBonePoseRotation = this->WorldToBoneRestRotation;
  CopyVector3(this->WorldToBoneHeadRestTranslation,
    this->WorldToBoneHeadPoseTranslation);
  CopyVector3(this->WorldToBoneTailRestTranslation,
    this->WorldToBoneTailPoseTranslation);

  // World Positions.
  CopyVector3(this->WorldHeadRest, this->WorldHeadPose);
  CopyVector3(this->WorldTailRest, this->WorldTailPose);

  // Local Positions.
  CopyVector3(this->LocalHeadRest, this->LocalHeadPose);
  CopyVector3(this->LocalTailRest, this->LocalTailPose);
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetWidgetSelectedState(int selectionState)
{
  if (!this->GetBoneRepresentation())
    {
    return;
    }

  this->BoneSelected = selectionState;
  if (selectionState == vtkBoneWidget::HeadSelected)
    {
    this->GetBoneRepresentation()->GetHeadRepresentation()->Highlight(1);
    }
  else if (selectionState == vtkBoneWidget::TailSelected)
    {
    this->GetBoneRepresentation()->GetTailRepresentation()->Highlight(1);
    }
  else if (selectionState == vtkBoneWidget::LineSelected)
    {
    this->GetBoneRepresentation()->Highlight(1);
    }
  else
    {
    this->GetBoneRepresentation()->Highlight(0);
    }
}

//----------------------------------------------------------------------------
void vtkBoneWidget::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  os << indent << "Bone Widget " << this << "\n";

  os << indent << "Widget State: "<< this->WidgetState<< "\n";
  os << indent << "Bone Selected: "<< this->BoneSelected<< "\n";

  os << indent << "Handle Widgets:" << "\n";
  os << indent << "  Head Widget: "<< this->HeadWidget<< "\n";
  os << indent << "  Tail Widget: "<< this->TailWidget<< "\n";

  os << indent << "World Points:" << "\n";
  os << indent << "  Rest Mode:" << "\n";
  os << indent << "    World Head Rest: "<< this->WorldHeadRest[0]
    << "  " << this->WorldHeadRest[1]
    << "  " << this->WorldHeadRest[2]<< "\n";
  os << indent << "    World Tail Rest: "<< this->WorldTailRest[0]
    << "  " << this->WorldTailRest[1]
    << "  " << this->WorldTailRest[2]<< "\n";
  os << indent << "  Pose Mode:" << "\n";
  os << indent << "    World Head Pose: "<< this->WorldHeadPose[0]
    << "  " << this->WorldHeadPose[1]
    << "  " << this->WorldHeadPose[2]<< "\n";
  os << indent << "    World Tail Pose: "<< this->WorldTailPose[0]
    << "  " << this->WorldTailPose[1]
    << "  " << this->WorldTailPose[2]<< "\n";

  os << indent << "Local Points:" << "\n";
  os << indent << "  Rest Mode:" << "\n";
  os << indent << "    Local Rest Head: "<< this->LocalHeadRest[0]
    << "  " << this->LocalHeadRest[1]
    << "  " << this->LocalHeadRest[2]<< "\n";
  os << indent << "    Local Rest Tail: "<< this->LocalTailRest[0]
    << "  " << this->LocalTailRest[1]
    << "  " << this->LocalTailRest[2]<< "\n";
  os << indent << "  Pose Mode:" << "\n";
  os << indent << "    Local Pose Head: "<< this->LocalHeadPose[0]
    << "  " << this->LocalHeadPose[1]
    << "  " << this->LocalHeadPose[2]<< "\n";
  os << indent << "  Local Pose Tail: "<< this->LocalTailPose[0]
    << "  " << this->LocalTailPose[1]
    << "  " << this->LocalTailPose[2]<< "\n";

  os << indent << "Roll: "<< this->Roll << "\n";

  os << indent << "Rest Transforms:" << "\n";
  os << indent << "  Parent To Bone:" << "\n";
  os << indent << "    Rotation: "<< this->ParentToBoneRestRotation[0]
    << "  " << this->ParentToBoneRestRotation[1]
    << "  " << this->ParentToBoneRestRotation[2]
    << "  " << this->ParentToBoneRestRotation[3]<< "\n";
  os << indent << "    Translation: "<< this->ParentToBoneRestTranslation[0]
    << "  " << this->ParentToBoneRestTranslation[1]
    << "  " << this->ParentToBoneRestTranslation[2]<< "\n";
  os << indent << "  World To Parent:" << "\n";
  os << indent << "    Rotation: "<< this->WorldToParentRestRotation[0]
    << "  " << this->WorldToParentRestRotation[1]
    << "  " << this->WorldToParentRestRotation[2]
    << "  " << this->WorldToParentRestRotation[3]<< "\n";
  os << indent << "    Translation: "<< this->WorldToParentRestTranslation[0]
    << "  " << this->WorldToParentRestTranslation[1]
    << "  " << this->WorldToParentRestTranslation[2]<< "\n";
  os << indent << "  World To Bone:" << "\n";
  os << indent << "    Rotation: "<< this->WorldToBoneRestRotation[0]
    << "  " << this->WorldToBoneRestRotation[1]
    << "  " << this->WorldToBoneRestRotation[2]
    << "  " << this->WorldToBoneRestRotation[3]<< "\n";
  os << indent << "    Head Translation: "
    << this->WorldToBoneHeadRestTranslation[0]
    << "  " << this->WorldToBoneHeadRestTranslation[1]
    << "  " << this->WorldToBoneHeadRestTranslation[2]<< "\n";
  os << indent << "    Tail Translation: "
    << this->WorldToBoneTailRestTranslation[0]
    << "  " << this->WorldToBoneTailRestTranslation[1]
    << "  " << this->WorldToBoneTailRestTranslation[2]<< "\n";

  os << indent << "Pose Transforms:" << "\n";
  os << indent << "  Parent To Bone:" << "\n";
  os << indent << "    Rotation: "<< this->ParentToBonePoseRotation[0]
    << "  " << this->ParentToBonePoseRotation[1]
    << "  " << this->ParentToBonePoseRotation[2]
    << "  " << this->ParentToBonePoseRotation[3]<< "\n";
  os << indent << "    Translation: "<< this->ParentToBonePoseTranslation[0]
    << "  " << this->ParentToBonePoseTranslation[1]
    << "  " << this->ParentToBonePoseTranslation[2]<< "\n";
  os << indent << "  World To Parent:" << "\n";
  os << indent << "    Rotation: "<< this->WorldToParentPoseRotation[0]
    << "  " << this->WorldToParentPoseRotation[1]
    << "  " << this->WorldToParentPoseRotation[2]
    << "  " << this->WorldToParentPoseRotation[3]<< "\n";
  os << indent << "    Translation: "<< this->WorldToParentPoseTranslation[0]
    << "  " << this->WorldToParentPoseTranslation[1]
    << "  " << this->WorldToParentPoseTranslation[2]<< "\n";
  os << indent << "  World To Bone:" << "\n";
  os << indent << "    Rotation: "<< this->WorldToBonePoseRotation[0]
    << "  " << this->WorldToBonePoseRotation[1]
    << "  " << this->WorldToBonePoseRotation[2]
    << "  " << this->WorldToBonePoseRotation[3]<< "\n";
  os << indent << "    Head Translation: "
    << this->WorldToBoneHeadPoseTranslation[0]
    << "  " << this->WorldToBoneHeadPoseTranslation[1]
    << "  " << this->WorldToBoneHeadPoseTranslation[2]<< "\n";
  os << indent << "    Tail Translation: "
    << this->WorldToBoneTailPoseTranslation[0]
    << "  " << this->WorldToBoneTailPoseTranslation[1]
    << "  " << this->WorldToBoneTailPoseTranslation[2]<< "\n";

  os << indent << "Pose Interactions Variables:" << "\n";
  os << indent << "  Start Pose Rotation:"<< this->StartPoseRotation[0]
    << "  " << this->StartPoseRotation[1]
    << "  " << this->StartPoseRotation[2]
    << "  " << this->StartPoseRotation[3]<< "\n";
  os << indent << "  Interaction World Head Pose:"
    << this->InteractionWorldHeadPose[0]
    << "  " << this->InteractionWorldHeadPose[1]
    << "  " << this->InteractionWorldHeadPose[2]<< "\n";
  os << indent << "  Interaction World Tail Pose:"
    << this->InteractionWorldTailPose[0]
    << "  " << this->InteractionWorldTailPose[1]
    << "  " << this->InteractionWorldTailPose[2]<< "\n";

  os << indent << "Axes:" << "\n";
  os << indent << "  Axes Actor: "<< this->AxesActor << "\n";
  os << indent << "  Axes Visibility: "<< this->AxesVisibility << "\n";
  os << indent << "  Axes Size: "<< this->AxesSize << "\n";

  os << indent << "Parent link: "<< "\n";
  os << indent << "  Show Parenthood: "<< this->ShowParenthood << "\n";
  os << indent << "  Parenthood Link: "<< this->ParenthoodLink << "\n";
}
