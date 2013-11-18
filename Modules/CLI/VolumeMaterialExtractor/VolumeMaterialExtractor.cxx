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
#include "VolumeMaterialExtractorCLP.h"
#include "benderIOUtils.h"

// ITK includes
#include <itksys/SystemTools.hxx>

// VTK includes
#include <vtkCleanPolyData.h>
#include <vtkGeometryFilter.h>
#include <vtkNew.h>
#include <vtkPolyDataReader.h>
#include <vtkSmartPointer.h>
#include <vtkThreshold.h>
#include <vtkUnstructuredGrid.h>

int main( int argc, char * argv[] )
{
  PARSE_ARGS;

  vtkSmartPointer<vtkPolyDataReader> reader =
    vtkSmartPointer<vtkPolyDataReader>::New();
  reader->SetFileName(InputTetMesh.c_str());
  reader->Update();
  
  vtkNew<vtkThreshold> threshold;
  threshold->SetInput(reader->GetOutput());
  
  threshold->ThresholdBetween(double(MaterialLabel),double(MaterialLabel));

  vtkNew<vtkGeometryFilter> polyMesh;
  polyMesh->SetInput(threshold->GetOutput());
  
  vtkNew<vtkCleanPolyData> cleanFilter;
  cleanFilter->SetInput(polyMesh->GetOutput());

  bender::IOUtils::WritePolyData(cleanFilter->GetOutput(), OutputTetMesh);

  return EXIT_SUCCESS;
}

