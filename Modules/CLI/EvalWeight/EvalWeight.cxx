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

#include "EvalWeightCLP.h"

//------- Bender-----------
#include "benderWeightMap.h"
#include "benderWeightMapMath.h"
#include "benderWeightMapIO.h"

//--------ITK --------------
#include <itkImageFileWriter.h>
#include <itkImage.h>
#include <itkPluginUtilities.h>
#include <itkImageRegionIteratorWithIndex.h>
#include <itkContinuousIndex.h>
#include <itkMath.h>
#include <itkIndex.h>
#include <itkMatrix.h>
#include <itkVariableLengthVector.h>
#include <itkDirectory.h>

//--------VTK --------------
#include <vtkTimerLog.h>
#include <vtkSTLReader.h>
#include <vtkPolyData.h>
#include <vtkCellArray.h>
#include <vtkNew.h>
#include <vtkSmartPointer.h>
#include <vtkPointData.h>
#include <vtkCellData.h>
#include <vtkFloatArray.h>
#include <vtkMath.h>
#include <vtkPolyDataReader.h>
#include <vtkXMLPolyDataReader.h>
#include <vtkPolyDataWriter.h>
#include <vtksys/SystemTools.hxx>

//--------standard-------------
#include <sstream>
#include <iostream>
#include <vector>
#include <limits>
#include <math.h>


using namespace std;

typedef itk::Image<float, 3>  WeightImage;
typedef itk::Image<bool, 3>  BoolImage;

typedef itk::Index<3> Voxel;
typedef itk::Offset<3> VoxelOffset;
typedef itk::ImageRegion<3> Region;


//-------------------------------------------------------------------------------
template<class T>
void PrintVector(T* a, int n)
{
  cout<<"[";
  for(int i=0; i<n; ++i)
    {
    cout<<a[i]<<(i==n-1?"": ", ");
    }
  cout<<"]"<<endl;
}

//-------------------------------------------------------------------------------
class CubeNeighborhood
{
public:
  CubeNeighborhood()
  {
    int index=0;
    for(unsigned int i=0; i<=1; ++i)
      {
      for(unsigned int j=0; j<=1; ++j)
        {
        for(unsigned int k=0; k<=1; ++k,++index)
          {
          this->Offsets[index][0] = i;
          this->Offsets[index][1] = j;
          this->Offsets[index][2] = k;
          }
        }
      }
  }
  VoxelOffset Offsets[8];
};

//-------------------------------------------------------------------------------
vtkPolyData* ReadPolyData(const std::string& fileName, bool invertY=false)
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

  if(invertY)
    {
    vtkPoints* points = polyData->GetPoints();
    for(int i=0; i<points->GetNumberOfPoints();++i)
      {
      double x[3];
      points->GetPoint(i,x);
      x[1]*=-1;
      points->SetPoint(i, x);
      }
    }
  polyData->Register(0);
  return polyData;
}


//-------------------------------------------------------------------------------
void ComputeDomainVoxels(WeightImage::Pointer image //input
                         ,vtkPoints* points //input
                         ,std::vector<Voxel>& domainVoxels //output
                         )
{
  CubeNeighborhood cubeNeighborhood;
  VoxelOffset* offsets = cubeNeighborhood.Offsets;

  BoolImage::Pointer domain = BoolImage::New();
  domain->SetRegions(image->GetLargestPossibleRegion());
  domain->Allocate();
  domain->FillBuffer(false);

  for(int pi=0; pi<points->GetNumberOfPoints();++pi)
    {
    double xraw[3];
    points->GetPoint(pi,xraw);

    itk::Point<double,3> x(xraw);
    itk::ContinuousIndex<double,3> coord;
    image->TransformPhysicalPointToContinuousIndex(x, coord);

    Voxel p;
    p.CopyWithCast(coord);

    for(int iOff=0; iOff<8; ++iOff)
      {
      Voxel q = p + offsets[iOff];
      if(!domain->GetPixel(q))
        {
        domain->SetPixel(q,true);
        domainVoxels.push_back(q);
        }
      }
    }
}

//-----------------------------------------------------------------------------
void WritePolyData(vtkPolyData* polyData, const std::string& fileName)
{
  cout<<"Wrote polydata to "<<fileName<<endl;
  vtkNew<vtkPolyDataWriter> pdWriter;
  pdWriter->SetInput(polyData);
  pdWriter->SetFileName(fileName.c_str() );
  pdWriter->SetFileTypeToBinary();
  pdWriter->Update();
}


//-------------------------------------------------------------------------------
int main( int argc, char * argv[] )
{
  PARSE_ARGS;
  typedef bender::WeightMap WeightMap;

  cout<<"Evaluate weight in  "<<WeightDirectory<<endl;
  cout<<"Evaluating surface: "<<InputSurface<<endl;
  if(InvertY)
    {
    cout<<"Invert y coordinate\n";
    }
  cout<<"Output to "<<OutputSurface<<endl;

  //----------------------------
  // Read the first weight image
  // and all file names
  //----------------------------
  vector<string> fnames;
  bender::GetWeightFileNames(WeightDirectory, fnames);
  int numSites = fnames.size();
  if(numSites<1)
    {
    cerr<<"No weight file is found."<<endl;
    return 1;
    }

  typedef itk::ImageFileReader<WeightImage>  ReaderType;
  ReaderType::Pointer reader = ReaderType::New();
  reader->SetFileName(fnames[0].c_str());
  reader->Update();

  WeightImage::Pointer weight0 =  reader->GetOutput();
  Region weightRegion = weight0->GetLargestPossibleRegion();
  cout<<"Weight volume description: "<<endl;
  cout<<weightRegion<<endl;

  int numForeGround(0);
  for(itk::ImageRegionIterator<WeightImage> it(weight0,weightRegion);!it.IsAtEnd(); ++it)
    {
    numForeGround+= it.Get()>=0;
    }
  cout<<numForeGround<<" foreground voxels"<<endl;

  //----------------------------
  // Read in the stl file
  //----------------------------
  vtkSmartPointer<vtkPolyData> surface;
  surface.TakeReference(ReadPolyData(InputSurface.c_str(),InvertY));

  vtkPoints* points = surface->GetPoints();
  int numPoints = points->GetNumberOfPoints();
  std::vector<Voxel> domainVoxels;
  ComputeDomainVoxels(weight0,points,domainVoxels);
  cout<<points->GetNumberOfPoints()<<" points, "<<domainVoxels.size()<<" voxels"<<endl;

  //----------------------------
  // Read Weights
  //----------------------------
  WeightMap weightMap;
  bender::ReadWeights(fnames,domainVoxels,weightMap);

  //----------------------------
  //Perform interpolation
  //----------------------------
  vtkPointData* pointData = surface->GetPointData();
  pointData->Initialize();
  std::vector<vtkFloatArray*> surfaceVertexWeights;
  for(int i=0; i<numSites; ++i)
    {
    vtkFloatArray* arr = vtkFloatArray::New();
    arr->SetNumberOfTuples(numPoints);
    arr->SetNumberOfComponents(1);
    for(vtkIdType j=0; j<static_cast<vtkIdType>(numPoints); ++j)
      {
      arr->SetValue(j,0.0);
      }

    string name = vtksys::SystemTools::GetFilenameWithoutExtension(fnames[i]);
    arr->SetName(name.c_str());
    pointData->AddArray(arr);
    surfaceVertexWeights.push_back(arr);
    arr->Delete();
    assert(pointData->GetArray(i)->GetNumberOfTuples()==numPoints);
    }

  int numZeros(0);
  WeightMap::WeightVector w_pi(numSites);
  WeightMap::WeightVector w_corner(numSites);
  for(int pi=0; pi<points->GetNumberOfPoints();++pi)
    {
    double xraw[3];
    points->GetPoint(pi,xraw);

    itk::Point<double,3> x(xraw);
    itk::ContinuousIndex<double,3> coord;
    weight0->TransformPhysicalPointToContinuousIndex(x, coord);
    bool res = bender::Lerp<WeightImage>(weightMap,coord,weight0, 0, w_pi);
    if(!res)
      {
      cerr<<"Lerp failed for "<<coord<<endl;
      }
    else
      {
      numZeros+=w_pi.GetNorm()==0;
      for(int i=0; i<numSites;++i)
        {
        surfaceVertexWeights[i]->SetValue(pi, w_pi[i]);
        }
      }
    }
  cerr<<numZeros<<" points have zero weight"<<endl;
  WritePolyData(surface,OutputSurface);

  return EXIT_SUCCESS;
}
