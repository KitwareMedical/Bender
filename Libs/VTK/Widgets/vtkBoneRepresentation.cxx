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

#include "vtkBoneRepresentation.h"

// Bender includes
#include "vtkBoneEnvelopeRepresentation.h"

// VTK includes
#include <vtkActor.h>
#include <vtkBox.h>
#include <vtkCallbackCommand.h>
#include <vtkCamera.h>
#include <vtkCellPicker.h>
#include <vtkFollower.h>
#include <vtkInteractorObserver.h>
#include <vtkLine.h>
#include <vtkLineSource.h>
#include <vtkMath.h>
#include <vtkObjectFactory.h>
#include <vtkOpenGL.h>
#include <vtkPointHandleRepresentation3D.h>
#include <vtkPolyDataMapper.h>
#include <vtkPolyData.h>
#include <vtkProperty.h>
#include <vtkSmartPointer.h>
#include <vtkSphereSource.h>
#include <vtkTransform.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkVectorText.h>
#include <vtkWindow.h>

vtkStandardNewMacro(vtkBoneRepresentation);

//----------------------------------------------------------------------------
vtkBoneRepresentation::vtkBoneRepresentation()
{
  this->AlwaysOnTop = 0;
  this->Pose = false;
  this->Envelope = vtkBoneEnvelopeRepresentation::New();
  this->ShowEnvelope = false;
}

//----------------------------------------------------------------------------
vtkBoneRepresentation::~vtkBoneRepresentation()
{
  this->Envelope->Delete();
}

//----------------------------------------------------------------------------
void vtkBoneRepresentation::GetWorldHeadPosition(double pos[3])
{
  this->Point1Representation->GetWorldPosition(pos);
}

//----------------------------------------------------------------------------
double* vtkBoneRepresentation::GetWorldHeadPosition()
{
  return this->Point1Representation->GetWorldPosition();
}

//----------------------------------------------------------------------------
void vtkBoneRepresentation::GetDisplayHeadPosition(double pos[3])
{
  this->Point1Representation->GetDisplayPosition(pos);
}

//----------------------------------------------------------------------------
double* vtkBoneRepresentation::GetDisplayHeadPosition()
{
  return this->Point1Representation->GetDisplayPosition();
}

//----------------------------------------------------------------------------
void vtkBoneRepresentation::SetWorldHeadPosition(double x[3])
{
  this->Superclass::SetPoint1WorldPosition(x);
  this->Envelope->SetHead(x);
}

//----------------------------------------------------------------------------
void vtkBoneRepresentation::SetDisplayHeadPosition(double x[3])
{
  this->Superclass::SetPoint1DisplayPosition(x);
  double head[3];
  this->GetWorldHeadPosition(head);
  this->Envelope->SetHead(head);
}

//----------------------------------------------------------------------------
void vtkBoneRepresentation::GetWorldTailPosition(double pos[3])
{
  this->Point2Representation->GetWorldPosition(pos);
}

//----------------------------------------------------------------------------
double* vtkBoneRepresentation::GetWorldTailPosition()
{
  return this->Point2Representation->GetWorldPosition();
}

//----------------------------------------------------------------------------
void vtkBoneRepresentation::GetDisplayTailPosition(double pos[3])
{
  this->Point2Representation->GetDisplayPosition(pos);
}

//----------------------------------------------------------------------------
double* vtkBoneRepresentation::GetDisplayTailPosition()
{
  return this->Point2Representation->GetDisplayPosition();
}

//----------------------------------------------------------------------------
void vtkBoneRepresentation::SetWorldTailPosition(double x[3])
{
  this->Superclass::SetPoint2WorldPosition(x);
  this->Envelope->SetTail(x);
}

//----------------------------------------------------------------------------
void vtkBoneRepresentation::SetDisplayTailPosition(double x[3])
{
  this->Superclass::SetPoint2DisplayPosition(x);
  double tail[3];
  this->GetWorldTailPosition(tail);
  this->Envelope->SetTail(tail);
}

//----------------------------------------------------------------------------
double vtkBoneRepresentation::GetLength()
{
  return this->Superclass::GetDistance();
}


//----------------------------------------------------------------------------
vtkPointHandleRepresentation3D* vtkBoneRepresentation::GetHeadRepresentation()
{
  return this->GetPoint1Representation();
}

//----------------------------------------------------------------------------
vtkPointHandleRepresentation3D* vtkBoneRepresentation::GetTailRepresentation()
{
  return this->GetPoint2Representation();
}

//----------------------------------------------------------------------------
void vtkBoneRepresentation::Highlight(int highlight)
{
  this->HighlightLine(highlight);
  this->HighlightPoint(0, highlight);
  this->HighlightPoint(1, highlight);
  // no higlight for the envelope
}

//----------------------------------------------------------------------------
void vtkBoneRepresentation::SetAlwaysOnTop(int onTop)
{
  if (onTop == this->AlwaysOnTop)
    {
    return;
    }

  this->AlwaysOnTop = onTop;

  this->EndPointProperty->SetBackfaceCulling(onTop);
  this->SelectedEndPointProperty->SetBackfaceCulling(onTop);
  this->EndPoint2Property->SetBackfaceCulling(onTop);
  this->SelectedEndPoint2Property->SetBackfaceCulling(onTop);
  this->LineProperty->SetBackfaceCulling(onTop);
  this->SelectedLineProperty->SetBackfaceCulling(onTop);
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkBoneRepresentation::SetRenderer(vtkRenderer* ren)
{
  this->Superclass::SetRenderer(ren);
  this->Envelope->SetRenderer(ren);
}

//----------------------------------------------------------------------------
void vtkBoneRepresentation::BuildRepresentation()
{
  // Rebuild only if necessary
  if ( this->GetMTime() > this->BuildTime ||
       this->Envelope->GetMTime() > this->BuildTime ||
       this->Point1Representation->GetMTime() > this->BuildTime ||
       this->Point2Representation->GetMTime() > this->BuildTime ||
       this->LineHandleRepresentation->GetMTime() > this->BuildTime ||
       (this->Renderer && this->Renderer->GetVTKWindow() &&
        (this->Renderer->GetVTKWindow()->GetMTime() > this->BuildTime ||
        this->Renderer->GetActiveCamera()->GetMTime() > this->BuildTime)) )
    {
    this->Superclass::BuildRepresentation();
    this->Envelope->BuildRepresentation();

    this->BuildTime.Modified();
    }
}

//----------------------------------------------------------------------------
void vtkBoneRepresentation::GetActors(vtkPropCollection *pc)
{
  this->Superclass::GetActors(pc);
  this->Envelope->GetActors(pc);
}

//----------------------------------------------------------------------------
void vtkBoneRepresentation::ReleaseGraphicsResources(vtkWindow* w)
{
  this->Superclass::ReleaseGraphicsResources(w);
  this->Envelope->ReleaseGraphicsResources(w);
}

//----------------------------------------------------------------------------
void vtkBoneRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  os << indent << "Always On Top: " << this->AlwaysOnTop << "\n";
  this->Envelope->PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
void vtkBoneRepresentation::WidgetInteraction(double e[2])
{
  if (!this->Pose)
    {
    this->Superclass::WidgetInteraction(e);
    return;
    }

  //
  // Make rotation in camera view plane center on Head.
  //
  double newPos[3];
  newPos[0] = e[0];
  newPos[1] = e[1];
  newPos[2] = 0.;

  // Get display positions
  double center[3];
  this->GetDisplayHeadPosition(center);

  // Get the current line (-> the line between Head and the event)
  // in display coordinates.
  double currentLine[2], oldLine[2];
  currentLine[0] = newPos[0] - center[0];
  currentLine[1] = newPos[1] - center[1];
  vtkMath::Normalize2D(currentLine);

  // Get the old line (-> the line between Head and the LAST event)
  // in display coordinates.
  oldLine[0] = this->LastEventPosition[0] - center[0];
  oldLine[1] = this->LastEventPosition[1] - center[1];
  vtkMath::Normalize2D(oldLine);

  // Get the angle between those two lines.
  double angle = vtkMath::DegreesFromRadians(
                   acos(vtkMath::Dot2D(currentLine, oldLine)));

  //Get the camera vector.
  double cameraVec[3];
  if (!this->GetRenderer()
      || !this->GetRenderer()->GetActiveCamera())
    {
    vtkErrorMacro(
      "There should be a renderer and a camera. Make sure to set these !"
      "\n ->Cannot move Tail in pose mode");
    return;
    }
  this->GetRenderer()->GetActiveCamera()->GetDirectionOfProjection(cameraVec);

  // Need to figure if the rotation is clockwise or counterclowise.
  double spaceCurrentLine[3];
  spaceCurrentLine[0] = currentLine[0];
  spaceCurrentLine[1] = currentLine[1];
  spaceCurrentLine[2] = 0.0;

  double spaceOldLine[3];
  spaceOldLine[0] = oldLine[0];
  spaceOldLine[1] = oldLine[1];
  spaceOldLine[2] = 0.0;

  double handenessVec[3];
  vtkMath::Cross(spaceOldLine, spaceCurrentLine, handenessVec);

  // Handeness is opposite beacuse camera is toward the focal point.
  const double Z[3] = {0.0, 0.0, 1.0};
  double handeness = vtkMath::Dot(handenessVec, Z) > 0 ? -1.0: 1.0;
  angle *= handeness;

  // Finally rotate tail
  double newTailPos[3];
  this->Rotate(angle, cameraVec, this->GetWorldHeadPosition(), this->GetWorldTailPosition(), newTailPos);
  this->SetWorldTailPosition(newTailPos);

  // Store the start position
  this->LastEventPosition[0] = e[0];
  this->LastEventPosition[1] = e[1];
  this->LastEventPosition[2] = 0.0;
}

//----------------------------------------------------------------------------
void vtkBoneRepresentation::
Rotate(double angle, double axis[3], double center[3], double pos[3], double res[3])
{
  vtkSmartPointer<vtkTransform> transformTail =
    vtkSmartPointer<vtkTransform>::New();
  transformTail->Translate(center);
  transformTail->RotateWXYZ(angle, axis);
  double minusHead[3];
  minusHead[0] = center[0];
  minusHead[1] = center[1];
  minusHead[2] = center[2];
  vtkMath::MultiplyScalar(minusHead, -1.0);
  transformTail->Translate(minusHead);

  double* trans = transformTail->TransformDoublePoint(pos);
  res[0] = trans[0];
  res[1] = trans[1];
  res[2] = trans[2];
}

//----------------------------------------------------------------------------
int vtkBoneRepresentation::ComputeInteractionState(int X, int Y, int modifier)
{
  this->InteractionState =
    this->Superclass::ComputeInteractionState(X, Y, modifier);
  // Don't select head in pose mode.
  if (this->Pose && this->InteractionState== vtkBoneRepresentation::OnHead)
    {
    this->InteractionState = vtkBoneRepresentation::Outside;
    this->SetRepresentationState(this->InteractionState);
    }

  return this->InteractionState;
}

//----------------------------------------------------------------------------
void vtkBoneRepresentation
::SetWorldToBoneRotation(vtkTransform* worldToBoneRotation)
{
  this->Envelope->SetWorldToBoneRotation(worldToBoneRotation);
}

//----------------------------------------------------------------------------
int vtkBoneRepresentation::RenderTranslucentPolygonalGeometry(vtkViewport *v)
{
  int count = 0;

  if (! this->AlwaysOnTop)
    {
    count = this->RenderTranslucentPolygonalGeometryInternal(v);
    }

  return count;
}

//----------------------------------------------------------------------------
int vtkBoneRepresentation::HasOnlyTranslucentPolygonalGeometry()
{
  int count = 0;
  this->BuildRepresentation();

  count |= this->Superclass::HasTranslucentPolygonalGeometry();
  if (this->ShowEnvelope)
    {
    count &= this->Envelope->HasTranslucentPolygonalGeometry();
    }

  return count;
}

//----------------------------------------------------------------------------
int vtkBoneRepresentation::HasTranslucentPolygonalGeometry()
{
  int count = 0;
  this->BuildRepresentation();
  if (this->ShowEnvelope)
    {
    count |= this->Envelope->HasTranslucentPolygonalGeometry();
    }
  count |= this->Superclass::HasTranslucentPolygonalGeometry();

  return count;
}

//----------------------------------------------------------------------------
int vtkBoneRepresentation::RenderOpaqueGeometry(vtkViewport *v)
{
  int count = 0;
  if (! this->AlwaysOnTop)
    {
    count = this->RenderOpaqueGeometryInternal(v);
    }

  return count;
}

//----------------------------------------------------------------------------
int vtkBoneRepresentation::RenderOverlay(vtkViewport *v)
{
  int count = 0;
  if (this->AlwaysOnTop)
    {
    GLint flag;
    glGetIntegerv(GL_DEPTH_FUNC,&flag);

    if(flag != GL_ALWAYS)
      {
      glDepthFunc(GL_ALWAYS);
      }

    if (this->HasOnlyTranslucentPolygonalGeometry())
      {
      count = this->RenderTranslucentPolygonalGeometryInternal(v);
      }
    else if (! this->HasTranslucentPolygonalGeometry())
      {
      count = this->RenderOpaqueGeometryInternal(v);
      }
    else
      {
      // Render both
      count = this->RenderTranslucentPolygonalGeometryInternal(v);
      count += this->RenderOpaqueGeometryInternal(v);
      }

    if(flag != GL_ALWAYS)
      {
      glDepthFunc(flag);
      }
    }
  else
    {
    count = this->RenderOverlayInternal(v);
    }

  return count;
}

//----------------------------------------------------------------------------
void vtkBoneRepresentation::DeepCopy(vtkProp* prop)
{
  vtkBoneRepresentation *rep = vtkBoneRepresentation::SafeDownCast(prop);
  if (rep)
    {
    // vtkLineRepresentation copies, should probably go in vtk
    this->SetInteractionState(rep->GetInteractionState());
    this->SetPoint1WorldPosition(rep->GetPoint1WorldPosition());
    this->SetPoint2WorldPosition(rep->GetPoint2WorldPosition());
    this->SetRepresentationState(rep->GetRepresentationState());

    // Representation
    this->DeepCopyRepresentationOnly(rep);
    }

  this->Superclass::ShallowCopy(prop);
}

//----------------------------------------------------------------------------
void vtkBoneRepresentation
::DeepCopyRepresentationOnly(vtkBoneRepresentation* boneRep)
{
  if (!boneRep)
    {
    return;
    }

  // vtkLineRepresentation copies, should probably go in vtk
  this->SetDistanceAnnotationFormat(boneRep->GetDistanceAnnotationFormat());
  this->SetDistanceAnnotationScale(boneRep->GetDistanceAnnotationScale());
  this->SetDistanceAnnotationVisibility(
    boneRep->GetDistanceAnnotationVisibility());
  this->SetResolution(boneRep->GetResolution());
  this->SetTolerance(boneRep->GetTolerance());

  // Properties:
  // Enpoint (Head)
  this->EndPointProperty->DeepCopy(boneRep->GetEndPointProperty());
  this->SelectedEndPointProperty->DeepCopy(boneRep->GetSelectedEndPointProperty());
  // Enpoint2 (Tail)
  this->EndPoint2Property->DeepCopy(boneRep->GetEndPoint2Property());
  this->SelectedEndPoint2Property->DeepCopy(boneRep->GetSelectedEndPoint2Property());
  // Line
  this->LineProperty->DeepCopy(boneRep->GetLineProperty());
  this->SelectedLineProperty->DeepCopy(boneRep->GetSelectedLineProperty());

  // vtkBoneWidget copies
  this->SetAlwaysOnTop(boneRep->GetAlwaysOnTop());
  this->SetOpacity(boneRep->GetLineProperty()->GetOpacity());

  // Envelope
  this->ShowEnvelope = boneRep->GetShowEnvelope();
  this->Envelope->GetProperty()->DeepCopy(boneRep->GetEnvelope()->GetProperty());
}

//----------------------------------------------------------------------------
void vtkBoneRepresentation::SetOpacity(double opacity)
{
  this->LineProperty->SetOpacity(opacity);
  this->EndPointProperty->SetOpacity(opacity);
  this->EndPoint2Property->SetOpacity(opacity);

  this->SelectedLineProperty->SetOpacity(opacity);
  this->SelectedEndPointProperty->SetOpacity(opacity);
  this->SelectedEndPoint2Property->SetOpacity(opacity);

  this->TextActor->GetProperty()->SetOpacity(opacity);
}

//----------------------------------------------------------------------------
int vtkBoneRepresentation
::RenderTranslucentPolygonalGeometryInternal(vtkViewport *v)
{
  int count = 0;
  this->BuildRepresentation();
  if (this->ShowEnvelope && this->Envelope->HasTranslucentPolygonalGeometry())
    {
    count += this->Envelope->RenderTranslucentPolygonalGeometry(v);
    }
  count += this->Superclass::RenderTranslucentPolygonalGeometry(v);
  return count;
}

//----------------------------------------------------------------------------
int vtkBoneRepresentation::RenderOpaqueGeometryInternal(vtkViewport *v)
{
  int count = 0;
  this->BuildRepresentation();
  if (this->ShowEnvelope && !this->Envelope->HasTranslucentPolygonalGeometry())
    {
    count += this->Envelope->RenderOpaqueGeometry(v);
    }
  count += this->Superclass::RenderOpaqueGeometry(v);
  return count;
}

//----------------------------------------------------------------------------
int vtkBoneRepresentation::RenderOverlayInternal(vtkViewport *v)
{
  int count = 0;
  this->BuildRepresentation();
  if (this->ShowEnvelope)
    {
    count += this->Envelope->RenderOverlay(v);
    }
  count += this->Superclass::RenderOverlay(v);
  return count;
}
