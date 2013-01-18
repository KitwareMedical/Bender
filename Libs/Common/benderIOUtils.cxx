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

#include "benderIOUtils.h"

#include "vtkSmartPointer.h"
#include <vtkPolyDataReader.h>
#include <vtkXMLPolyDataReader.h>
#include <vtkPolyDataWriter.h>
#include <vtkSTLReader.h>
#include <vtkPolyData.h>

namespace bender
{
//-----------------------------------------------------------------------------
vtkPolyData* IOUtils::ReadPolyData(const std::string& fileName, bool invertXY)
{
  vtkPolyData* polyData = 0;
  vtkSmartPointer<vtkPolyDataReader> pdReader;
  vtkSmartPointer<vtkXMLPolyDataReader> pdxReader;
  vtkSmartPointer<vtkSTLReader> stlReader;

  // do we have vtk or vtp models?
  std::string::size_type loc = fileName.find_last_of(".");
  if( loc == std::string::npos )
    {
    std::cerr << "Failed to find an extension for " << fileName << std::endl;
    return polyData;
    }

  std::string extension = fileName.substr(loc);

  if( extension == std::string(".vtk") )
    {
    pdReader = vtkSmartPointer<vtkPolyDataReader>::New();
    pdReader->SetFileName(fileName.c_str() );
    pdReader->Update();
    polyData = pdReader->GetOutput();
    }
  else if( extension == std::string(".vtp") )
    {
    pdxReader = vtkSmartPointer<vtkXMLPolyDataReader>::New();
    pdxReader->SetFileName(fileName.c_str() );
    pdxReader->Update();
    polyData = pdxReader->GetOutput();
    }
  else if( extension == std::string(".stl") )
    {
    stlReader = vtkSmartPointer<vtkSTLReader>::New();
    stlReader->SetFileName(fileName.c_str() );
    stlReader->Update();
    polyData = stlReader->GetOutput();
    }
  if( polyData == NULL )
    {
    std::cerr << "Failed to read surface " << fileName << std::endl;
    return polyData;
    }
  // Build Cells
  polyData->BuildLinks();

  if(invertXY)
    {
    vtkPoints* points = polyData->GetPoints();
    for(int i=0; i<points->GetNumberOfPoints();++i)
      {
      double x[3];
      points->GetPoint(i,x);
      x[0]*=-1;
      x[1]*=-1;
      points->SetPoint(i, x);
      }
    }
  polyData->Register(0);
  return polyData;
}

//-----------------------------------------------------------------------------
void IOUtils::WritePolyData(vtkPolyData* polyData, const std::string& fileName)
{
  cout<<"Write polydata to "<<fileName<<endl;
  vtkNew<vtkPolyDataWriter> pdWriter;
  pdWriter->SetInput(polyData);
  pdWriter->SetFileName(fileName.c_str() );
  pdWriter->SetFileTypeToBinary();
  pdWriter->Update();
}

};
