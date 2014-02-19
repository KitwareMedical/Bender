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

#include "vtkDoubleConeBoneRepresentation.h"

// Bender includes
#include "vtkBoneEnvelopeRepresentation.h"

#include <vtkActor.h>
#include <vtkAppendPolyData.h>
#include <vtkBox.h>
#include <vtkCamera.h>
#include <vtkCellPicker.h>
#include <vtkConeSource.h>
#include <vtkFollower.h>
#include <vtkLineSource.h>
#include <vtkObjectFactory.h>
#include <vtkOpenGL.h>
#include <vtkPointHandleRepresentation3D.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkTubeFilter.h>
#include <vtkWindow.h>

vtkStandardNewMacro(vtkDoubleConeBoneRepresentation);

//----------------------------------------------------------------------------
vtkDoubleConeBoneRepresentation::vtkDoubleConeBoneRepresentation()
{
  // Instantiate cones representation
  this->ConesActor = vtkActor::New();
  this->ConesMapper = vtkPolyDataMapper::New();
  this->Cone1 = vtkConeSource::New();
  this->Cone2 = vtkConeSource::New();
  this->GlueFilter = vtkAppendPolyData::New();
  this->ConesPicker = vtkCellPicker::New();

  // Set up the initial properties
  this->CreateDefaultProperties();
  this->ConesActor->SetProperty(this->ConesProperty);

  // Initialize self properties
  this->Radius = 0.0;
  this->NumberOfSides = 5;
  this->Ratio = 0.25;
  this->Capping = 0;

  // Add a picker
  this->ConesPicker->SetTolerance(0.005);
  this->ConesPicker->AddPickList(this->ConesActor);
  this->ConesPicker->PickFromListOn();

  // Make the filters connections
  this->GlueFilter->AddInput( this->Cone1->GetOutput() );
  this->GlueFilter->AddInput( this->Cone2->GetOutput() );
  this->ConesMapper->SetInput(this->GlueFilter->GetOutput());
  this->ConesActor->SetMapper(this->ConesMapper);
}

//----------------------------------------------------------------------------
vtkDoubleConeBoneRepresentation::~vtkDoubleConeBoneRepresentation()
{
  this->ConesProperty->Delete();
  this->SelectedConesProperty->Delete();

  this->Cone1->Delete();
  this->Cone2->Delete();

  this->ConesPicker->Delete();
  this->GlueFilter->Delete();
  this->ConesActor->Delete();
  this->ConesMapper->Delete();
}

//----------------------------------------------------------------------
void vtkDoubleConeBoneRepresentation::SetRatio(double ratio)
{
  double minimizedRatio = std::max(0.0001, ratio);
  this->Ratio = std::min(0.99999, minimizedRatio);
}

//----------------------------------------------------------------------
void vtkDoubleConeBoneRepresentation::SetNumberOfSides(int numberOfSides)
{
  this->NumberOfSides = std::max(3, numberOfSides);
}

//----------------------------------------------------------------------
double *vtkDoubleConeBoneRepresentation::GetBounds()
{
  this->Superclass::GetBounds();
  this->BoundingBox->AddBounds(this->ConesActor->GetBounds());
  return this->BoundingBox->GetBounds();
}

//----------------------------------------------------------------------------
void vtkDoubleConeBoneRepresentation::CreateDefaultProperties()
{
  // Cones properties
  this->ConesProperty = vtkProperty::New();
  this->ConesProperty->SetAmbient(1.0);
  this->ConesProperty->SetAmbientColor(1.0,1.0,1.0);
  //this->ConesProperty->SetOpacity(0.3);

  this->SelectedConesProperty = vtkProperty::New();
  this->SelectedConesProperty->SetAmbient(1.0);
  this->SelectedConesProperty->SetAmbientColor(0.0,1.0,0.0);
  //this->SelectedConesProperty->SetOpacity(0.3);

}

//----------------------------------------------------------------------------
void vtkDoubleConeBoneRepresentation::BuildRepresentation()
{
  // Rebuild only if necessary
  if ( this->GetMTime() > this->BuildTime ||
       (this->Renderer && this->Renderer->GetVTKWindow() &&
        (this->Renderer->GetVTKWindow()->GetMTime() > this->BuildTime ||
        this->Renderer->GetActiveCamera()->GetMTime() > this->BuildTime)) )
    {
    this->Superclass::BuildRepresentation();
    this->RebuildCones();

    this->BuildTime.Modified();
    }
}

//----------------------------------------------------------------------
void vtkDoubleConeBoneRepresentation::RebuildCones()
{
  double x1[3], x2[3], vect[3];
  this->GetPoint1WorldPosition(x1);
  this->GetPoint2WorldPosition(x2);
  vect[0] = x2[0] - x1[0];
  vect[1] = x2[1] - x1[1];
  vect[2] = x2[2] - x1[2];

  double cone1Ratio = this->Ratio * 0.5;
  double cone2Ratio = (1 + this->Ratio) * 0.5;
  this->Cone1->SetCenter( x1[0] + (vect[0] * cone1Ratio),
                          x1[1] + (vect[1] * cone1Ratio),
                          x1[2] + (vect[2] * cone1Ratio) );
  this->Cone1->SetDirection(-vect[0], -vect[1], -vect[2]);
  this->Cone1->SetHeight( this->Distance * this->Ratio );
  this->Cone1->SetRadius( this->Distance / 10 );
  this->Cone1->SetCapping(this->Capping);
  this->Cone1->SetResolution(this->NumberOfSides);

  this->Cone2->SetCenter( x1[0] + (vect[0] * cone2Ratio),
                          x1[1] + (vect[1] * cone2Ratio),
                          x1[2] + (vect[2] * cone2Ratio) );

  this->Cone2->SetDirection(vect);
  this->Cone2->SetHeight( this->Distance * (1 - this->Ratio));
  this->Cone2->SetRadius( this->Distance / 10 );
  this->Cone2->SetCapping(this->Capping);
  this->Cone2->SetResolution(this->NumberOfSides);

  //this->GlueFilter->Update();
}

//----------------------------------------------------------------------
void vtkDoubleConeBoneRepresentation::GetPolyData(vtkPolyData *pd)
{
  this->RebuildCones();
  pd->ShallowCopy( this->GlueFilter->GetOutput() );
}

//----------------------------------------------------------------------
void vtkDoubleConeBoneRepresentation::GetActors(vtkPropCollection *pc)
{
  this->Superclass::GetActors(pc);
  this->ConesActor->GetActors(pc);
}

//----------------------------------------------------------------------------
void vtkDoubleConeBoneRepresentation::ReleaseGraphicsResources(vtkWindow *w)
{
  this->Superclass::ReleaseGraphicsResources(w);
  this->ConesActor->ReleaseGraphicsResources(w);
}

//----------------------------------------------------------------------------
int vtkDoubleConeBoneRepresentation
::RenderOpaqueGeometryInternal(vtkViewport *v)
{
  int count = 0;
  this->BuildRepresentation();
  if (this->ShowEnvelope && !this->Envelope->HasTranslucentPolygonalGeometry())
    {
    count += this->Envelope->RenderOpaqueGeometry(v);
    }
  // Bone representation actors
  count += this->LineActor->RenderOpaqueGeometry(v);
  // Cones actor
  count += this->ConesActor->RenderOpaqueGeometry(v);
  // Handles after cones
  count += this->Handle[0]->RenderOpaqueGeometry(v);
  count += this->Handle[1]->RenderOpaqueGeometry(v);
  if (this->DistanceAnnotationVisibility)
    {
    count += this->TextActor->RenderOpaqueGeometry(v);
    }
  return count;
}

//----------------------------------------------------------------------------
int vtkDoubleConeBoneRepresentation
::RenderTranslucentPolygonalGeometryInternal(vtkViewport *v)
{
  int count = 0;
  this->BuildRepresentation();
  if (this->ShowEnvelope && this->Envelope->HasTranslucentPolygonalGeometry())
    {
    count += this->Envelope->RenderTranslucentPolygonalGeometry(v);
    }
  // Bone representation actors
  count += this->LineActor->RenderTranslucentPolygonalGeometry(v);
  // Cones actor
  count += this->ConesActor->RenderTranslucentPolygonalGeometry(v);
  // Handles after cones
  count += this->Handle[0]->RenderTranslucentPolygonalGeometry(v);
  count += this->Handle[1]->RenderTranslucentPolygonalGeometry(v);
  if (this->DistanceAnnotationVisibility)
    {
    count += this->TextActor->RenderTranslucentPolygonalGeometry(v);
    }
  return count;
}

//----------------------------------------------------------------------------
int vtkDoubleConeBoneRepresentation::RenderOverlayInternal(vtkViewport *v)
{
  int count = 0;
  this->BuildRepresentation();
  if (this->ShowEnvelope)
    {
    count += this->Envelope->RenderOverlay(v);
    }
  // Bone representation actors
  count += this->LineActor->RenderOverlay(v);
  // Cones actor
  count += this->ConesActor->RenderOverlay(v);
  // Handles after cones;
  count += this->Handle[0]->RenderOverlay(v);
  count += this->Handle[1]->RenderOverlay(v);
  if (this->DistanceAnnotationVisibility)
    {
    count += this->TextActor->RenderOverlay(v);
    }
  return count;
}

//----------------------------------------------------------------------------
int vtkDoubleConeBoneRepresentation::HasTranslucentPolygonalGeometry()
{
  int count = 0;
  this->BuildRepresentation();
  if (this->ShowEnvelope)
    {
    count |= this->Envelope->HasTranslucentPolygonalGeometry();
    }
  // Bone representation actors
  count |= this->LineActor->HasTranslucentPolygonalGeometry();
  // Cones actor
  count |= this->ConesActor->HasTranslucentPolygonalGeometry();
  // Handles after cones
  count |= this->Handle[0]->HasTranslucentPolygonalGeometry();
  count |= this->Handle[1]->HasTranslucentPolygonalGeometry();
  if (this->DistanceAnnotationVisibility)
    {
    count |= this->TextActor->HasTranslucentPolygonalGeometry();
    }
  return count;
}

//----------------------------------------------------------------------------
int vtkDoubleConeBoneRepresentation::HasOnlyTranslucentPolygonalGeometry()
{
  int count = 0;
  this->BuildRepresentation();
  // Bone representation actors
  count |= this->LineActor->HasTranslucentPolygonalGeometry();
  // Cones actor
  count &= this->ConesActor->HasTranslucentPolygonalGeometry();
  // Handles after cones
  count &= this->Handle[0]->HasTranslucentPolygonalGeometry();
  count &= this->Handle[1]->HasTranslucentPolygonalGeometry();
  if (this->DistanceAnnotationVisibility)
    {
    count &= this->TextActor->HasTranslucentPolygonalGeometry();
    }
  if (this->ShowEnvelope)
    {
    count &= this->Envelope->HasTranslucentPolygonalGeometry();
    }

  return count;
}

//----------------------------------------------------------------------------
void vtkDoubleConeBoneRepresentation::SetOpacity(double opacity)
{
  this->Superclass::SetOpacity(opacity);
  this->ConesProperty->SetOpacity(opacity);
  this->SelectedConesProperty->SetOpacity(opacity);
}


//----------------------------------------------------------------------------
void vtkDoubleConeBoneRepresentation::SetAlwaysOnTop(int onTop)
{
  if (onTop == this->AlwaysOnTop)
    {
    return;
    }

  this->ConesProperty->SetBackfaceCulling(onTop);
  this->SelectedConesProperty->SetBackfaceCulling(onTop);
  this->Superclass::SetAlwaysOnTop(onTop);
}

//----------------------------------------------------------------------------
void vtkDoubleConeBoneRepresentation::Highlight(int highlight)
{
  this->Superclass::Highlight(highlight);
  if (highlight)
    {
    this->ConesActor->SetProperty(this->SelectedConesProperty);
    }
  else
    {
    this->ConesActor->SetProperty(this->ConesProperty);
    }
}

//----------------------------------------------------------------------------
int vtkDoubleConeBoneRepresentation
::ComputeInteractionState(int X, int Y, int modifier)
{
  this->InteractionState =
    this->Superclass::ComputeInteractionState(X, Y, modifier);
  if (this->InteractionState == vtkBoneRepresentation::Outside
    && this->ConesPicker->Pick(X,Y,0.0,this->Renderer))
    {
    this->InteractionState = vtkBoneRepresentation::OnLine;
    this->SetRepresentationState(this->InteractionState);

    double closest[3];
    this->ConesPicker->GetPickPosition(closest);
    this->LineHandleRepresentation->SetWorldPosition(closest);
    }

  return this->InteractionState;
}

//----------------------------------------------------------------------------
void vtkDoubleConeBoneRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  if ( this->ConesProperty )
    {
    os << indent << "Cone Property: " << this->ConesProperty << "\n";
    }
  else
    {
    os << indent << "Cone Property: (none)\n";
    }

  if ( this->SelectedConesProperty )
    {
    os << indent << "Selected Cone Property: "
       << this->SelectedConesProperty << "\n";
    }
  else
    {
    os << indent << "Selected Cone Property: (none)\n";
    }

  os << indent << "Number Of Sides: " << this->NumberOfSides << "\n";
  os << indent << "Radius: " << this->Radius << "\n";
}



