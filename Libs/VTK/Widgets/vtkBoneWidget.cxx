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

// STD includes
#include <cassert>

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

bool CopyVector3IfDifferent(const double* vec, double* copyVec)
{
  if (CompareVector3(vec, copyVec))
    {
    return false;
    }

  CopyVector3(vec, copyVec);
  return true;
}

bool CompareQuaternion(vtkQuaterniond q1, double q2[4])
{
  vtkQuaterniond quad2;
  quad2.Set(q2);

  return q1.Compare(quad2, 1e-6);
}

bool CopyQuaternionIfDifferent(const vtkQuaterniond& quat, vtkQuaterniond& copyQuat)
{
  if (quat.Compare(copyQuat, 1e-6))
    {
    return false;
    }

  copyQuat = quat;
  return true;
}


}// End namespace


//----------------------------------------------------------------------------
vtkBoneWidget::vtkBoneWidget()
{
  // Name
  this->Name = "";

  // Widget interaction init.
  this->WidgetState = vtkBoneWidget::Rest;
  this->BoneSelected = vtkBoneWidget::NotSelected;

  // The widgets for moving the end points. They observe this widget (i.e.,
  // this widget is the parent to the handles).
  this->HeadWidget = vtkHandleWidget::New();
  this->HeadWidget->SetPriority(this->Priority-0.01);
  this->HeadWidget->SetParent(this);
  this->HeadWidget->ManagesCursorOff();

  this->TailWidget = vtkHandleWidget::New();
  this->TailWidget->SetPriority(this->Priority-0.001);
  this->TailWidget->SetParent(this);
  this->TailWidget->ManagesCursorOff();

  this->LineWidget = vtkHandleWidget::New();
  this->LineWidget->SetPriority(this->Priority-0.01);
  this->LineWidget->SetParent(this);
  this->LineWidget->ManagesCursorOff();

  // These are the event callbacks supported by this widget.
  this->CallbackMapper->SetCallbackMethod(vtkCommand::LeftButtonPressEvent,
    vtkWidgetEvent::Select, this, vtkBoneWidget::StartSelectAction);
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
  CopyVector3(this->WorldHeadRest, this->LocalHeadRest);
  CopyVector3(this->WorldTailRest, this->LocalTailRest);
  // - Pose:
  CopyVector3(this->WorldHeadPose, this->LocalHeadPose);
  CopyVector3(this->WorldTailPose, this->LocalTailPose);
  // Roll Angle:
  this->Roll = 0.0;

  //
  // Transform inits
  //
  // - Rest Transforms:
  //   * Parent To Bone:
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
  this->ShowAxes = vtkBoneWidget::Hidden;
  this->AxesActor = vtkAxesActor::New();
  this->AxesActor->SetAxisLabels(0);
  this->AxesSize = 0.4;

  this->ShowParenthood = 1;
  this->ParenthoodLink = vtkLineWidget2::New();

  // Init the transforms correctly just in case.
  this->UpdateRestMode();
  this->ResetPoseToRest();

  this->SetWidgetState(vtkBoneWidget::PlaceHead);
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

  this->HeadWidget->Delete();
  this->TailWidget->Delete();
  this->LineWidget->Delete();
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetEnabled(int enabling)
{
  if (enabling == this->GetEnabled())
    {
    return;
    }

  // The handle widgets are not actually enabled until they are placed.
  // The handle widgets take their representation
  // from the vtkBoneRepresentation.
  if ( enabling )
    {
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

    this->LineWidget->SetRepresentation(
      vtkBoneRepresentation::SafeDownCast
      (this->WidgetRep)->GetLineHandleRepresentation());
    this->LineWidget->SetInteractor(this->Interactor);
    this->LineWidget->GetRepresentation()->SetRenderer(
      this->CurrentRenderer);

    // invisible handle
    this->GetBoneRepresentation()->GetLineHandleRepresentation()->SetHandleSize(0.0);

    this->ParenthoodLink->SetInteractor(this->Interactor);
    this->ParenthoodLink->SetCurrentRenderer(this->CurrentRenderer);
    }


  this->Superclass::SetEnabled(enabling);
  // Handle enabling is only controlled by the mouse interaction
  if (!enabling)
    {
    this->HeadWidget->SetEnabled(enabling);
    this->TailWidget->SetEnabled(enabling);
    this->LineWidget->SetEnabled(enabling);
    }

  this->ParenthoodLink->SetEnabled(enabling);
  this->UpdateVisibility();

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
    this->UpdateShowAxes();
    }
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetRepresentation(vtkBoneRepresentation* representation)
{
  // Internally fires ModifiedEvent
  this->Superclass::SetWidgetRepresentation(representation);
  if (representation)
    {
    this->InstantiateParenthoodLink();
    }
  this->UpdateRepresentation();
  // Refresh the view with the new representation
  this->Render();
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
    vtkBoneRepresentation* boneRepresentation = vtkBoneRepresentation::New();
    boneRepresentation->InstantiateHandleRepresentation();
    this->SetRepresentation(boneRepresentation);
    boneRepresentation->Delete();
    }
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetProcessEvents(int pe)
{
  this->Superclass::SetProcessEvents(pe);

  this->HeadWidget->SetProcessEvents(pe);
  this->TailWidget->SetProcessEvents(pe);
  this->LineWidget->SetProcessEvents(pe);
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetWidgetState(int state)
{
  this->SetWidgetStateInternal(
    std::max(vtkBoneWidget::PlaceHead,
    std::min(static_cast<vtkBoneWidget::WidgetStateType>(state),
    vtkBoneWidget::Pose)));
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetWidgetStateInternal(int state)
{
  if (state == this->WidgetState)
    {
    return;
    }

  // Could maybe check a flag to see if anything changed in rest mode.
  if (this->WidgetState == vtkBoneWidget::Rest
      && state == vtkBoneWidget::Pose)
    {
    this->UpdatePoseMode();
    }

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
  CopyVector3(currentHead, head);
}

//----------------------------------------------------------------------------
const double* vtkBoneWidget::GetCurrentWorldHead() const
{
  if (this->WidgetState == vtkBoneWidget::Pose)
    {
    return this->WorldHeadPose;
    }
  return this->WorldHeadRest;
}

//----------------------------------------------------------------------------
void vtkBoneWidget::GetCurrentWorldTail(double tail[3]) const
{
  const double* currentTail = this->GetCurrentWorldTail();
  CopyVector3(currentTail, tail);
}

//----------------------------------------------------------------------------
const double* vtkBoneWidget::GetCurrentWorldTail() const
{
  if (this->WidgetState == vtkBoneWidget::Pose)
    {
    return this->WorldTailPose;
    }
  return this->WorldTailRest;
}

//----------------------------------------------------------------------------
void vtkBoneWidget
::SetWorldToParentRestRotationAndTranslation(double quat[4],
                                             double translate[3])
{
  bool changeRotation =
    ! CompareQuaternion(this->WorldToParentRestRotation, quat);
  bool changeTranslation =
    ! CompareVector3(this->WorldToParentRestTranslation, translate);

  if (changeRotation)
    {
    this->WorldToParentRestRotation.Set(quat);
    this->WorldToParentRestRotation.Normalize();
    }
  if (changeTranslation)
    {
    CopyVector3(translate, this->WorldToParentRestTranslation);
    }

  if (changeRotation || changeTranslation)
    {
    this->UpdateRestMode();
    }
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetWorldToParentRestRotation(double quat[4])
{
  if (CompareQuaternion(this->WorldToParentRestRotation, quat))
    {
    return;
    }

  this->WorldToParentRestRotation.Set(quat);
  this->WorldToParentRestRotation.Normalize();
  this->UpdateRestMode();
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetWorldToParentRestTranslation(double translate[3])
{
  if (CompareVector3(this->WorldToParentRestTranslation, translate))
    {
    return;
    }

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
  bool changeRotation =
    ! CompareQuaternion(this->WorldToParentPoseRotation, quat);
  bool changeTranslation =
    ! CompareVector3(this->WorldToParentPoseTranslation, translate);

  if (changeRotation)
    {
    this->WorldToParentPoseRotation.Set(quat);
    this->WorldToParentPoseRotation.Normalize();
    }
  if (changeTranslation)
    {
    CopyVector3(translate, this->WorldToParentPoseTranslation);
    }

  if (changeRotation || changeTranslation)
    {
    this->UpdateWorldPosePositions();
    this->UpdatePoseMode();
    }
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetWorldToParentPoseRotation(double quat[4])
{
  if (CompareQuaternion(this->WorldToParentPoseRotation, quat))
    {
    return;
    }

  this->WorldToParentPoseRotation.Set(quat);
  this->WorldToParentPoseRotation.Normalize();
  this->UpdateWorldPosePositions();
  this->UpdatePoseMode();
}

///----------------------------------------------------------------------------
void vtkBoneWidget::SetWorldToParentPoseTranslation(double translate[3])
{
  if (CompareVector3(this->WorldToParentPoseTranslation, translate))
    {
    return;
    }

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
  bool changeHead = ! CompareVector3(this->WorldHeadRest, head);
  bool changeTail = ! CompareVector3(this->WorldTailRest, tail);

  if (changeHead)
    {
    CopyVector3(head, this->WorldHeadRest);
    }
  if (changeTail)
    {
    CopyVector3(tail, this->WorldTailRest);
    }

  if (changeHead || changeTail)
    {
    this->UpdateRestMode();
    }
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
  double displayHead[3];
  displayHead[0] = x;
  displayHead[1] = y;
  displayHead[2] = 0.0;
  this->SetDisplayHeadRestPosition(displayHead);
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetDisplayHeadRestPosition(double displayHead[3])
{
  if (CompareVector3(
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
  double displayTail[3];
  displayTail[0] = x;
  displayTail[1] = y;
  displayTail[2] = 0.0;
  this->SetDisplayTailRestPosition(displayTail);
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetDisplayTailRestPosition(double displayTail[3])
{
  if (CompareVector3(
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
  bool changeHead = ! CompareVector3(this->LocalHeadRest, head);
  bool changeTail = ! CompareVector3(this->LocalTailRest, tail);

  if (changeHead)
    {
    CopyVector3(head, this->LocalHeadRest);
    }
  if (changeTail)
    {
    CopyVector3(tail, this->LocalTailRest);
    }

  if (changeHead || changeTail)
    {
    this->UpdateRestMode();
    }
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
void vtkBoneWidget::SetWorldTailPose(double tail[3])
{
  if (CompareVector3(this->WorldTailRest, tail))
    {
    return;
    }
#ifndef _NDEBUG
  double bone[3];
  vtkMath::Subtract(this->GetWorldTailPose(), this->GetWorldHeadPose(), bone);
  double length = vtkMath::Norm(bone);
  vtkMath::Subtract(tail, this->GetWorldHeadPose(), bone);
  double newLength = vtkMath::Norm(bone);
  assert( (newLength < length + 0.0000001) && (newLength > length - 0.0000001));
#endif
  CopyVector3(tail, this->WorldTailPose);
  // \tbd how about calling UpdatePoseMode() instead ?
  this->RebuildLocalTailPose();
  this->RebuildWorldToBonePoseRotationInteraction();
  // Update translations:
  this->RebuildWorldToBonePoseTranslations();
  this->UpdateDisplay();
  this->InvokeEvent(vtkBoneWidget::PoseChangedEvent, NULL);
  this->Modified();
}

//----------------------------------------------------------------------------
double vtkBoneWidget::GetLength()
{
  double lineVect[3];
  vtkMath::Subtract(this->GetCurrentWorldTail(),
    this->GetCurrentWorldHead(), lineVect);
  return vtkMath::Norm(lineVect);
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetShowAxes(int show)
{
  if (this->ShowAxes == show)
    {
    return;
    }

  this->ShowAxes = show;
  this->UpdateShowAxes();
  this->Modified();
  this->Render();
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetAxesSize(double size)
{
  if (fabs(size - this->AxesSize) < 1e-6)
    {
    return;
    }

  this->ShowAxes = size;
  this->RebuildAxes();
  this->Modified();
  this->Render();
}

//----------------------------------------------------------------------------
vtkAxesActor* vtkBoneWidget::GetAxesActor()
{
  return this->AxesActor;
}

//----------------------------------------------------------------------------
vtkLineRepresentation* vtkBoneWidget::GetParenthoodRepresentation()
{
  return this->ParenthoodLink->GetLineRepresentation();
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
    this->UpdateRestToPoseRotation();

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
void vtkBoneWidget::DeepCopy(vtkBoneWidget* other)
{
  if (!other)
    {
    return;
    }

  bool modified = false;

  // Name
  if (strcmp(this->Name.c_str(), other->GetName().c_str()) != 0)
    {
    this->Name = other->GetName();
    modified = true;
    }

  // The different states of the widget.
  if (this->WidgetState != other->GetWidgetState())
    {
    this->WidgetState = other->GetWidgetState();
    modified = true;
    }
  if (this->BoneSelected != other->GetBoneSelected())
    {
    this->BoneSelected = other->GetBoneSelected();
    modified = true;
    }

  // World positions:
  // - Rest:
  modified
    |= CopyVector3IfDifferent(other->GetWorldHeadRest(), this->WorldHeadRest);
  modified
    |= CopyVector3IfDifferent(other->GetWorldTailRest(), this->WorldTailRest);
  // - Pose:
  modified
    |= CopyVector3IfDifferent(other->GetWorldHeadPose(), this->WorldHeadPose);
  modified
    |= CopyVector3IfDifferent(other->GetWorldTailPose(), this->WorldTailPose);
  // Local Positions:
  // - Rest:
  modified
    |= CopyVector3IfDifferent(other->GetLocalHeadRest(), this->LocalHeadRest);
  modified
    |= CopyVector3IfDifferent(other->GetLocalTailRest(), this->LocalTailRest);
  // - Pose
  modified
    |= CopyVector3IfDifferent(other->GetLocalHeadPose(), this->LocalHeadPose);
  modified
    |= CopyVector3IfDifferent(other->GetLocalTailPose(), this->LocalTailPose);

  // Roll Angle:
  if (fabs(this->Roll - other->GetRoll()) > 1e-6)
    {
    this->Roll = other->GetRoll();
    modified = true;
    }

  // - Rest Transforms:
  //   * Parent To Bone:
  modified |= CopyQuaternionIfDifferent(other->GetParentToBoneRestRotation(),
    this->ParentToBoneRestRotation);
  modified |= CopyVector3IfDifferent(other->GetParentToBoneRestTranslation(),
    this->ParentToBoneRestTranslation);
  //   * World To Parent:
  modified |= CopyQuaternionIfDifferent(other->GetWorldToParentRestRotation(),
    this->WorldToParentRestRotation);
  modified |= CopyVector3IfDifferent(other->GetWorldToParentRestTranslation(),
    this->WorldToParentRestTranslation);
  //   * World To Bone:
  modified |= CopyQuaternionIfDifferent(other->GetWorldToBoneRestRotation(),
    this->WorldToBoneRestRotation);
  modified
    |= CopyVector3IfDifferent(other->GetWorldToBoneHeadRestTranslation(),
      this->WorldToBoneHeadRestTranslation);
  modified
    |= CopyVector3IfDifferent(other->GetWorldToBoneTailRestTranslation(),
      this->WorldToBoneTailRestTranslation);

  // - Pose Transforms:
  //   * Parent To Bone:
  modified |= CopyQuaternionIfDifferent(other->GetParentToBonePoseRotation(),
    this->ParentToBonePoseRotation);
  modified |= CopyVector3IfDifferent(other->GetParentToBonePoseTranslation(),
    this->ParentToBonePoseTranslation);
  //   * World To Parent:
  modified |= CopyQuaternionIfDifferent(other->GetWorldToParentPoseRotation(),
    this->WorldToParentPoseRotation);
  modified |= CopyVector3IfDifferent(other->GetWorldToParentPoseTranslation(),
    this->WorldToParentPoseTranslation);
  //    * World To Bone:
  modified |= CopyQuaternionIfDifferent(other->GetWorldToBonePoseRotation(),
    this->WorldToBonePoseRotation);
  modified
    |= CopyVector3IfDifferent(other->GetWorldToBoneHeadPoseTranslation(),
      this->WorldToBoneHeadPoseTranslation);
  modified
    |= CopyVector3IfDifferent(other->GetWorldToBoneTailPoseTranslation(),
      this->WorldToBoneTailPoseTranslation);

  // - Pose To Rest Transform:
  modified |= CopyQuaternionIfDifferent(other->GetRestToPoseRotation(),
    this->RestToPoseRotation);

  // Axes variables:
  // For an easier debug and understanding.
  if (this->ShowAxes != other->GetShowAxes())
    {
    this->ShowAxes = other->GetShowAxes();
    modified = true;
    }

  if (fabs(this->AxesSize - other->GetAxesSize()) > 1e-6)
    {
    this->AxesSize = other->GetAxesSize();
    modified = true;
    }

  // Parentage line
  if (this->ShowParenthood != other->GetShowParenthood())
    {
    this->ShowParenthood = other->GetShowParenthood();
    modified = true;
    }

  if (modified)
    {
    this->UpdateDisplay();
    this->Modified();
    }
}

//----------------------------------------------------------------------------
void vtkBoneWidget::StartSelectAction(vtkAbstractWidget *w)
{
  vtkBoneWidget *self = vtkBoneWidget::SafeDownCast(w);

  int X = self->Interactor->GetEventPosition()[0];
  int Y = self->Interactor->GetEventPosition()[1];
  double e[3];
  e[0] = static_cast<double>(X);
  e[1] = static_cast<double>(Y);
  e[2] = 0.0;

  bool acceptEvent = false;

  switch (self->WidgetState)
    {
    // If we are placing the first point it's easy.
    case vtkBoneWidget::PlaceHead:
      {
      // Place point
      self->SetDisplayHeadRestPosition(e);
      self->SetDisplayTailRestPosition(e);
      // Activate point
      self->HeadWidget->SetEnabled(1);
      self->TailWidget->SetEnabled(1);
      // Select tail point so that a drag would move the tail
      self->SetWidgetStateInternal(vtkBoneWidget::PlaceTail);
      self->SetWidgetSelectedState(vtkBoneWidget::TailSelected);
      // Fire events
      acceptEvent = true;
      break;
      }
    // If defining we are placing the second or third point.
    case vtkBoneWidget::PlaceTail:
      {
      // Place point
      self->SetDisplayTailRestPosition(e);
      // Activate point and entire widget as all the points are added
      self->TailWidget->SetEnabled(1);
      // Select point
      self->SetWidgetSelectedState(vtkBoneWidget::TailSelected);
      // Fire events
      acceptEvent = true;
      break;
      }
    // Edit the existing widget
    default:
    case vtkBoneWidget::Rest:
    case vtkBoneWidget::Pose:
      {
      int modifier =
        self->Interactor->GetShiftKey() | self->Interactor->GetControlKey();
      // Compute on what the mouse cursor is
      int state = self->GetBoneRepresentation()->ComputeInteractionState(X,Y,modifier);
      int selectedState = self->GetSelectedStateFromInteractionState(state);
      // Select the bone part under the mouse cursor
      self->SetWidgetSelectedState(selectedState);
      // Fire events
      acceptEvent = (selectedState != vtkBoneWidget::NotSelected);
      break;
      }
    }

  if (acceptEvent)
    {
    // Widgets catch it to call StartBoneInteraction
    self->InvokeEvent(vtkCommand::LeftButtonPressEvent,NULL);
    // Save the current position for movement computation in MoveAction.
    self->GetBoneRepresentation()->StartWidgetInteraction(e);
    // Grab to receive all the comming move/release event.
    self->GrabFocus(self->EventCallbackCommand);
    // Start low refresh rate
    self->StartInteraction();
    // Notify observers
    self->InvokeEvent(vtkCommand::StartInteractionEvent,NULL);
    // Abort to make sure no other widgets take the event.
    self->EventCallbackCommand->SetAbortFlag(1);
    // Render to show new widgets (place mode) or refresh the highlight.
    self->Render();
    }
}

//----------------------------------------------------------------------------
void vtkBoneWidget::MoveAction(vtkAbstractWidget *w)
{
  vtkBoneWidget *self = vtkBoneWidget::SafeDownCast(w);

  // compute some info we need for all cases
  int X = self->Interactor->GetEventPosition()[0];
  int Y = self->Interactor->GetEventPosition()[1];
  double e[2];
  e[0] = static_cast<double>(X);
  e[1] = static_cast<double>(Y);
  bool modified = false;

  // See whether we're active
  if ( self->BoneSelected == vtkBoneWidget::NotSelected
    && self->WidgetState != vtkBoneWidget::PlaceHead)
    {
    // Highlight the handles
    int state = self->GetBoneRepresentation()->ComputeInteractionState(X,Y);
    vtkAbstractWidget* handle = self->GetHandleFromInteractionState(state);
    int enableHead = (handle == self->HeadWidget) ? 1 : 0;
    if ( self->HeadWidget->GetEnabled() != enableHead )
      {
      self->HeadWidget->SetEnabled( enableHead );
      modified = true;
      }
    int enableTail = (handle == self->TailWidget) ? 1 : 0;
    if ( self->TailWidget->GetEnabled() != enableTail )
      {
      self->TailWidget->SetEnabled( enableTail );
      modified = true;
      }
    int enableLine = (handle == self->LineWidget) ? 1 : 0;
    if ( self->LineWidget->GetEnabled() != enableLine )
      {
      modified = true;
      }
    self->LineWidget->SetEnabled( enableLine );
    }
  else
    {
    // Move the head, tail or line handles
    if (self->WidgetState != vtkBoneWidget::Pose)
      {
      // Update the position of the handles.
      self->InvokeEvent(vtkCommand::MouseMoveEvent,NULL);
      }
    // Move the bone representation positions.
    self->GetBoneRepresentation()->WidgetInteraction(e);
    switch (self->WidgetState)
      {
      case vtkBoneWidget::PlaceHead:
        // Synchronize tail position with head position
        self->SetDisplayTailRestPosition(e);
      default:
        // Copy the bone representation positions into the widget
        self->SetWorldHeadAndTailRest(
          self->GetBoneRepresentation()->GetWorldHeadPosition(),
          self->GetBoneRepresentation()->GetWorldTailPosition());
        break;
      case vtkBoneWidget::Pose:
        // Only the tail can be changed
        self->SetWorldTailPose (
          self->GetBoneRepresentation()->GetWorldTailPosition());
        break;
      }
    self->InvokeEvent(vtkCommand::InteractionEvent,NULL);
    modified = true;
    }
  if ( modified )
    {
    bool handleSelected = self->HeadWidget->GetEnabled()
      || self->TailWidget->GetEnabled()
      || self->LineWidget->GetEnabled();
    if (handleSelected)
      {
      // Abort to make sure no other widgets take the event.
      self->EventCallbackCommand->SetAbortFlag(1);
      }
    // Render to show new positions/highlights.
    self->Render();
    }
}

//----------------------------------------------------------------------------
void vtkBoneWidget::EndSelectAction(vtkAbstractWidget *w)
{
  vtkBoneWidget *self = vtkBoneWidget::SafeDownCast(w);

  // Was doing nothing if outside.
  if (self->BoneSelected == vtkBoneWidget::NotSelected)
    {
    return;
    }

  // Release GrabFocus
  self->ReleaseFocus();
  // Deselect anything that was selected
  self->SetWidgetSelectedState(vtkBoneWidget::NotSelected);
  // If the tail has been placed, move to rest mode
  if (self->WidgetState == vtkBoneWidget::PlaceTail &&
      !CompareVector3(self->WorldHeadRest, self->WorldTailRest))
    {
    self->SetWidgetStateInternal(vtkBoneWidget::Rest);
    }
  // Widgets observe it
  self->InvokeEvent(vtkCommand::LeftButtonReleaseEvent,NULL);
  // Abort to make sure no other widgets take the event.
  self->EventCallbackCommand->SetAbortFlag(1);
  // Notify end of interaction
  self->EndInteraction();
  self->InvokeEvent(vtkCommand::EndInteractionEvent,NULL);
  // refresh rendering to remove highlight
  self->Render();
}

//----------------------------------------------------------------------------
void vtkBoneWidget::UpdateVisibility()
{
  if (this->GetBoneRepresentation())
    {
    this->GetBoneRepresentation()->SetVisibility(
      this->WidgetState != vtkBoneWidget::PlaceHead ||
      this->BoneSelected != vtkBoneWidget::NotSelected);
    }
  this->UpdateShowAxes();
  this->UpdateParenthoodLinkVisibility();
}

//----------------------------------------------------------------------------
void vtkBoneWidget::RebuildAxes()
{
  double distance = this->GetLength() * this->AxesSize;
  this->AxesActor->SetTotalLength(distance, distance, distance);

  vtkSmartPointer<vtkTransform> transform =
    vtkSmartPointer<vtkTransform>::New();
  transform->Translate(this->GetCurrentWorldTail());

  if (this->ShowAxes == vtkBoneWidget::ShowRestTransform)
    {
    transform->Concatenate(this->CreateWorldToBoneRestRotation());
    }
  else if (this->ShowAxes == vtkBoneWidget::ShowPoseTransform)
    {
    transform->Concatenate(this->CreateWorldToBonePoseRotation());
    }

  this->AxesActor->SetUserTransform(transform);
}

//----------------------------------------------------------------------------
void vtkBoneWidget::UpdateShowAxes()
{
  bool show = this->ShowAxes != vtkBoneWidget::Hidden
    && this->WidgetState != vtkBoneWidget::PlaceHead
    && this->GetEnabled();
  this->AxesActor->SetVisibility(show ? 1 : 0);
  if (show)
    {
    this->RebuildAxes();
    }
}

//----------------------------------------------------------------------------
void vtkBoneWidget::UpdateParenthoodLinkVisibility()
{
  if (!this->ParenthoodLink->GetLineRepresentation())
    {
    return;
    }
  bool visible = this->ShowParenthood
    && this->Enabled
    && this->GetBoneRepresentation()
    && this->WidgetState >= vtkBoneWidget::PlaceTail;
  this->ParenthoodLink->GetLineRepresentation()->SetVisibility( visible ? 1 : 0);
  if (visible)
    {
    this->RebuildParenthoodLink();
    }
}

//----------------------------------------------------------------------------
void vtkBoneWidget::RebuildParenthoodLink()
{
  if (!this->ParenthoodLink->GetLineRepresentation())
    {
    return;
    }
  double* p1 = 0;
  double* p2 = 0;
  if (this->WidgetState == vtkBoneWidget::Pose)
    {
    p1 = this->GetWorldToParentPoseTranslation();
    p2 = this->WorldHeadPose;
    }
  else
    {
    p1 = this->GetWorldToParentRestTranslation();
    p2 = this->WorldHeadRest;
    }
  this->ParenthoodLink->GetLineRepresentation()->SetPoint1WorldPosition(p1);
  this->ParenthoodLink->GetLineRepresentation()->SetPoint2WorldPosition(p2);
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
  if (vtkMath::Normalize(viewOut) < 0.0000001)
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

  this->UpdateRestToPoseRotation();
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
void vtkBoneWidget::StartInteraction()
{
  if (this->WidgetState == vtkBoneWidget::Pose)
    {
    this->UpdatePoseIntercationsVariables();
    }
  this->Superclass::StartInteraction();
}

//----------------------------------------------------------------------------
void vtkBoneWidget::EndInteraction()
{
  if (this->WidgetState == vtkBoneWidget::Pose)
    {
    this->UpdatePoseIntercationsVariables();
    }
  this->Superclass::EndInteraction();
}

//----------------------------------------------------------------------------
void vtkBoneWidget::UpdateRestMode()
{
  if (this->WidgetState == vtkBoneWidget::PlaceHead)
    {
    this->UpdateDisplay();
    }
  else // PlaceHead, Rest and Pose mode
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
    }

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

  if (this->WidgetState == vtkBoneWidget::Rest)
    {
    this->RebuildPoseFromRest();
    }
  else
    {
    this->RebuildWorldToBonePoseRotationFromParent();
    }

  // Update translations:
  this->RebuildWorldToBonePoseTranslations();
  this->RebuildParentToBonePoseTranslation();

  // Finaly update representation and propagate
  this->UpdateDisplay();

  this->InvokeEvent(vtkBoneWidget::PoseChangedEvent, NULL);
  if (this->WidgetState != vtkBoneWidget::Rest)
    {
    this->Modified(); // In rest mode, update rest mode will call the modified
    }
}

//----------------------------------------------------------------------------
void vtkBoneWidget::UpdateRestToPoseRotation()
{
  this->RestToPoseRotation =
    this->ParentToBonePoseRotation * this->ParentToBoneRestRotation.Inverse();
    this->RestToPoseRotation.Normalize();
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
  this->StartPoseRotation.Normalize(); // Normalization paranoia
}

//----------------------------------------------------------------------------
void vtkBoneWidget::RebuildPoseFromRest()
{
  // Need to rebuild the pose mode as a function of the the rest mode.

  // Head is given by the position of the local rest in the parent.
  CopyVector3(this->LocalHeadRest, this->LocalHeadPose);

  // Compute pose tail.
  // Pose tail is rest tail transformed by the RestToPoseRotation.
  // First things first, centers rest tail.
  double centeredTail[3];
  vtkMath::Subtract(this->LocalTailRest, this->LocalHeadRest, centeredTail);

  // Apply Rest -> Pose rotation
  double axis[3];
  double angle = this->RestToPoseRotation.GetRotationAngleAndAxis(axis);
  vtkSmartPointer<vtkTransform> rotateTail =
    vtkSmartPointer<vtkTransform>::New();
  rotateTail->RotateWXYZ(vtkMath::DegreesFromRadians(angle), axis);
  double* newLocalTail = rotateTail->TransformDoubleVector(centeredTail);

  // Re-Translate
  vtkMath::Add(this->LocalHeadRest, newLocalTail, this->LocalTailPose);

  // Update world positions
  this->UpdateWorldPosePositions();

  // Update Parent to bone
  this->ParentToBonePoseRotation =
    this->RestToPoseRotation * this->ParentToBoneRestRotation;
  this->ParentToBonePoseRotation.Normalize(); // Normalization paranoia

  // Update World to bone Pose rotation.
  this->RebuildWorldToBonePoseRotationFromParent();
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

    bool stateIsPose = this->WidgetState == vtkBoneWidget::Pose;
    rep->SetPose(stateIsPose);
    rep->SetWorldToBoneRotation(
      stateIsPose ? this->CreateWorldToBonePoseRotation() :
      this->CreateWorldToBoneRestRotation());
    }
}

//----------------------------------------------------------------------------
void vtkBoneWidget::UpdateDisplay()
{
  this->UpdateRepresentation();
  this->UpdateVisibility();
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

  // Should be equivalent to this->RestToPoseRotation.ToIdentity();
  this->UpdateRestToPoseRotation();

}

//----------------------------------------------------------------------------
vtkAbstractWidget* vtkBoneWidget::GetHandleFromInteractionState(int state)
{
  vtkAbstractWidget* handle = 0;
  switch (state)
    {
    case vtkBoneRepresentation::OnHead:
      handle = this->HeadWidget;
      break;
    case vtkBoneRepresentation::OnTail:
      handle = this->TailWidget;
      break;
    case vtkBoneRepresentation::OnLine:
      handle = this->LineWidget;
      break;
    default:
    case vtkBoneRepresentation::Outside:
      handle = 0;
      break;
    }
  return handle;
}

//----------------------------------------------------------------------------
int vtkBoneWidget::GetSelectedStateFromInteractionState(int state)
{
  int selection = vtkBoneWidget::NotSelected;
  switch (state)
    {
    case vtkBoneRepresentation::OnHead:
      selection = vtkBoneWidget::HeadSelected;
      break;
    case vtkBoneRepresentation::OnTail:
      selection = vtkBoneWidget::TailSelected;
      break;
    case vtkBoneRepresentation::OnLine:
      selection = vtkBoneWidget::LineSelected;
      break;
    default:
    case vtkBoneRepresentation::Outside:
      selection = vtkBoneWidget::NotSelected;
      break;
    }
  return selection;
}

//----------------------------------------------------------------------------
void vtkBoneWidget::SetWidgetSelectedState(int selectionState)
{
  if (this->BoneSelected == selectionState)
    {
    return;
    }
  this->BoneSelected = selectionState;
  if (this->GetBoneRepresentation())
    {
    // \todo Should only highlight the selected representation
    this->GetBoneRepresentation()->Highlight(
      selectionState != vtkBoneWidget::NotSelected);
    }

  this->InvokeEvent(vtkBoneWidget::SelectedStateChangedEvent, NULL);
  this->Modified();
}

//----------------------------------------------------------------------------
vtkWidgetRepresentation* vtkBoneWidget::GetSelectedRepresentation()
{
  vtkWidgetRepresentation* rep = 0;
  if (!this->GetBoneRepresentation())
    {
    return rep;
    }
  switch (this->BoneSelected)
    {
    case vtkBoneWidget::HeadSelected:
      rep = this->GetBoneRepresentation()->GetHeadRepresentation();
      break;
    case vtkBoneWidget::TailSelected:
      rep = this->GetBoneRepresentation()->GetTailRepresentation();
      break;
    case vtkBoneWidget::LineSelected:
      rep = this->GetBoneRepresentation();
      break;
    default:
    case vtkBoneWidget::NotSelected:
      break;
    }
  return rep;
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

  os << indent << "Rest To Pose Rotation:" << this->RestToPoseRotation<< "\n";

  os << indent << "Axes:" << "\n";
  os << indent << "  Axes Actor: "<< this->AxesActor << "\n";
  os << indent << "  Show Axes: "<< this->ShowAxes << "\n";
  os << indent << "  Axes Size: "<< this->AxesSize << "\n";

  os << indent << "Parent link: "<< "\n";
  os << indent << "  Show Parenthood: "<< this->ShowParenthood << "\n";
  os << indent << "  Parenthood Link: "<< this->ParenthoodLink << "\n";
}
