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
#include "vtkArmatureRepresentation.h"

// VTK includes
#include <vtkCamera.h>
#include <vtkObjectFactory.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>

vtkStandardNewMacro(vtkArmatureRepresentation);

//----------------------------------------------------------------------------
vtkArmatureRepresentation::vtkArmatureRepresentation()
{
  this->Property = vtkProperty::New();
}

//----------------------------------------------------------------------------
vtkArmatureRepresentation::~vtkArmatureRepresentation()
{
  this->Property->Delete();
}

//----------------------------------------------------------------------
void vtkArmatureRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}

//----------------------------------------------------------------------------
void vtkArmatureRepresentation::BuildRepresentation()
{
  // Rebuild only if necessary
  if ( this->GetMTime() > this->BuildTime ||
       (this->Renderer && this->Renderer->GetVTKWindow() &&
        (this->Renderer->GetVTKWindow()->GetMTime() > this->BuildTime ||
        this->Renderer->GetActiveCamera()->GetMTime() > this->BuildTime)) )
    {
    this->BuildTime.Modified();
    }
}
