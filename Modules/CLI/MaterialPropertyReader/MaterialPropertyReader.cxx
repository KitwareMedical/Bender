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

// ITK includes
#include "itkChangeLabelImageFilter.h"
#include "itkImageFileWriter.h"

#include "itkPluginUtilities.h"
#include "MaterialPropertyReaderCLP.h"

// VTK includes
#include <vtkCleanPolyData.h>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkDoubleArray.h>
#include <vtkTetra.h>
#include <vtkPolyData.h>
#include <vtkPolyDataWriter.h>
#include <vtkNew.h>
#include <vtkSmartPointer.h>

// STD includes
#include <numeric>
#include <sstream>

// Use an anonymous namespace to keep class types and function names
// from colliding when module is used as shared object module.  Every
// thing should be in an anonymous namespace except for the module
// entry point, e.g. main()
//
namespace
{
template <class T> int DoIt( int argc, char * argv[] );

template<typename MaterialMapType>
void ReadFile(const std::string &fileName, MaterialMapType &map);
} // end of anonymous namespace


int main( int argc, char * argv[] )
{
  PARSE_ARGS;

  typedef std::map<int,std::vector<double> > MaterialMapType;
  MaterialMapType materialMap;
  vtkSmartPointer<vtkPolyData> output = vtkSmartPointer<vtkPolyData>::New();

  // Read material properties file
  ReadFile(MaterialFile,materialMap);

  // Read mesh and extract active cell data
  vtkPolyData * tetraMesh = bender::IOUtils::ReadPolyData(
    MeshPoly.c_str());
  vtkIntArray * materialIdArray = vtkIntArray::SafeDownCast(
    tetraMesh->GetCellData()->GetScalars());

  output->DeepCopy(tetraMesh);
  
  // Create new array to store material parameters
  vtkNew<vtkDoubleArray> cellPropArray;

  // In case name has not been specified
  materialIdArray->SetName("MaterialId");
  cellPropArray->SetName("MaterialParameters");

  // max number of parameters is 2, this can change later
  size_t const numOFMaterialParameters = 2;
  cellPropArray->SetNumberOfComponents(numOFMaterialParameters);

  for(size_t i = 0; i < materialIdArray->GetDataSize(); ++i)
    {
    MaterialMapType::mapped_type a(numOFMaterialParameters,0.0);
    int                         id = materialIdArray->GetValue(i);
  
    // assign zero to this element if there is no material property for it
    if(materialMap.end() == materialMap.find(id))
      {
      cellPropArray->InsertNextTupleValue(&a[0]);
      continue;
      }

    // There should be at least two and no more than five parameters per element
    if(materialMap[id].size() < 2 && materialMap[id].size() > numOFMaterialParameters)
      {
      std::cerr << "Not enough material parameters." << std::endl;
      return EXIT_FAILURE;
      }

    for (size_t k = 0, k_end = materialMap[id].size(); k != k_end; ++k)
      a[k] = materialMap[id][k];

    cellPropArray->InsertNextTupleValue(&a[0]);
    }
    
  output->GetCellData()->AddArray(cellPropArray.GetPointer());
  output->GetCellData()->SetScalars(materialIdArray);
  bender::IOUtils::WritePolyData(output, OutputMesh);

  return EXIT_SUCCESS;
}

// Use an anonymous namespace to keep class types and function names
// from colliding when module is used as shared object module.  Every
// thing should be in an anonymous namespace except for the module
// entry point, e.g. main()
//
namespace
{

template<typename MaterialMapType>
void ReadFile(const std::string& fileName, MaterialMapType &parameterMap)
{
  std::ifstream     fileStream(fileName.c_str());
  std::stringstream lineStream;

  size_t size;

  std::string line;
  std::getline(fileStream,line);

  lineStream << line;
  lineStream >> size;
  while(std::getline(fileStream,line))
    {
    int    index;
    double p1,p2;
  
    // Clean the stream first
    lineStream << "";
    lineStream.clear();
  
    lineStream << line;
    lineStream >> index >> p1 >> p2;

    parameterMap[index].push_back(p1);
    parameterMap[index].push_back(p2);
    }
}

} // end of anonymous namespace
