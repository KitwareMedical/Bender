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

#include "vtkActor.h"
#include "vtkAppendPolyData.h"
#include "vtkBox.h"
#include "vtkCamera.h"
#include "vtkConeSource.h"
#include "vtkFollower.h"
#include "vtkLineSource.h"
#include "vtkObjectFactory.h"
#include "vtkPolyData.h"
#include "vtkPolyDataMapper.h"
#include "vtkProperty.h"
#include "vtkRenderer.h"
#include "vtkSmartPointer.h"
#include "vtkTubeFilter.h"
#include "vtkWindow.h"

vtkStandardNewMacro(vtkDoubleConeBoneRepresentation);

//----------------------------------------------------------------------------
vtkDoubleConeBoneRepresentation::vtkDoubleConeBoneRepresentation()
{
  // Cones representation
  this->ConesActor = vtkActor::New();
  this->ConesMapper = vtkPolyDataMapper::New();
  this->Cone1 = vtkConeSource::New();
  this->Cone2 = vtkConeSource::New();
  this->GlueFilter = vtkAppendPolyData::New();

  // Set up the initial properties
  this->CreateDefaultProperties();
  this->ConesActor->SetProperty(this->ConesProperty);

  // Initialize self properties
  this->Radius = 0.0;
  this->NumberOfSides = 5;
  this->Ratio = 0.25;
  this->Capping = 1;

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
  this->SelectedConesProperty->SetAmbientColor(1.0,1.0,1.0);
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
int vtkDoubleConeBoneRepresentation::RenderOpaqueGeometry(vtkViewport *v)
{
  int count = 0;
  this->BuildRepresentation();
  // Bone representation actors
  count += this->LineActor->RenderOpaqueGeometry(v);
  count += this->Handle[0]->RenderOpaqueGeometry(v);
  count += this->Handle[1]->RenderOpaqueGeometry(v);
  if (this->DistanceAnnotationVisibility)
    {
    count += this->TextActor->RenderOpaqueGeometry(v);
    }
  // Cones actor
  count += this->ConesActor->RenderOpaqueGeometry(v);
  return count;
}

//----------------------------------------------------------------------------
int vtkDoubleConeBoneRepresentation::RenderTranslucentPolygonalGeometry(vtkViewport *v)
{
  int count = 0;
  this->BuildRepresentation();
  // Bone representation actors
  count += this->LineActor->RenderTranslucentPolygonalGeometry(v);
  count += this->Handle[0]->RenderTranslucentPolygonalGeometry(v);
  count += this->Handle[1]->RenderTranslucentPolygonalGeometry(v);
  if (this->DistanceAnnotationVisibility)
    {
    count += this->TextActor->RenderTranslucentPolygonalGeometry(v);
    }
  // Cones actor
  count += this->ConesActor->RenderTranslucentPolygonalGeometry(v);
  return count;
}

//----------------------------------------------------------------------------
int vtkDoubleConeBoneRepresentation::HasTranslucentPolygonalGeometry()
{
  int count = 0;
  this->BuildRepresentation();
  // Bone representation actors
  count |= this->LineActor->HasTranslucentPolygonalGeometry();
  count |= this->Handle[0]->HasTranslucentPolygonalGeometry();
  count |= this->Handle[1]->HasTranslucentPolygonalGeometry();
  if (this->DistanceAnnotationVisibility)
    {
    count |= this->TextActor->HasTranslucentPolygonalGeometry();
    }
  // Cones actor
  count |= this->ConesActor->HasTranslucentPolygonalGeometry();
  return count;
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



