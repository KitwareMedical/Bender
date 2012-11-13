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
#include "vtkCylinderBoneRepresentation.h"

#include <vtkActor.h>
#include <vtkAppendPolyData.h>
#include <vtkBox.h>
#include <vtkCamera.h>
#include <vtkCallbackCommand.h>
#include <vtkCylinderSource.h>
#include <vtkFollower.h>
#include <vtkInteractorObserver.h>
#include <vtkLineSource.h>
#include <vtkObjectFactory.h>
#include <vtkOpenGL.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkTubeFilter.h>
#include <vtkWindow.h>

vtkStandardNewMacro(vtkCylinderBoneRepresentation);

//----------------------------------------------------------------------------
vtkCylinderBoneRepresentation::vtkCylinderBoneRepresentation()
{
  // Instantiate cylinder representations
  this->CylinderActor = vtkActor::New();
  this->CylinderMapper = vtkPolyDataMapper::New();
  this->CylinderGenerator= vtkTubeFilter::New();

  // Define cylinde properties
  this->Radius = 0.0;
  this->Capping = 1;
  this->NumberOfSides = 5;

  // Make the necessary connections
  this->CylinderGenerator->SetInput(this->LineSource->GetOutput());
  this->CylinderMapper->SetInput(this->CylinderGenerator->GetOutput());
  this->CylinderActor->SetMapper(this->CylinderMapper);

  // Set up the initial properties
  this->CreateDefaultProperties();
  this->CylinderActor->SetProperty(this->CylinderProperty);
}

//----------------------------------------------------------------------------
vtkCylinderBoneRepresentation::~vtkCylinderBoneRepresentation()
{
  this->CylinderProperty->Delete();
  this->SelectedCylinderProperty->Delete();

  this->CylinderGenerator->Delete();
  this->CylinderActor->Delete();
  this->CylinderMapper->Delete();
}

//----------------------------------------------------------------------
void vtkCylinderBoneRepresentation::SetNumberOfSides(int numberOfSides)
{
  this->NumberOfSides = std::max(3, numberOfSides);
}

//----------------------------------------------------------------------
double *vtkCylinderBoneRepresentation::GetBounds()
{
  this->Superclass::GetBounds();
  this->BoundingBox->AddBounds(this->CylinderActor->GetBounds());
  return this->BoundingBox->GetBounds();
}

//----------------------------------------------------------------------------
void vtkCylinderBoneRepresentation::CreateDefaultProperties()
{
  // Cylinder properties
  this->CylinderProperty = vtkProperty::New();
  this->CylinderProperty->SetAmbient(1.0);
  this->CylinderProperty->SetAmbientColor(1.0,1.0,1.0);

  this->SelectedCylinderProperty = vtkProperty::New();
  this->SelectedCylinderProperty->SetAmbient(1.0);
  this->SelectedCylinderProperty->SetAmbientColor(0.0,0.0,1.0);
}

//----------------------------------------------------------------------------
void vtkCylinderBoneRepresentation::BuildRepresentation()
{
  // Rebuild only if necessary
  if ( this->GetMTime() > this->BuildTime ||
       (this->Renderer && this->Renderer->GetVTKWindow() &&
        (this->Renderer->GetVTKWindow()->GetMTime() > this->BuildTime ||
        this->Renderer->GetActiveCamera()->GetMTime() > this->BuildTime)) )
    {
    this->Superclass::BuildRepresentation();
    this->RebuildCylinder();

    this->BuildTime.Modified();
    }
}

//----------------------------------------------------------------------
void vtkCylinderBoneRepresentation::RebuildCylinder()
{
  this->CylinderGenerator->SetCapping( this->Capping );
  this->CylinderGenerator->SetNumberOfSides( this->NumberOfSides );
  this->CylinderGenerator->SetRadius( this->Distance / 10 );
  //this->CylinderGenerator->Update();
}

//----------------------------------------------------------------------
void vtkCylinderBoneRepresentation::GetPolyData(vtkPolyData *pd)
{
  this->RebuildCylinder();
  pd->ShallowCopy( this->CylinderGenerator->GetOutput() );
}

//----------------------------------------------------------------------
void vtkCylinderBoneRepresentation::GetActors(vtkPropCollection *pc)
{
  this->Superclass::GetActors(pc);
  this->CylinderActor->GetActors(pc);
}

//----------------------------------------------------------------------------
void vtkCylinderBoneRepresentation::ReleaseGraphicsResources(vtkWindow *w)
{
  this->Superclass::ReleaseGraphicsResources(w);
  this->CylinderActor->ReleaseGraphicsResources(w);
}

//----------------------------------------------------------------------------
int vtkCylinderBoneRepresentation::RenderOpaqueGeometryInternal(vtkViewport *v)
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
  // Cylinder actor
  count += this->CylinderActor->RenderOpaqueGeometry(v);

  return count;
}

//----------------------------------------------------------------------------
int vtkCylinderBoneRepresentation
::RenderTranslucentPolygonalGeometryInternal(vtkViewport *v)
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
  // Cylinder actor
  count += this->CylinderActor->RenderTranslucentPolygonalGeometry(v);

  return count;
}

//----------------------------------------------------------------------------
int vtkCylinderBoneRepresentation::RenderOverlayInternal(vtkViewport *v)
{
  int count = 0;
  this->BuildRepresentation();
  // Bone representation actors
  count += this->LineActor->RenderOverlay(v);
  count += this->Handle[0]->RenderOverlay(v);
  count += this->Handle[1]->RenderOverlay(v);
  if (this->DistanceAnnotationVisibility)
    {
    count += this->TextActor->RenderOverlay(v);
    }
  // Cylinder actor
  count += this->CylinderActor->RenderOverlay(v);

  return count;
}

//----------------------------------------------------------------------------
int vtkCylinderBoneRepresentation::HasTranslucentPolygonalGeometry()
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
  // Cylinder actor
  count |= this->CylinderActor->HasTranslucentPolygonalGeometry();
  return count;
}

//----------------------------------------------------------------------------
void vtkCylinderBoneRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  if ( this->CylinderProperty )
    {
    os << indent << "Cylinder Property: " << this->CylinderProperty << "\n";
    }
  else
    {
    os << indent << "Cylinder Property: (none)\n";
    }

  if ( this->SelectedCylinderProperty )
    {
    os << indent << "Selected Cylinder Property: "
       << this->SelectedCylinderProperty << "\n";
    }
  else
    {
    os << indent << " Selected Cylinder Property: (none)\n";
    }

  os << indent << "Number Of Sides: " << this->NumberOfSides << "\n";
  os << indent << "Capping: " << this->Capping << "\n";
  os << indent << "Radius: " << this->Radius << "\n";
}



