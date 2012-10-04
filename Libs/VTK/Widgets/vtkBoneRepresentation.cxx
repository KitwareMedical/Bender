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
}

//----------------------------------------------------------------------------
vtkBoneRepresentation::~vtkBoneRepresentation()
{
}

//----------------------------------------------------------------------
void vtkBoneRepresentation::GetHeadWorldPosition(double pos[3])
{
  this->Point1Representation->GetWorldPosition(pos);
}

//----------------------------------------------------------------------
double* vtkBoneRepresentation::GetHeadWorldPosition()
{
  return this->Point1Representation->GetWorldPosition();
}

//----------------------------------------------------------------------
void vtkBoneRepresentation::GetHeadDisplayPosition(double pos[3])
{
  this->Point1Representation->GetDisplayPosition(pos);
}

//----------------------------------------------------------------------
double* vtkBoneRepresentation::GetHeadDisplayPosition()
{
  return this->Point1Representation->GetDisplayPosition();
}

//----------------------------------------------------------------------
void vtkBoneRepresentation::SetHeadWorldPosition(double x[3])
{
  this->Point1Representation->SetWorldPosition(x);
  this->LineSource->SetPoint1(x);
}

//----------------------------------------------------------------------
void vtkBoneRepresentation::SetHeadDisplayPosition(double x[3])
{
  this->Point1Representation->SetDisplayPosition(x);
  double p[3];
  this->Point1Representation->GetWorldPosition(p);
  this->Point1Representation->SetWorldPosition(p);
}

//----------------------------------------------------------------------
void vtkBoneRepresentation::GetTailWorldPosition(double pos[3])
{
  this->Point2Representation->GetWorldPosition(pos);
}

double* vtkBoneRepresentation::GetTailWorldPosition()
{
  return this->Point2Representation->GetWorldPosition();
}

//----------------------------------------------------------------------
void vtkBoneRepresentation::GetTailDisplayPosition(double pos[3])
{
  this->Point2Representation->GetDisplayPosition(pos);
}

//----------------------------------------------------------------------
double* vtkBoneRepresentation::GetTailDisplayPosition()
{
  return this->Point2Representation->GetDisplayPosition();
}

//----------------------------------------------------------------------
void vtkBoneRepresentation::SetTailWorldPosition(double x[3])
{
  this->Point2Representation->SetWorldPosition(x);
  this->LineSource->SetPoint2(x);
  //double p[3];
  //this->Point2Representation->GetDisplayPosition(p);
  //this->Point2Representation->SetDisplayPosition(p);
}

//----------------------------------------------------------------------
void vtkBoneRepresentation::SetTailDisplayPosition(double x[3])
{
  this->Point2Representation->SetDisplayPosition(x);
  double p[3];
  this->Point2Representation->GetWorldPosition(p);
  this->Point2Representation->SetWorldPosition(p);
}

//----------------------------------------------------------------------
vtkPointHandleRepresentation3D* vtkBoneRepresentation::GetHeadRepresentation()
{
  return this->GetPoint1Representation();
}

//----------------------------------------------------------------------
vtkPointHandleRepresentation3D* vtkBoneRepresentation::GetTailRepresentation()
{
  return this->GetPoint2Representation();
}

//----------------------------------------------------------------------
void vtkBoneRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}
