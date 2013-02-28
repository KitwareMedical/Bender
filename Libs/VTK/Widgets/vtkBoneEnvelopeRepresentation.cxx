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
#include "vtkBoneEnvelopeRepresentation.h"
#include "vtkCapsuleSource.h"

// VTK includes
#include <vtkActor.h>
#include <vtkAppendPolyData.h>
#include <vtkCamera.h>
#include <vtkInteractorObserver.h>
#include <vtkMath.h>
#include <vtkObjectFactory.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkTransform.h>
#include <vtkWindow.h>

vtkStandardNewMacro(vtkBoneEnvelopeRepresentation);

//----------------------------------------------------------------------------
vtkBoneEnvelopeRepresentation::vtkBoneEnvelopeRepresentation()
{
  this->Head[0] = 0.0; this->Head[1] = 0.0; this->Head[2] = 0.0;
  this->Tail[0] = 1.0; this->Tail[1] = 0.0; this->Tail[2] = 0.0;
  this->Radius = 10.0;
  this->Rotation = vtkTransform::New();

  // Represent envelope
  this->CapsuleSource = vtkCapsuleSource::New();
  this->CapsuleSource->SetCenter(0.5, 0.0, 0.0);
  this->CapsuleSource->SetCylinderLength(1.0);
  this->CapsuleSource->SetThetaResolution(this->GetResolution());
  this->CapsuleSource->SetPhiResolution(this->GetResolution());

  // Mapper
  this->EnvelopeMapper = vtkPolyDataMapper::New();
  this->EnvelopeMapper->SetInput(this->CapsuleSource->GetOutput());
  // Actor
  this->EnvelopeActor = vtkActor::New();
  this->EnvelopeActor->SetMapper(this->EnvelopeMapper);

  // Set up the initial properties
  this->CreateDefaultProperties();
  this->EnvelopeActor->SetProperty(this->Property);
}

//----------------------------------------------------------------------------
vtkBoneEnvelopeRepresentation::~vtkBoneEnvelopeRepresentation()
{
  this->Rotation->Delete();
  this->EnvelopeActor->Delete();
  this->EnvelopeMapper->Delete();
  this->CapsuleSource->Delete();
  this->Property->Delete();
}

//----------------------------------------------------------------------------
void vtkBoneEnvelopeRepresentation::SetResolution(int r)
{
  this->CapsuleSource->SetPhiResolution(r);
  this->CapsuleSource->SetThetaResolution(r);
}

//----------------------------------------------------------------------------
int vtkBoneEnvelopeRepresentation::GetResolution()
{
  return this->CapsuleSource->GetPhiResolution();
}

//----------------------------------------------------------------------------
void vtkBoneEnvelopeRepresentation::GetPolyData(vtkPolyData *pd)
{
  this->RebuildEnvelope();
  pd->ShallowCopy(this->CapsuleSource->GetOutput());
}

//----------------------------------------------------------------------------
void vtkBoneEnvelopeRepresentation::RebuildEnvelope()
{
  double x[3];
  vtkMath::Subtract(this->Tail, this->Head, x);
  this->CapsuleSource->SetCylinderLength(vtkMath::Norm(x));
  this->CapsuleSource->SetRadius(this->Radius);

  vtkMath::MultiplyScalar(x, 0.5);
  vtkMath::Add(this->Head, x, x);
  vtkSmartPointer<vtkTransform> envelopeTransform =
    vtkSmartPointer<vtkTransform>::New();
  envelopeTransform->Translate(x);
  envelopeTransform->Concatenate(this->Rotation);


  this->EnvelopeActor->SetUserTransform(envelopeTransform);
}

//----------------------------------------------------------------------------
void vtkBoneEnvelopeRepresentation::CreateDefaultProperties()
{
  // Endpoint properties
  this->Property = vtkProperty::New();
  this->Property->SetColor(0.99, 0.99 ,1); // Ghost white
  this->Property->SetEdgeVisibility(0);
  this->Property->SetFrontfaceCulling(true);
}

//----------------------------------------------------------------------------
void vtkBoneEnvelopeRepresentation::BuildRepresentation()
{
  // Rebuild only if necessary
  if ( this->GetMTime() > this->BuildTime ||
       (this->Renderer && this->Renderer->GetVTKWindow() &&
        (this->Renderer->GetVTKWindow()->GetMTime() > this->BuildTime ||
        this->Renderer->GetActiveCamera()->GetMTime() > this->BuildTime)) )
    {
    //std::cout<<"Building envelope !"<<std::endl;
    this->RebuildEnvelope();

    this->BuildTime.Modified();
    }
}

//----------------------------------------------------------------------------
void vtkBoneEnvelopeRepresentation::GetActors(vtkPropCollection *pc)
{
  this->EnvelopeActor->GetActors(pc);
}

//----------------------------------------------------------------------------
void vtkBoneEnvelopeRepresentation::ReleaseGraphicsResources(vtkWindow *w)
{
  this->EnvelopeActor->ReleaseGraphicsResources(w);
}

//----------------------------------------------------------------------------
int vtkBoneEnvelopeRepresentation::RenderOpaqueGeometry(vtkViewport *v)
{
  this->BuildRepresentation();
  return this->EnvelopeActor->RenderOpaqueGeometry(v);
}

//----------------------------------------------------------------------------
int vtkBoneEnvelopeRepresentation
::RenderTranslucentPolygonalGeometry(vtkViewport *v)
{
  this->BuildRepresentation();
  return this->EnvelopeActor->RenderTranslucentPolygonalGeometry(v);
}

//----------------------------------------------------------------------------
int vtkBoneEnvelopeRepresentation::HasTranslucentPolygonalGeometry()
{
  return this->EnvelopeActor->HasTranslucentPolygonalGeometry();
}

//----------------------------------------------------------------------------
unsigned long vtkBoneEnvelopeRepresentation::GetMTime()
{
  unsigned long mTime=this->Superclass::GetMTime();
  unsigned long mTime2=this->CapsuleSource->GetMTime();
  mTime = ( mTime2 > mTime ? mTime2 : mTime );
  return mTime;
}

//----------------------------------------------------------------------------
void vtkBoneEnvelopeRepresentation::DeepCopy(vtkProp* other)
{
  vtkBoneEnvelopeRepresentation *rep
    = vtkBoneEnvelopeRepresentation::SafeDownCast(other);
  if (rep)
    {
    // Properties:
    this->Property->DeepCopy(rep->GetProperty());

    // Envelope
    this->SetHead(rep->GetHead());
    this->SetTail(rep->GetTail());
    this->SetRadius(rep->GetRadius());
    this->SetResolution(rep->GetResolution());
    }

  this->Superclass::ShallowCopy(other);
}

//----------------------------------------------------------------------------
void vtkBoneEnvelopeRepresentation
::SetWorldToBoneRotation(vtkTransform* worldToBoneRotation)
{
  this->Rotation->DeepCopy(worldToBoneRotation);
}

//----------------------------------------------------------------------------
void vtkBoneEnvelopeRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  if ( this->Property )
    {
    os << indent << "Property: " << this->Property << "\n";
    }
  else
    {
    os << indent << "Property: (none)\n";
    }

  int res = this->GetResolution();

  os << indent << "Resolution: " << res << "\n";
  os << indent << "Head: (" << this->Head[0] << ", "
                            << this->Head[1] << ", "
                            << this->Head[2] << ")\n";
  os << indent << "Tail: (" << this->Tail[0] << ", "
                            << this->Tail[1] << ", "
                            << this->Tail[2] << ")\n";
  os << indent << "Radius: " << this->GetRadius() << "\n";
}
