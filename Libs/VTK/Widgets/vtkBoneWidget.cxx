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

#include "vtkBoneWidget.h"

//My includes
#include "vtkBoneRepresentation.h"

//VTK Includes
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

//Static declaration of the world coordinates
static const double X[3] = {1.0, 0.0, 0.0};
static const double Y[3] = {0.0, 1.0, 0.0};
static const double Z[3] = {0.0, 0.0, 1.0};

namespace
{

void MultiplyQuaternion(double* quad1, double* quad2, double resultQuad[4])
{
  //Quaternion are (w, x, y, z)
  //The multiplication is given by :
  //(Q1 * Q2).w = (w1w2 - x1x2 - y1y2 - z1z2)
  //(Q1 * Q2).x = (w1x2 + x1w2 + y1z2 - z1y2)
  //(Q1 * Q2).y = (w1y2 - x1z2 + y1w2 + z1x2)
  //(Q1 * Q2).z = (w1z2 + x1y2 - y1x2 + z1w2)

  resultQuad[0] = quad1[0]*quad2[0] - quad1[1]*quad2[1]
                  - quad1[2]*quad2[2] - quad1[3]*quad2[3];

  resultQuad[1] = quad1[0]*quad2[1] + quad1[1]*quad2[0]
                  + quad1[2]*quad2[3] - quad1[3]*quad2[2];

  resultQuad[2] = quad1[0]*quad2[2] + quad1[2]*quad2[0]
                  + quad1[3]*quad2[1] - quad1[1]*quad2[3];

  resultQuad[3] = quad1[0]*quad2[3] + quad1[3]*quad2[0]
                  + quad1[1]*quad2[2] - quad1[2]*quad2[1];
}

void NormalizeQuaternion(double* quad)
{
  double mag2 = quad[0]*quad[0] + quad[1]*quad[1] + quad[2]*quad[2] + quad[3]*quad[3];
  double mag = sqrt(mag2);
  quad[0] /= mag;
  quad[1] /= mag;
  quad[2] /= mag;
  quad[3] /= mag;
}

void InitializeQuaternion(double* quad)
{
  quad[0] = 1.0;
  quad[1] = 0.0;
  quad[2] = 0.0;
  quad[3] = 0.0;
}

void CopyQuaternion(double* quad, double* copyQuad)
{
  copyQuad[0] = quad[0];
  copyQuad[1] = quad[1];
  copyQuad[2] = quad[2];
  copyQuad[3] = quad[3];
}

void InitializeVector3(double* vec)
{
  vec[0] = 0.0;
  vec[1] = 0.0;
  vec[2] = 0.0;
}

void CopyVector3(double* vec, double* copyVec)
{
  copyVec[0] = vec[0];
  copyVec[1] = vec[1];
  copyVec[2] = vec[2];
}

}// end namespace


class vtkBoneWidgetCallback : public vtkCommand
{
public:
  static vtkBoneWidgetCallback *New()
    { return new vtkBoneWidgetCallback; }
  vtkBoneWidgetCallback()
    { this->BoneWidget = 0; }
  virtual void Execute(vtkObject* caller, unsigned long eventId, void*)
    {
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
        case vtkBoneWidget::RestChangedEvent:
          {
          if (this->BoneWidget->BoneParent ==
                vtkBoneWidget::SafeDownCast(caller))
            {
            this->BoneWidget->BoneParentRestChanged();
            }
          break;
          }
        case vtkBoneWidget::PoseChangedEvent:
          {
          if (this->BoneWidget->BoneParent ==
                vtkBoneWidget::SafeDownCast(caller))
            {
            this->BoneWidget->BoneParentPoseChanged();
            }
          break;
          }
        case vtkBoneWidget::PoseInteractionStoppedEvent:
          {
          if (this->BoneWidget->BoneParent ==
                vtkBoneWidget::SafeDownCast(caller))
            {
            this->BoneWidget->BoneParentInteractionStopped();
            }
          break;
          }
        }
    }
  vtkBoneWidget *BoneWidget;
};

//----------------------------------------------------------------------
vtkBoneWidget::vtkBoneWidget()
{
  this->ManagesCursor = 1;

  //Widget interaction init
  this->WidgetState = vtkBoneWidget::Start;
  this->BoneSelected = 0;
  this->HeadSelected = 0;
  this->TailSelected = 0;

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

  // Set up the callbacks on the two handles
  this->BoneWidgetCallback1 = vtkBoneWidgetCallback::New();
  this->BoneWidgetCallback1->BoneWidget = this;
  this->HeadWidget->AddObserver(vtkCommand::StartInteractionEvent, this->BoneWidgetCallback1,
                                  this->Priority);
  this->HeadWidget->AddObserver(vtkCommand::EndInteractionEvent, this->BoneWidgetCallback1,
                                  this->Priority);

  this->BoneWidgetCallback2 = vtkBoneWidgetCallback::New();
  this->BoneWidgetCallback2->BoneWidget = this;
  this->TailWidget->AddObserver(vtkCommand::StartInteractionEvent, this->BoneWidgetCallback2,
                                  this->Priority);
  this->TailWidget->AddObserver(vtkCommand::EndInteractionEvent, this->BoneWidgetCallback2,
                                  this->Priority);

//Setup the callback for the parent/child processing
  this->BoneParent = NULL;

  this->BoneWidgetChildrenCallback = vtkBoneWidgetCallback::New();
  this->BoneWidgetChildrenCallback->BoneWidget = this;

  // These are the event callbacks supported by this widget
  this->CallbackMapper->SetCallbackMethod(vtkCommand::LeftButtonPressEvent,
                                          vtkWidgetEvent::AddPoint,
                                          this, vtkBoneWidget::AddPointAction);
  this->CallbackMapper->SetCallbackMethod(vtkCommand::MouseMoveEvent,
                                          vtkWidgetEvent::Move,
                                          this, vtkBoneWidget::MoveAction);
  this->CallbackMapper->SetCallbackMethod(vtkCommand::LeftButtonReleaseEvent,
                                          vtkWidgetEvent::EndSelect,
                                          this, vtkBoneWidget::EndSelectAction);

  //Init bone variables
  InitializeVector3(this->LocalRestHead);
  InitializeVector3(this->LocalRestTail);
  InitializeVector3(this->LocalPoseHead);
  InitializeVector3(this->LocalPoseTail);
  InitializeVector3(this->InteractionWorldHead);
  InitializeVector3(this->InteractionWorldTail);
  this->Roll = 0.0;
  InitializeQuaternion(this->RestTransform);
  InitializeQuaternion(this->PoseTransform);

  //parentage link init
  this->HeadLinkedToParent = 0;
  this->ShowParentage = 0;
  this->ParentageLink = vtkLineWidget2::New();

  //Debug axes init
  this->AxesVisibility = vtkBoneWidget::Nothing;
  this->AxesActor = vtkAxesActor::New();
  this->AxesActor->SetAxisLabels(0);
  this->AxesSize = 0.2;

  this->UpdateAxesVisibility();
}

//----------------------------------------------------------------------
vtkBoneWidget::~vtkBoneWidget()
{
  if(this->CurrentRenderer)
    {
    this->CurrentRenderer->RemoveActor(this->AxesActor);
    }
  this->AxesActor->Delete();

  this->ParentageLink->Delete();

  this->HeadWidget->RemoveObserver(this->BoneWidgetCallback1);
  this->HeadWidget->Delete();
  this->BoneWidgetCallback1->Delete();

  this->TailWidget->RemoveObserver(this->BoneWidgetCallback2);
  this->TailWidget->Delete();
  this->BoneWidgetCallback2->Delete();

  this->BoneWidgetChildrenCallback->Delete();
}

//----------------------------------------------------------------------
void vtkBoneWidget::CreateDefaultRepresentation()
{
  //Init the bone
  if ( ! this->WidgetRep )
    {
    this->WidgetRep = vtkBoneRepresentation::New();
    }

  vtkBoneRepresentation::SafeDownCast(this->WidgetRep)->
    InstantiateHandleRepresentation();

  //
  //The parent line
  this->ParentageLink->SetInteractor(this->Interactor);
  this->ParentageLink->SetCurrentRenderer(this->CurrentRenderer);
  this->ParentageLink->CreateDefaultRepresentation();

  // Dotted line
  this->ParentageLink->GetLineRepresentation()
    ->GetLineProperty()->SetLineStipplePattern(0x000f);
  this->ParentageLink->SetProcessEvents(0);
  this->UpdateParentageLinkVisibility();
}

//----------------------------------------------------------------------
void vtkBoneWidget::GetHeadRestWorldPosition(double head[3])
{
  if (this->WidgetState == vtkBoneWidget::Rest)
    {
    CopyVector3(this->GetBoneRepresentation()->GetHeadWorldPosition(), head);
    }
  else
    {
    vtkSmartPointer<vtkTransform> restTransform =
      this->CreateWorldToBoneParentRestTransform();
    double* restHead = restTransform->TransformDoublePoint(this->LocalRestHead);
    CopyVector3(restHead, head);
    }
}

//----------------------------------------------------------------------
double* vtkBoneWidget::GetHeadRestWorldPosition()
{
  if (this->WidgetState == vtkBoneWidget::Rest)
    {
    return this->GetBoneRepresentation()->GetHeadWorldPosition();
    }
  else
    {
    vtkSmartPointer<vtkTransform> restTransform =
      this->CreateWorldToBoneParentRestTransform();
    return restTransform->TransformDoublePoint(this->LocalRestHead);
    }
}

//----------------------------------------------------------------------
void vtkBoneWidget::GetHeadPoseWorldPosition(double head[3])
{
  if (this->WidgetState == vtkBoneWidget::Pose)
    {
    CopyVector3(this->GetBoneRepresentation()->GetHeadWorldPosition(), head);
    }
  else
    {
    vtkSmartPointer<vtkTransform> poseTransform =
      this->CreateWorldToBoneParentPoseTransform();
    double* poseHead = poseTransform->TransformDoublePoint(this->LocalPoseHead);
    CopyVector3(poseHead, head);
    }
}

//----------------------------------------------------------------------
double* vtkBoneWidget::GetHeadPoseWorldPosition()
{
  if (this->WidgetState == vtkBoneWidget::Pose)
    {
    return this->GetBoneRepresentation()->GetHeadWorldPosition();
    }
  else
    {
    vtkSmartPointer<vtkTransform> poseTransform =
      this->CreateWorldToBoneParentPoseTransform();
    return poseTransform->TransformDoublePoint(this->LocalPoseHead);
    }
}

//----------------------------------------------------------------------
void vtkBoneWidget::SetHeadRestWorldPosition(double x, double y, double z)
{
   double head[3];
   head[0] = x;
   head[1] = y;
   head[2] = z;
   this->SetHeadRestWorldPosition(head);
}

//----------------------------------------------------------------------
void vtkBoneWidget::SetHeadRestWorldPosition(double head[3])
{
  double diff[3];
  vtkMath::Subtract(head,
                    this->GetBoneRepresentation()->GetHeadWorldPosition(),
                    diff);
  if (vtkMath::Norm(diff) < 1e-13)
    {
    return;
    }

  if (this->WidgetState == vtkBoneWidget::Pose)
    {
    //This should not be happening. Just in case, we are going to try our best
    // to make this work. No promises however.

    //I- Need to figure the pose transform between the old position
    // and the new one
    double oldHead[3], tail[3];
    this->GetBoneRepresentation()->GetHeadWorldPosition( oldHead );
    this->GetBoneRepresentation()->GetTailWorldPosition( tail );

    // 2- Get the previous line directionnal vector and the new line vector
    double previousLineVect[3], newLineVect[3];
    vtkMath::Subtract(tail, oldHead, previousLineVect);
    vtkMath::Normalize(previousLineVect);
    vtkMath::Subtract(tail, head, newLineVect);
    vtkMath::Normalize(newLineVect);

    //This is our best bet, since we cannot be sure that the new point
    //is in the camera plan
    // 4- Compute Rotation Axis
    double rotationAxis[3];
    vtkMath::Cross(previousLineVect, newLineVect, rotationAxis);
    vtkMath::Normalize(rotationAxis);

    // 5- Compute Angle
    double angle = acos(vtkMath::Dot(newLineVect, previousLineVect));

    // 6- Update Pose transform and start pose transform
    double quad[4];
    AxisAngleToQuaternion(rotationAxis, -1.0*angle, quad); //INVERSE ROTATION !
    NormalizeQuaternion(quad);
    MultiplyQuaternion(quad, this->StartPoseTransform, this->PoseTransform);
    NormalizeQuaternion(this->PoseTransform);

    CopyQuaternion(this->PoseTransform, this->StartPoseTransform);

    //Set the point
    this->GetBoneRepresentation()->SetHeadWorldPosition(head);

    //Rebuilds local pose points
    this->RebuildLocalPosePoints();

    this->InvokeEvent(vtkBoneWidget::PoseChangedEvent, NULL);
    }
  else //other states
    {
    this->GetBoneRepresentation()->SetHeadWorldPosition(head);

    if (this->WidgetState == vtkBoneWidget::Rest)
      {
      this->RebuildRestTransform();
      this->RebuildLocalRestPoints();

      this->InvokeEvent(vtkBoneWidget::RestChangedEvent, NULL);
      }
    }

  this->RebuildAxes();
  this->RebuildParentageLink();
  this->Modified();
}

//----------------------------------------------------------------------
void vtkBoneWidget::GetTailRestWorldPosition(double tail[3])
{
  if (this->WidgetState == vtkBoneWidget::Rest)
    {
    CopyVector3(this->GetBoneRepresentation()->GetTailWorldPosition(), tail);
    }
  else
    {
    vtkSmartPointer<vtkTransform> restTransform =
      this->CreateWorldToBoneParentRestTransform();
    double* restTail = restTransform->TransformDoublePoint(this->LocalRestTail);
    CopyVector3(restTail, tail);
    }
}

//----------------------------------------------------------------------
double* vtkBoneWidget::GetTailRestWorldPosition()
{
  if (this->WidgetState == vtkBoneWidget::Rest)
    {
    return this->GetBoneRepresentation()->GetTailWorldPosition();
    }
  else
    {
    vtkSmartPointer<vtkTransform> restTransform =
      this->CreateWorldToBoneParentRestTransform();
    return restTransform->TransformDoublePoint(this->LocalRestTail);
    }
}

//----------------------------------------------------------------------
void vtkBoneWidget::GetTailPoseWorldPosition(double tail[3])
{
  if (this->WidgetState == vtkBoneWidget::Pose)
    {
    CopyVector3(this->GetBoneRepresentation()->GetTailWorldPosition(), tail);
    }
  else
    {
    vtkSmartPointer<vtkTransform> poseTransform =
      this->CreateWorldToBoneParentPoseTransform();
    double* poseTail = poseTransform->TransformDoublePoint(this->LocalPoseTail);
    CopyVector3(poseTail, tail);
    }
}

//----------------------------------------------------------------------
double* vtkBoneWidget::GetTailPoseWorldPosition()
{
  if (this->WidgetState == vtkBoneWidget::Pose)
    {
    return this->GetBoneRepresentation()->GetTailWorldPosition();
    }
  else
    {
    vtkSmartPointer<vtkTransform> poseTransform =
      this->CreateWorldToBoneParentPoseTransform();
    return poseTransform->TransformDoublePoint(this->LocalPoseTail);
    }
}

//----------------------------------------------------------------------
void vtkBoneWidget::SetTailRestWorldPosition(double x, double y, double z)
{
   double tail[3];
   tail[0] = x;
   tail[1] = y;
   tail[2] = z;
   this->SetTailRestWorldPosition(tail);
}


//----------------------------------------------------------------------
void vtkBoneWidget::RotateTailX(double angle)
{
  this->RotateTailWXYZ(angle, 1.0, 0.0, 0.0);
}

//----------------------------------------------------------------------
void vtkBoneWidget::RotateTailY(double angle)
{
  this->RotateTailWXYZ(angle, 0.0, 1.0, 0.0);
}

//----------------------------------------------------------------------
void vtkBoneWidget::RotateTailZ(double angle)
{
  this->RotateTailWXYZ(angle, 0.0, 0.0, 1.0);
}

//----------------------------------------------------------------------
void vtkBoneWidget::RotateTailWXYZ(double angle, double x, double y, double z)
{
  double axis[3];
  axis[0] = x;
  axis[1] = y;
  axis[2] = z;
  this->RotateTailWXYZ(angle, axis);
}

//----------------------------------------------------------------------
void vtkBoneWidget::RotateTailWXYZ(double angle, double axis[3])
{
  double head[3];
  this->GetBoneRepresentation()->GetHeadWorldPosition(head);

  //Make transform
  vtkSmartPointer<vtkTransform> T = vtkSmartPointer<vtkTransform>::New();
  T->Translate(head); //last transform: Translate back to Head origin
  T->RotateWXYZ(vtkMath::DegreesFromRadians(angle), axis); //middle rotatation
  double minusHead[3];
  CopyVector3(head, minusHead);
  vtkMath::MultiplyScalar(minusHead, -1.0);
  T->Translate(minusHead);//first transform: translate to origin

  //Compute and place new tail position
  double* newTail = T->TransformDoublePoint(
    this->GetBoneRepresentation()->GetTailWorldPosition());

  if (this->WidgetState == vtkBoneWidget::Pose)
    {
    this->GetBoneRepresentation()->SetTailWorldPosition(newTail);

    //Update pose transform
    double rotation[4];
    this->AxisAngleToQuaternion(axis, angle, rotation);
    NormalizeQuaternion(rotation);

    MultiplyQuaternion(rotation, this->PoseTransform, this->PoseTransform);
    NormalizeQuaternion(this->PoseTransform);

    CopyQuaternion(this->PoseTransform, this->StartPoseTransform);
    this->InvokeEvent(vtkBoneWidget::PoseChangedEvent, NULL);

    this->RebuildAxes();
    this->RebuildParentageLink();
    }
  else
    {
    this->SetTailRestWorldPosition(newTail);
    }

  this->Modified();
}

//----------------------------------------------------------------------
void vtkBoneWidget::SetTailRestWorldPosition(double tail[3])
{
  double diff[3];
  vtkMath::Subtract(tail,
                    this->GetBoneRepresentation()->GetTailWorldPosition(),
                    diff);
  if (vtkMath::Norm(diff) < 1e-13)
    {
    return;
    }

  if (this->WidgetState == vtkBoneWidget::Pose)
    {
    //This should not be happening. Just in case, we are going to try our best
    // to make this work. No promises however.

    //I- Need to figure the pose transform between the old position
    // and the new one
    double head[3], oldTail[3];
    this->GetBoneRepresentation()->GetHeadWorldPosition( head );
    this->GetBoneRepresentation()->GetTailWorldPosition( oldTail );

    // 2- Get the previous line directionnal vector and the new line vector
    double previousLineVect[3], newLineVect[3];
    vtkMath::Subtract(oldTail, head, previousLineVect);
    vtkMath::Normalize(previousLineVect);
    vtkMath::Subtract(tail, head, newLineVect);
    vtkMath::Normalize(newLineVect);

    //This is our best bet, since we cannot be sure that the new point
    //is in the camera plan
    // 4- Compute Rotation Axis
    double rotationAxis[3];
    vtkMath::Cross(previousLineVect, newLineVect, rotationAxis);
    vtkMath::Normalize(rotationAxis);

    // 5- Compute Angle
    double angle = acos(vtkMath::Dot(newLineVect, previousLineVect));

    // 6- Update Pose transform and start pose transform
    double quad[4];
    AxisAngleToQuaternion(rotationAxis, angle, quad);
    NormalizeQuaternion(quad);
    MultiplyQuaternion(quad, this->StartPoseTransform, this->PoseTransform);
    NormalizeQuaternion(this->PoseTransform);

    CopyQuaternion(this->PoseTransform, this->StartPoseTransform);

    //Set the point
    this->GetBoneRepresentation()->SetTailWorldPosition(tail);

    //Rebuilds local pose points
    this->RebuildLocalPosePoints();

    this->InvokeEvent(vtkBoneWidget::PoseChangedEvent, NULL);
    }
  else  //Other states than pose
    {
    this->GetBoneRepresentation()->SetTailWorldPosition(tail);

    if (this->WidgetState == vtkBoneWidget::Rest)
      {
      this->RebuildRestTransform();
      this->RebuildLocalRestPoints();

      this->InvokeEvent(vtkBoneWidget::RestChangedEvent, NULL);
      }
    }

  this->RebuildAxes();
  this->RebuildParentageLink();

  this->Modified();
}

//----------------------------------------------------------------------
void vtkBoneWidget::SetBoneParent(vtkBoneWidget* parent)
{
  if (this->WidgetState == vtkBoneWidget::Pose)
    {
    vtkErrorMacro("Cannot define parent bone in pose mode."
                  "\n ->Doing nothing");
    return;
    }

  if (this->BoneParent)
    {
    this->BoneParent->RemoveObserver(vtkBoneWidget::RestChangedEvent);
    this->BoneParent->RemoveObserver(vtkBoneWidget::PoseChangedEvent);
    this->BoneParent->RemoveObserver(vtkBoneWidget::PoseInteractionStoppedEvent);
    }

  this->BoneParent = parent;

  if (parent)
    {
    parent->AddObserver(vtkBoneWidget::RestChangedEvent,
                        this->BoneWidgetChildrenCallback,
                        this->Priority);
    parent->AddObserver(vtkBoneWidget::PoseChangedEvent,
                        this->BoneWidgetChildrenCallback,
                        this->Priority);
    parent->AddObserver(vtkBoneWidget::PoseInteractionStoppedEvent,
                        this->BoneWidgetChildrenCallback,
                        this->Priority);

    if (this->HeadLinkedToParent)
      {
      this->LinkHeadToParent();
      }
    this->UpdateAxesVisibility();

    this->RebuildLocalRestPoints();
    }
  else
    {
    this->GetBoneRepresentation()->GetHeadWorldPosition(this->LocalRestHead);
    this->GetBoneRepresentation()->GetTailWorldPosition(this->LocalRestTail);
    }

  this->Modified();
}

//----------------------------------------------------------------------
vtkBoneWidget* vtkBoneWidget::GetBoneParent()
{
  return this->BoneParent;
}

//----------------------------------------------------------------------
void vtkBoneWidget::GetRestTransform(double restTransform[4])
{
  CopyQuaternion(this->RestTransform, restTransform);
}

//----------------------------------------------------------------------
double* vtkBoneWidget::GetPoseTransform()
{
  return this->PoseTransform;
}

//----------------------------------------------------------------------
void vtkBoneWidget::GetPoseTransform(double poseTransform[4])
{
  CopyQuaternion(this->PoseTransform, poseTransform);
}

//----------------------------------------------------------------------
double* vtkBoneWidget::GetRestTransform()
{
  return this->RestTransform;
}

//----------------------------------------------------------------------
void vtkBoneWidget::RebuildRestTransform()
{
  //Code greatly inspired by: http://www.fastgraph.com/makegames/3drotation/

  double head[3], tail[3];
  this->GetBoneRepresentation()->GetHeadWorldPosition( head );
  this->GetBoneRepresentation()->GetTailWorldPosition( tail );

  double viewOut[3];      // the View or "new Z" vector
  double viewUp[3];       // the Up or "new Y" vector
  double viewRight[3];    // the Right or "new X" vector

  double upMagnitude;     // for normalizing the Up vector
  double upProjection;    // magnitude of projection of View Vector on World UP

  // first, calculate and normalize the view vector
  vtkMath::Subtract(tail, head, viewOut);

  // normalize. This is the unit vector in the "new Z" direction
  // invalid points (not far enough apart)
  if (vtkMath::Normalize(viewOut) < 0.000001)
    {
    vtkErrorMacro("Tail and Head are not enough apart,"
                  " could not rebuild rest Transform");
    InitializeQuaternion(this->RestTransform);
    return;
    }

  // Now the hard part: The ViewUp or "new Y" vector

  // dot product of ViewOut vector and World Up vector gives projection of
  // of ViewOut on WorldUp
  upProjection = vtkMath::Dot(viewOut, Y);

  // first try at making a View Up vector: use World Up
  viewUp[0] = Y[0] - upProjection*viewOut[0];
  viewUp[1] = Y[1] - upProjection*viewOut[1];
  viewUp[2] = Y[2] - upProjection*viewOut[2];

  // Check for validity:
  upMagnitude = vtkMath::Norm(viewUp);

  if (upMagnitude < 0.0000001)
    {
    //Second try at making a View Up vector: Use Y axis default  (0,1,0)
    viewUp[0] = -viewOut[1]*viewOut[0];
    viewUp[1] = 1-viewOut[1]*viewOut[1];
    viewUp[2] = -viewOut[1]*viewOut[2];

    // Check for validity:
    upMagnitude = vtkMath::Norm(viewUp);

    if (upMagnitude < 0.0000001)
      {
      //Final try at making a View Up vector: Use Z axis default  (0,0,1)
      viewUp[0] = -viewOut[2]*viewOut[0];
      viewUp[1] = -viewOut[2]*viewOut[1];
      viewUp[2] = 1-viewOut[2]*viewOut[2];

      // Check for validity:
      upMagnitude = vtkMath::Norm(viewUp);

      if (upMagnitude < 0.0000001)
        {
        vtkErrorMacro("Could not fin a vector perpendiculare to the bone,"
                      " check the bone values. This should not be happening.");
        return;
        }
      }
    }

  // normalize the Up Vector
  upMagnitude = vtkMath::Normalize(viewUp);

  // Calculate the Right Vector. Use cross product of Out and Up.
  vtkMath::Cross(viewUp, viewOut,  viewRight);
  vtkMath::Normalize(viewRight); //Let's be paranoid about the normalization

  //Get the rest transform matrix
  AxisAngleToQuaternion(viewRight, acos(upProjection) , this->RestTransform);
  NormalizeQuaternion(this->RestTransform);

  if (this->Roll != 0.0)
    {
    //Get the roll matrix
    double rollQuad[4];
    AxisAngleToQuaternion(viewOut, this->Roll , rollQuad);
    NormalizeQuaternion(rollQuad);

    //Get final matrix
    MultiplyQuaternion(rollQuad, this->RestTransform, this->RestTransform);
    NormalizeQuaternion(this->RestTransform);
    }
}

//----------------------------------------------------------------------
void vtkBoneWidget::RebuildLocalRestPoints()
{
  vtkSmartPointer<vtkTransform> transform =
    this->CreateWorldToBoneParentTransform();
  transform->Inverse();

  double* head = transform->TransformDoublePoint(
    this->GetBoneRepresentation()->GetHeadWorldPosition());
  CopyVector3(head, this->LocalRestHead);

  double* tail = transform->TransformDoublePoint(
    this->GetBoneRepresentation()->GetTailWorldPosition());
  CopyVector3(tail, this->LocalRestTail);
}

//----------------------------------------------------------------------
void vtkBoneWidget::RebuildLocalPosePoints()
{
  vtkSmartPointer<vtkTransform> transform =
    this->CreateWorldToBoneParentTransform();
  transform->Inverse();

  double* head = transform->TransformDoublePoint(
    this->GetBoneRepresentation()->GetHeadWorldPosition());
  CopyVector3(head, this->LocalPoseHead);

  double* tail = transform->TransformDoublePoint(
    this->GetBoneRepresentation()->GetTailWorldPosition());
  CopyVector3(tail, this->LocalPoseTail);
}

//----------------------------------------------------------------------
void vtkBoneWidget::SetEnabled(int enabling)
{
  // The handle widgets are not actually enabled until they are placed.
  // The handle widgets take their representation from the vtkBoneRepresentation.
  if ( enabling )
    {
    if ( this->WidgetState == vtkBoneWidget::Start )
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

      // The interactor must be set prior to enabling the widget.
      if (this->Interactor)
        {
        this->HeadWidget->SetInteractor(this->Interactor);
        this->TailWidget->SetInteractor(this->Interactor);
        }
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

    if (this->ParentageLink)
      {
      this->ParentageLink->SetInteractor(this->Interactor);
      this->ParentageLink->SetCurrentRenderer(this->CurrentRenderer);

      this->UpdateParentageLinkVisibility();
      }
    }

  this->HeadWidget->SetEnabled(enabling);
  this->TailWidget->SetEnabled(enabling);
  this->ParentageLink->SetEnabled(enabling);

  this->Superclass::SetEnabled(enabling);

  // Add/Remove the actor
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
      this->SetAxesVisibility(vtkBoneWidget::Nothing);
      }
    this->UpdateAxesVisibility();
    }
}

//----------------------------------------------------------------------
void vtkBoneWidget::SetRepresentation(vtkBoneRepresentation* r)
{
  if (this->WidgetState == vtkBoneWidget::Pose
    || this->WidgetState == vtkBoneWidget::Rest)
    {
    r->SetHeadWorldPosition(
      this->GetBoneRepresentation()->GetHeadWorldPosition());
    r->SetTailWorldPosition(
      this->GetBoneRepresentation()->GetTailWorldPosition());
    }
  else if (this->WidgetState == vtkBoneWidget::Define)
    {
    r->SetHeadWorldPosition(
      this->GetBoneRepresentation()->GetHeadWorldPosition());
    }

  this->Superclass::SetWidgetRepresentation(
    reinterpret_cast<vtkWidgetRepresentation*>(r));
}

//----------------------------------------------------------------------
vtkBoneRepresentation* vtkBoneWidget::GetBoneRepresentation()
{
  return vtkBoneRepresentation::SafeDownCast(this->WidgetRep);
}

//-------------------------------------------------------------------------
void vtkBoneWidget::AddPointAction(vtkAbstractWidget *w)
{
  vtkBoneWidget *self = vtkBoneWidget::SafeDownCast(w);

  int X = self->Interactor->GetEventPosition()[0];
  int Y = self->Interactor->GetEventPosition()[1];
  double e[2];
  e[0] = static_cast<double>(X);
  e[1] = static_cast<double>(Y);

  // If we are placing the first point it's easy
  if ( self->WidgetState == vtkBoneWidget::Start )
    {
    self->GrabFocus(self->EventCallbackCommand);
    self->WidgetState = vtkBoneWidget::Define;
    self->InvokeEvent(vtkCommand::StartInteractionEvent,NULL);

    if (self->HeadLinkedToParent && self->BoneParent)
      {
      vtkBoneRepresentation::SafeDownCast(self->WidgetRep)->SetHeadWorldPosition(
        self->BoneParent->GetBoneRepresentation()->GetHeadWorldPosition());
      }
    else
      {
      //Place Point yourself
      vtkBoneRepresentation::SafeDownCast(self->WidgetRep)->SetHeadDisplayPosition(e);
      self->HeadWidget->SetEnabled(1);
      }

    self->Modified();
    }

  // If defining we are placing the second or third point
  else if ( self->WidgetState == vtkBoneWidget::Define )
    {
    //Place Point
    self->WidgetState = vtkBoneWidget::Rest;

    vtkBoneRepresentation::SafeDownCast(self->WidgetRep)->SetTailDisplayPosition(e);
    self->TailWidget->SetEnabled(1);
    self->TailWidget->GetRepresentation()->SetVisibility(1);
    self->WidgetRep->SetVisibility(1);

    self->RebuildRestTransform();
    self->RebuildLocalRestPoints();
    self->RebuildAxes();
    self->RebuildParentageLink();

    self->Modified();
    }

  else if ( self->WidgetState == vtkBoneWidget::Rest 
            || self->WidgetState == vtkBoneWidget::Pose )
    {
    self->BoneSelected = 0;
    self->HeadSelected = 0;
    self->TailSelected = 0;

    int modifier = self->Interactor->GetShiftKey() | self->Interactor->GetControlKey();
    int state = self->WidgetRep->ComputeInteractionState(X,Y,modifier);
    if ( state == vtkBoneRepresentation::Outside )
      {
      return;
      }

    self->GrabFocus(self->EventCallbackCommand);
    if ( state == vtkBoneRepresentation::OnP1 )
      {
      self->GetBoneRepresentation()->GetHeadRepresentation()->Highlight(1);
      self->HeadSelected = 1;
      self->InvokeEvent(vtkCommand::LeftButtonPressEvent,NULL);
      }
    else if (state == vtkBoneRepresentation::OnP2 )
      {
      self->GetBoneRepresentation()->GetTailRepresentation()->Highlight(1);
      self->TailSelected = 1;
      self->InvokeEvent(vtkCommand::LeftButtonPressEvent,NULL);
      }
    else if ( state == vtkBoneRepresentation::OnLine)
      {
      if (self->WidgetState == vtkBoneWidget::Rest
          || !self->BoneParent)
        {
        self->GetBoneRepresentation()->GetLineHandleRepresentation()->Highlight(1);
        self->BoneSelected = 1;
        self->WidgetRep->StartWidgetInteraction(e);
        self->InvokeEvent(vtkCommand::LeftButtonPressEvent,NULL);
        }
      }
    }

  self->EventCallbackCommand->SetAbortFlag(1);
  self->Render();
}

//-------------------------------------------------------------------------
void vtkBoneWidget::MoveAction(vtkAbstractWidget *w)
{
  vtkBoneWidget *self = vtkBoneWidget::SafeDownCast(w);

  // Do nothing if outside
  if ( self->WidgetState == vtkBoneWidget::Start )
    {
    return;
    }

  // Delegate the event consistent with the state
  int X = self->Interactor->GetEventPosition()[0];
  int Y = self->Interactor->GetEventPosition()[1];
  double e[2];
  e[0] = static_cast<double>(X);
  e[1] = static_cast<double>(Y);

  if ( self->WidgetState == vtkBoneWidget::Define )
    {
    // \todo: factorize call to InteractionEvent and SetAbortFlag
    self->InvokeEvent(vtkCommand::InteractionEvent,NULL);
    self->EventCallbackCommand->SetAbortFlag(1);
    }

  else if (self->WidgetState == vtkBoneWidget::Rest)
    {
    if ( self->HeadSelected )
      {
      self->GetBoneRepresentation()->SetHeadDisplayPosition(e);
      self->RebuildRestTransform();
      self->RebuildLocalRestPoints();
      self->RebuildAxes();
      self->RebuildParentageLink();

      //self->InvokeEvent(vtkBoneWidget::RestChangedEvent, NULL);
      }

    else if ( self->TailSelected )
      {
      self->GetBoneRepresentation()->SetTailDisplayPosition(e);
      self->RebuildRestTransform();
      self->RebuildLocalRestPoints();
      self->RebuildAxes();
      self->RebuildParentageLink();

      self->InvokeEvent(vtkBoneWidget::RestChangedEvent, NULL);
      }

    else if ( self->BoneSelected )
      {
      self->GetBoneRepresentation()->GetLineHandleRepresentation()->SetDisplayPosition(e);
      vtkBoneRepresentation::SafeDownCast(self->WidgetRep)->WidgetInteraction(e);
      self->RebuildRestTransform();
      self->RebuildLocalRestPoints();
      self->RebuildAxes();
      self->RebuildParentageLink();

      if (self->HeadLinkedToParent && self->BoneParent)
        {
        self->BoneParent->LinkTailToChild(self);
        }

      self->InvokeEvent(vtkBoneWidget::RestChangedEvent, NULL);
      }

    self->InvokeEvent(vtkCommand::InteractionEvent,NULL);
    self->Modified();
    }

  else if (self->WidgetState == vtkBoneWidget::Pose)
    {
    //Cannot move Head in pose mode

    if ( self->TailSelected )
      {
      //
      //Make rotation in camera view plane center on Head
      //

      // Get display positions
      double e1[2], e2[2];
      self->GetBoneRepresentation()->GetHeadDisplayPosition(e1);
      self->GetBoneRepresentation()->GetTailDisplayPosition(e2);

      // Get the current line -> the line between Head and the event
      //in display coordinates
      double currentLine[2], oldLine[2];
      currentLine[0] = e[0] - e1[0];currentLine[1] = e[1] - e1[1];
      vtkMath::Normalize2D(currentLine);

      // Get the old line -> the line between Head and the LAST event
      //in display coordinates
      int lastX = self->Interactor->GetLastEventPosition()[0];
      int lastY = self->Interactor->GetLastEventPosition()[1];
      double lastE[2];
      lastE[0] = static_cast<double>(lastX);
      lastE[1] = static_cast<double>(lastY);
      oldLine[0] = lastE[0] - e1[0]; oldLine[1] = lastE[1] - e1[1];
      vtkMath::Normalize2D(oldLine);

      // Get the angle between those two lines
      double angle = vtkMath::DegreesFromRadians(
                       acos(vtkMath::Dot2D(currentLine, oldLine)));

      // Get the world coordinate of the line before anything moves
      double head[3], tail[3];
      self->GetBoneRepresentation()->GetHeadWorldPosition(head);
      self->GetBoneRepresentation()->GetTailWorldPosition(tail);

      //Get the camera vector
      double cameraVec[3];
      if (!self->GetCurrentRenderer()
          || !self->GetCurrentRenderer()->GetActiveCamera())
        {
        vtkErrorWithObjectMacro(self, "There should be a renderer and a camera."
                                      " Make sure to set these !"
                                      "\n ->Cannot move Tail in pose mode");
        return;
        }
      self->GetCurrentRenderer()->GetActiveCamera()->GetDirectionOfProjection(cameraVec);

      //Need to figure if the rotation is clockwise or counterclowise
      double spaceCurrentLine[3], spaceOldLine[3];
      spaceCurrentLine[0] = currentLine[0];
      spaceCurrentLine[1] = currentLine[1];
      spaceCurrentLine[2] = 0.0;

      spaceOldLine[0] = oldLine[0];
      spaceOldLine[1] = oldLine[1];
      spaceOldLine[2] = 0.0;

      double handenessVec[3];
      vtkMath::Cross(spaceOldLine, spaceCurrentLine, handenessVec);

      //handeness is oppostie beacuse camera is toward the focal point
      double handeness = vtkMath::Dot(handenessVec, Z) > 0 ? -1.0: 1.0;
      angle *= handeness;

      //Finally rotate Tail
      vtkSmartPointer<vtkTransform> T = vtkSmartPointer<vtkTransform>::New();
      T->Translate(head); //last transform: Translate back to Head origin
      T->RotateWXYZ(angle, cameraVec); //middle transform: rotate
      //first transform: Translate to world origin
      double minusHead[3];
      CopyVector3(head, minusHead);
      vtkMath::MultiplyScalar(minusHead, -1.0);
      T->Translate(minusHead);

      self->GetBoneRepresentation()->SetTailWorldPosition(
        T->TransformDoublePoint(tail));

      self->RebuildPoseTransform();
      self->RebuildLocalPosePoints();
      self->RebuildAxes();
      self->RebuildParentageLink();

      self->InvokeEvent(vtkBoneWidget::PoseChangedEvent, NULL);
      self->InvokeEvent(vtkCommand::InteractionEvent,NULL);
      self->Modified();
      }
    else if ( self->BoneSelected )
      {
      if (!self->BoneParent) //shouldn't be necessary since the
                             //sorting is done in AddAction but just in case
        {
        // moving outer portion of line -- rotating
        self->GetBoneRepresentation()
          ->GetLineHandleRepresentation()->SetDisplayPosition(e);
        vtkBoneRepresentation::SafeDownCast(self->WidgetRep)->WidgetInteraction(e);

        self->RebuildPoseTransform();
        self->RebuildLocalPosePoints();
        self->RebuildAxes();
        self->RebuildParentageLink();

        self->InvokeEvent(vtkBoneWidget::PoseChangedEvent, NULL);
        self->InvokeEvent(vtkCommand::InteractionEvent,NULL);
        self->Modified();
        }
      }
    }

  self->WidgetRep->BuildRepresentation();
  self->Render();
}

//-------------------------------------------------------------------------
void vtkBoneWidget::EndSelectAction(vtkAbstractWidget *w)
{
  vtkBoneWidget *self = vtkBoneWidget::SafeDownCast(w);

  // Do nothing if outside
  if ( self->WidgetState == vtkBoneWidget::Start ||
       self->WidgetState == vtkBoneWidget::Define )
    {
    return;
    }

  if ( self->WidgetState == vtkBoneWidget::Pose )
    {
    self->BoneParentInteractionStopped();
    }

  self->BoneSelected = 0;
  self->HeadSelected = 0;
  self->TailSelected = 0;
  self->WidgetRep->Highlight(0);
  self->ReleaseFocus();
  self->WidgetRep->BuildRepresentation();
  int state = self->WidgetRep->GetInteractionState();
  if ( state == vtkBoneRepresentation::OnP1 ||
       state == vtkBoneRepresentation::OnP2 )
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

//-------------------------------------------------------------------------
void vtkBoneWidget::BoneParentInteractionStopped()
{
  //If the movement is finished, store the pose transform
  CopyQuaternion(this->PoseTransform, this->StartPoseTransform);

  //And update the pose point
  this->GetBoneRepresentation()->GetHeadWorldPosition(this->InteractionWorldHead);
  this->GetBoneRepresentation()->GetTailWorldPosition(this->InteractionWorldTail);

  this->InvokeEvent(vtkBoneWidget::PoseInteractionStoppedEvent, NULL);
}

//-------------------------------------------------------------------------
void vtkBoneWidget::BoneParentPoseChanged()
{
  vtkSmartPointer<vtkTransform> transform =
    this->CreateWorldToBoneParentTransform();

  double* newHead = transform->TransformDoublePoint(this->LocalPoseHead);
  this->GetBoneRepresentation()->SetHeadWorldPosition(newHead);

  double* newTail = transform->TransformDoublePoint(this->LocalPoseTail);
  this->GetBoneRepresentation()->SetTailWorldPosition(newTail);

  this->RebuildPoseTransform();
  this->RebuildAxes();
  this->RebuildParentageLink();
  this->InvokeEvent(vtkBoneWidget::PoseChangedEvent, NULL);
  this->Modified();
}

//-------------------------------------------------------------------------
void vtkBoneWidget::RebuildPoseTransform()
{
  if (this->WidgetState != vtkBoneWidget::Pose)
    {
    return;
    }

  // Cumulative technique is simple but causes drift :(
  // That is why we need to recompute each time.
  // The old pose transform represents the sum of all the other
  // previous transformations.

  double head[3], tail[3], previousLineVect[3], newLineVect[3], rotationAxis[3], poseAngle;
  // 1- Get Head, Tail, Old Tail
  this->GetBoneRepresentation()->GetHeadWorldPosition( head );
  this->GetBoneRepresentation()->GetTailWorldPosition( tail );

  // 2- Get the previous line directionnal vector
  vtkMath::Subtract(this->InteractionWorldTail, this->InteractionWorldHead, previousLineVect);
  vtkMath::Normalize(previousLineVect);

  // 3- Get the new line vector
  vtkMath::Subtract(tail, head, newLineVect);
    vtkMath::Normalize(newLineVect);

  if (this->GetCurrentRenderer() && this->GetCurrentRenderer()->GetActiveCamera())
    {
    // 4- Compute Rotation Axis
    this->GetCurrentRenderer()->GetActiveCamera()->GetDirectionOfProjection(rotationAxis);
    vtkMath::Normalize(rotationAxis);//Let's be paranoid about normalization

    // 4- Compute Angle
    double rotationPlaneAxis1[3], rotationPlaneAxis2[3];
    vtkMath::Perpendiculars(rotationAxis, rotationPlaneAxis1, rotationPlaneAxis2, 0.0);
    vtkMath::Normalize(rotationPlaneAxis1);//Let's be paranoid about normalization
    vtkMath::Normalize(rotationPlaneAxis2);

    //The angle is the difference between the old angle and the new angle.
    //Doing this difference enables us to not care about the possible roll
    //of the camera
    double newVectAngle = atan2(vtkMath::Dot(newLineVect, rotationPlaneAxis2),
                                vtkMath::Dot(newLineVect, rotationPlaneAxis1));
    double previousVectAngle = atan2(vtkMath::Dot(previousLineVect, rotationPlaneAxis2),
                                     vtkMath::Dot(previousLineVect, rotationPlaneAxis1));
    poseAngle = newVectAngle - previousVectAngle;
    }
  else //this else is here for now but maybe we should just output an error message ?
       //beacuse I do not think this code would work properly.
    {
    // 4- Compute Rotation Axis
    vtkMath::Cross(previousLineVect, newLineVect, rotationAxis);
    vtkMath::Normalize(rotationAxis);

    // 4- Compute Angle
    poseAngle = acos(vtkMath::Dot(newLineVect, previousLineVect));
    }

  // PoseTransform is the sum of the transform applied to the bone in
  // pose mode. The previous transform are stored in StartPoseTransform
  double quad[4];
  AxisAngleToQuaternion(rotationAxis, poseAngle, quad);
  NormalizeQuaternion(quad);
  MultiplyQuaternion(quad, this->StartPoseTransform, this->PoseTransform);
  NormalizeQuaternion(this->PoseTransform);
}

//-------------------------------------------------------------------------
void vtkBoneWidget::BoneParentRestChanged()
{
  this->RebuildLocalRestPoints();

  //In the previous behavior, we had the child Head to follow the parent
  //Tail in distance

  //Now they either are stuck together or nothing
  if (this->HeadLinkedToParent)
    {
    this->LinkHeadToParent();
    }
  this->RebuildParentageLink();
  this->Modified();
}

//----------------------------------------------------------------------
void vtkBoneWidget::StartBoneInteraction()
{
  this->Superclass::StartInteraction();
  this->InvokeEvent(vtkCommand::StartInteractionEvent,NULL);
}

//----------------------------------------------------------------------
void vtkBoneWidget::EndBoneInteraction()
{
  this->Superclass::EndInteraction();
  this->InvokeEvent(vtkCommand::EndInteractionEvent,NULL);
}

//----------------------------------------------------------------------
void vtkBoneWidget::SetProcessEvents(int pe)
{
  this->Superclass::SetProcessEvents(pe);

  this->HeadWidget->SetProcessEvents(pe);
  this->TailWidget->SetProcessEvents(pe);
}

//----------------------------------------------------------------------
void vtkBoneWidget::SetWidgetState(int state)
{
  if ( state == this->WidgetState )
    {
    return;
    }

  int previousState = this->WidgetState;
  this->WidgetState = state;

  switch (this->WidgetState)
    {
    case vtkBoneWidget::Start:
      {
      vtkErrorMacro("Cannot set bone widget to start. The start mode is"
                    " automatically defined when the bone is created,"
                    " but cannot be set by the user.");
      return;
      }
    case vtkBoneWidget::Define:
      {
      vtkErrorMacro("Cannot set bone widget to define. The define mode is"
                    " automatically after the first point has been placed,"
                    " but cannot be set by the user.");
      return;
      }
    case vtkBoneWidget::Rest:
      {
      //Update selection
      this->BoneSelected = 0;
      this->HeadSelected = 0;
      this->TailSelected = 0;

      //Initialize pose variable used for computations positions
      InitializeQuaternion(this->StartPoseTransform);
      InitializeVector3(this->InteractionWorldHead);
      InitializeVector3(this->InteractionWorldTail);

      if (previousState != vtkBoneWidget::Pose)
        {
        //Just need to Rebuild transform and local points
        this->RebuildRestTransform();
        this->RebuildLocalRestPoints();
        }
      else // previous state was pose
        {
        //We need to reset the points to their original rest position
        vtkSmartPointer<vtkTransform> transform = this->CreateWorldToBoneParentTransform();

        double* newHead = transform->TransformDoublePoint(this->LocalRestHead);
        this->GetBoneRepresentation()->SetHeadWorldPosition(newHead);

        double* newTail = transform->TransformDoublePoint(this->LocalRestTail);
        this->GetBoneRepresentation()->SetTailWorldPosition(newTail);
        }

      //Update others
      this->UpdateAxesVisibility();
      this->RebuildParentageLink();

      break;
      }
    case vtkBoneWidget::Pose:
      {
      //Update selection
      this->BoneSelected = 0;
      this->HeadSelected = 0;
      this->TailSelected = 0;

      //Update local pose
      CopyVector3(this->LocalRestHead, this->LocalPoseHead);
      CopyVector3(this->LocalRestTail, this->LocalPoseTail);
      CopyQuaternion(this->PoseTransform, this->StartPoseTransform);

      //Update/Rebuild variables
      if (previousState != vtkBoneWidget::Rest)
        //this would be a weird case but one never knows
        {
        this->RebuildRestTransform();
        }
      else //previous state was rest
        {
        //Need to rebuild the pose mode as a function of the the rest mode.
        double distance = this->GetBoneRepresentation()->GetDistance();

        //Let's create the Y directionnal vector of the bone
        double rotateWorldYQuaternion[4];
        MultiplyQuaternion(this->GetPoseTransform(),
                           this->GetRestTransform(),
                           rotateWorldYQuaternion);
        NormalizeQuaternion(rotateWorldYQuaternion);

        //Convert transform to rotation axis
        double worldYRotationAxis[3];
        double worldYRotationAngle =
          QuaternionToAxisAngle(rotateWorldYQuaternion, worldYRotationAxis);

        //Create transform
        vtkSmartPointer<vtkTransform> rotateWorldYTransform =
          vtkSmartPointer<vtkTransform>::New();
       rotateWorldYTransform->RotateWXYZ(
         vtkMath::DegreesFromRadians(worldYRotationAngle),
         worldYRotationAxis);

        //Rotate Y
        double* newY = rotateWorldYTransform->TransformDoubleVector(Y);

        if (!this->BoneParent)
          {
          //Head is local rest position
          this->GetBoneRepresentation()->SetHeadWorldPosition(this->LocalRestHead);
          }
        else //Has bone parent
          {
          //Head is given by the position of the local rest in the father
          //coordinates sytem (pose+rest !)
          vtkSmartPointer<vtkTransform> headT =
            this->CreateWorldToBoneParentPoseTransform();
          double* newHead = headT->TransformDoublePoint(this->LocalRestHead);
          this->GetBoneRepresentation()->SetHeadWorldPosition(newHead);
          }

        //The tail is given by the new Y direction (scaled to the correct
        // distance of course) and added to the new head position
        double newTail[3],newHead[3];
        this->GetBoneRepresentation()->GetHeadWorldPosition(newHead);

        vtkMath::MultiplyScalar(newY, distance);
        vtkMath::Add(newHead, newY, newTail);
        this->GetBoneRepresentation()->SetTailWorldPosition(newTail);

        //Finally update local pose points
        this->RebuildLocalPosePoints();
        }

      //Update intercation position
      this->GetBoneRepresentation()->GetHeadWorldPosition(this->InteractionWorldHead);
      this->GetBoneRepresentation()->GetTailWorldPosition(this->InteractionWorldTail);

      this->UpdateAxesVisibility();
      this->RebuildParentageLink();

      break;
      }
    default:
      {
      vtkErrorMacro("Unknown state. The only possible values are:"
                     "\n    0 <-> Start"
                     "\n    2 <-> Rest"
                     "\n    3 <-> Pose"
                     "\n -> Doing nothing.");
      return;
      }
    }

  this->Modified();
}

//----------------------------------------------------------------------
void vtkBoneWidget::SetWidgetStateToPose()
{
  this->SetWidgetState(vtkBoneWidget::Pose);
}

//----------------------------------------------------------------------
void vtkBoneWidget::SetWidgetStateToRest()
{
  this->SetWidgetState(vtkBoneWidget::Rest);
}

//----------------------------------------------------------------------
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

//----------------------------------------------------------------------
void vtkBoneWidget::UpdateParentageLinkVisibility()
{
  if (this->ShowParentage
      && !this->HeadLinkedToParent
      && this->BoneParent)
    {
    this->ParentageLink->GetLineRepresentation()->SetVisibility(1);
    }
  else
    {
    this->ParentageLink->GetLineRepresentation()->SetVisibility(0);
    }

  this->RebuildParentageLink();
}

//----------------------------------------------------------------------
void vtkBoneWidget::UpdateAxesVisibility()
{
  if (this->AxesVisibility == vtkBoneWidget::Nothing
      || this->WidgetState == vtkBoneWidget::Start
      || this->WidgetState == vtkBoneWidget::Define)
    {
    this->AxesActor->SetVisibility(0);
    }
  else
    {
    this->AxesActor->SetVisibility(1);
    }

  this->RebuildAxes();
}

//----------------------------------------------------------------------
void vtkBoneWidget::RebuildParentageLink()
{
  if (this->ParentageLink->GetLineRepresentation()->GetVisibility())
    {
    this->ParentageLink->GetLineRepresentation()->SetPoint1WorldPosition(
      this->BoneParent->GetBoneRepresentation()->GetTailWorldPosition());

    this->ParentageLink->GetLineRepresentation()->SetPoint2WorldPosition(
      this->GetBoneRepresentation()->GetHeadWorldPosition());
    }
}

//----------------------------------------------------------------------
void vtkBoneWidget::RebuildAxes()
{
  // only update axes if they are visible to prevent unecessary computation
  if (! this->AxesActor->GetVisibility())
    {
    return;
    }
  
  double distance = 
    this->GetBoneRepresentation()->GetDistance() * this->AxesSize;
  this->AxesActor->SetTotalLength(distance, distance, distance);

  double axis[3], angle;
  if (this->AxesVisibility == vtkBoneWidget::ShowRestTransform)
    {
    angle = vtkBoneWidget::QuaternionToAxisAngle(this->RestTransform, axis);
    }
  else if (this->AxesVisibility == vtkBoneWidget::ShowPoseTransform)
    {
    angle = vtkBoneWidget::QuaternionToAxisAngle(this->PoseTransform, axis);
    }
  else if (this->AxesVisibility
    == vtkBoneWidget::ShowPoseTransformAndRestTransform)
    {
    double resultTransform[4];
    MultiplyQuaternion(this->GetPoseTransform(),
                       this->GetRestTransform(),
                        resultTransform);
    NormalizeQuaternion(resultTransform);

    angle = vtkBoneWidget::QuaternionToAxisAngle(resultTransform, axis);
    }

  vtkSmartPointer<vtkTransform> transform =
    vtkSmartPointer<vtkTransform>::New();
  transform->Translate(this->GetBoneRepresentation()->GetTailWorldPosition());
  transform->RotateWXYZ(vtkMath::DegreesFromRadians(angle), axis);

  this->AxesActor->SetUserTransform(transform);
}

//----------------------------------------------------------------------
void vtkBoneWidget::SetHeadLinkedToParent(int link)
{
  if (link == this->HeadLinkedToParent)
    {
    return;
    }

  if (link)
    {
    this->HeadSelected = 0;
    this->LinkHeadToParent();
    }

  this->HeadLinkedToParent = link;

  vtkLineRepresentation* rep=
    vtkLineRepresentation::SafeDownCast(
      this->ParentageLink->GetRepresentation());
  rep->SetVisibility(!link && this->ShowParentage);
  this->ParentageLink->SetEnabled(!link);

  this->UpdateParentageLinkVisibility();

  this->Modified();
}

//----------------------------------------------------------------------
void vtkBoneWidget::LinkHeadToParent()
{
  assert(this->HeadLinkedToParent);

  //Move this Head to Follow the parent movement
  if (this->BoneParent)
    {
    this->SetHeadRestWorldPosition(
      this->BoneParent->GetBoneRepresentation()->GetTailWorldPosition());
    }
  else
    {
    vtkErrorMacro("Cannot link Head to a non-existing parent."
                  " Set the bone parent before. ->Doing nothing.");
    return;
    }
}

//----------------------------------------------------------------------
void vtkBoneWidget::LinkTailToChild(vtkBoneWidget* child)
{
  assert(child);
  // Move this point to snap to given child
  if (child->GetHeadLinkedToParent()) //never too sure
    //(I could even verify the child's parent is indeed this)
    {
    this->SetTailRestWorldPosition(
      child->GetBoneRepresentation()->GetHeadWorldPosition());
    }
}

//----------------------------------------------------------------------
void vtkBoneWidget::SetShowParentage(int parentage)
{
  if (this->ShowParentage == parentage)
    {
    return;
    }

  this->ShowParentage = parentage;
  this->UpdateParentageLinkVisibility();
  this->RebuildParentageLink();

  this->Modified();
}

//----------------------------------------------------------------------
vtkSmartPointer<vtkTransform> vtkBoneWidget::CreateWorldToBoneParentTransform()
{
  if (this->WidgetState == vtkBoneWidget::Rest)
    {
    return this->CreateWorldToBoneParentRestTransform();
    }
  else if (this->WidgetState == vtkBoneWidget::Pose)
    {
    return this->CreateWorldToBoneParentPoseTransform();
    }

  //Start or define mode
  return NULL;
}

//----------------------------------------------------------------------
vtkSmartPointer<vtkTransform> vtkBoneWidget::CreateWorldToBoneParentPoseTransform()
{
  vtkSmartPointer<vtkTransform> poseTransform
    = vtkSmartPointer<vtkTransform>::New();
  if (!this->BoneParent)
    {
    return poseTransform;
    }

  poseTransform->Translate(this->BoneParent->GetTailPoseWorldPosition());

  double resultTransform[4];
  MultiplyQuaternion(this->BoneParent->GetPoseTransform(),
                     this->BoneParent->GetRestTransform(),
                     resultTransform);
  NormalizeQuaternion(resultTransform);

  double axis[3];
  double angle = QuaternionToAxisAngle(resultTransform, axis);
  poseTransform->RotateWXYZ(vtkMath::DegreesFromRadians(angle), axis);
  return poseTransform;
}

//----------------------------------------------------------------------
vtkSmartPointer<vtkTransform> vtkBoneWidget::CreateWorldToBoneParentRestTransform()
{
  vtkSmartPointer<vtkTransform> restTransform
    = vtkSmartPointer<vtkTransform>::New();
  if (!this->BoneParent)
    {
    return restTransform;
    }

  //Translation
  restTransform->Translate(this->BoneParent->GetTailRestWorldPosition());

  //Rotation
  double axis[3];
  double angle =
    QuaternionToAxisAngle(this->BoneParent->GetRestTransform(), axis);
  restTransform->RotateWXYZ(vtkMath::DegreesFromRadians(angle), axis);

  return restTransform;
}

//----------------------------------------------------------------------
double vtkBoneWidget::QuaternionToAxisAngle(double quad[4], double axis[3])
{
  double angle = acos(quad[0]) * 2.0;
  double f = sin( angle * 0.5 );
  if (f > 1e-13)
    {
    axis[0] = quad[1] / f;
    axis[1] = quad[2] / f;
    axis[2] = quad[3] / f;
    }
  else if (angle > 1e-13 || angle < -1e-13) //means rotation of pi
    {
    axis[0] = 1.0;
    axis[1] = 0.0;
    axis[2] = 0.0;
    }
  else
    {
    axis[0] = 0.0;
    axis[1] = 0.0;
    axis[2] = 0.0;
    }

  return angle;
}

//----------------------------------------------------------------------
void vtkBoneWidget::AxisAngleToQuaternion(double axis[3], double angle, double quad[4])
{
  quad[0] = cos( angle / 2.0 );

  double vec[3];
  vec[0] = axis[0];
  vec[1] = axis[1];
  vec[2] = axis[2];
  vtkMath::Normalize(vec);

  double f = sin( angle / 2.0);
  quad[1] =  vec[0] * f;
  quad[2] =  vec[1] * f;
  quad[3] =  vec[2] * f;
}

//----------------------------------------------------------------------
void vtkBoneWidget::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  os << indent << "Bone Widget " << this << "\n";

  os << indent << "Widget State: "<< this->WidgetState<< "\n";

  os << indent << "Selected:"<< "\n";
  os << indent << "  Bone Selected: "<< this->BoneSelected<< "\n";
  os << indent << "  Head Selected: "<< this->HeadSelected<< "\n";
  os << indent << "  Tail Selected: "<< this->TailSelected<< "\n";

  if (this->BoneParent)
    {
    os << indent << "Bone Parent: "<< this->BoneParent << "\n";
    }

  os << indent << "Local Points:" << "\n";
  os << indent << "  Local Rest Head: "<< this->LocalRestHead[0]
                                << "  " << this->LocalRestHead[1]
                                << "  " << this->LocalRestHead[2]<< "\n";
  os << indent << "  Local Rest Tail: "<< this->LocalRestTail[0]
                                << "  " << this->LocalRestTail[1]
                                << "  " << this->LocalRestTail[2]<< "\n";
  os << indent << "  Local Pose Head: "<< this->LocalPoseHead[0]
                                << "  " << this->LocalPoseHead[1]
                                << "  " << this->LocalPoseHead[2]<< "\n";
  os << indent << "  Local Pose Tail: "<< this->LocalPoseTail[0]
                                << "  " << this->LocalPoseTail[1]
                                << "  " << this->LocalPoseTail[2]<< "\n";

  os << indent << "Temporary Points:" << "\n";
  os << indent << "  Interaction World Head: "<< this->InteractionWorldHead[0]
                                << "  " << this->InteractionWorldHead[1]
                                << "  " << this->InteractionWorldHead[2]<< "\n";
  os << indent << "  Interaction World Tail: "<< this->InteractionWorldTail[0]
                                << "  " << this->InteractionWorldTail[1]
                                << "  " << this->InteractionWorldTail[2]<< "\n";

  os << indent << "Tranforms:" << "\n";
  os << indent << "  Rest Transform: "<< this->RestTransform[0]
                                << "  " << this->RestTransform[1]
                                << "  " << this->RestTransform[2]
                                << "  " << this->RestTransform[3]<< "\n";
  os << indent << "  Pose Transform: "<< this->PoseTransform[0]
                                << "  " << this->PoseTransform[1]
                                << "  " << this->PoseTransform[2]
                                << "  " << this->PoseTransform[3]<< "\n";
  os << indent << "  Start PoseTransform: "<< this->StartPoseTransform[0]
                                << "  " << this->StartPoseTransform[1]
                                << "  " << this->StartPoseTransform[2]
                                << "  " << this->StartPoseTransform[3]<< "\n";

  os << indent << "Roll: "<< this->Roll << "\n";

  os << indent << "Parent link: "<< "\n";
  os << indent << "  HeadLinkToParent: "<< this->HeadLinkedToParent << "\n";
  os << indent << "  ShowParentage: "<< this->ShowParentage << "\n";

  os << indent << "Axes:" << "\n";
  os << indent << "  Axes Actor: "<< this->AxesActor << "\n";
  os << indent << "  Axes Visibility: "<< this->AxesVisibility << "\n";
  os << indent << "  Axes Size: "<< this->AxesSize << "\n";
}
