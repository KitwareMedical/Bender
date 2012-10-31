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
#include <vtkSphereSource.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkVectorText.h>
#include <vtkWindow.h>

vtkStandardNewMacro(vtkBoneRepresentation);

//----------------------------------------------------------------------------
vtkBoneRepresentation::vtkBoneRepresentation()
{
  this->AlwaysOnTop = 1;
}

//----------------------------------------------------------------------------
vtkBoneRepresentation::~vtkBoneRepresentation()
{
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
void vtkBoneRepresentation::GetDisplayHeadPosition(double pos[2])
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
}

//----------------------------------------------------------------------------
void vtkBoneRepresentation::SetDisplayHeadPosition(double x[2])
{
  this->Superclass::SetPoint1DisplayPosition(x);
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
void vtkBoneRepresentation::GetDisplayTailPosition(double pos[2])
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
}

//----------------------------------------------------------------------------
void vtkBoneRepresentation::SetDisplayTailPosition(double x[2])
{
  this->Superclass::SetPoint2DisplayPosition(x);
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
}

//----------------------------------------------------------------------------
void vtkBoneRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  os << indent << "Always On Top: " << this->AlwaysOnTop << "\n";
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

    if (this->HasTranslucentPolygonalGeometry())
      {
      count = this->RenderTranslucentPolygonalGeometryInternal(v);
      }
    else
      {
      count = this->RenderOpaqueGeometryInternal(v);
      }

    if(flag != GL_ALWAYS)
      {
      glDepthFunc(flag);
      }
    }

  return count;
}

//----------------------------------------------------------------------------
int vtkBoneRepresentation
::RenderTranslucentPolygonalGeometryInternal(vtkViewport *v)
{
  return this->Superclass::RenderTranslucentPolygonalGeometry(v);
}

//----------------------------------------------------------------------------
int vtkBoneRepresentation::RenderOpaqueGeometryInternal(vtkViewport *v)
{
  return this->Superclass::RenderOpaqueGeometry(v);
}
