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
#include "benderIOUtils.h"

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
void ComputeDomainVoxels(WeightImage::Pointer image //input
                         ,vtkPoints* points //input
                         ,const std::vector<vtkIdType>& selection
                         ,std::vector<Voxel>& domainVoxels //output
                         )
{
  CubeNeighborhood cubeNeighborhood;
  VoxelOffset* offsets = cubeNeighborhood.Offsets;

  BoolImage::Pointer domain = BoolImage::New();
  domain->SetRegions(image->GetLargestPossibleRegion());
  domain->Allocate();
  domain->FillBuffer(false);

  for(size_t i = 0; i<selection.size(); i++)
    {
    vtkIdType pi = selection[i];
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
      if(domain->GetLargestPossibleRegion().IsInside(q) && !domain->GetPixel(q))
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
  typedef std::vector<vtkIdType> IdArray;

  cout<<"Evaluate weight in  "<<WeightDirectory<<endl;
  cout<<"Evaluating surface: "<<InputSurface<<endl;
  if(!IsSurfaceInRAS)
    {
    cout<<"Invert x,y coordinates\n";
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
  cout<<" origin: "<<weight0->GetOrigin()<<endl;
  cout<<" spacing: "<<weight0->GetSpacing()<<endl;



  int numForeGround(0);
  for(itk::ImageRegionIterator<WeightImage> it(weight0,weightRegion);!it.IsAtEnd(); ++it)
    {
    numForeGround+= it.Get()>=0;
    }
  cout<<numForeGround<<" foreground voxels"<<endl;

  //----------------------------
  // Read in the surface input file
  // , pick only the vertices that are
  // inside the image and store them in sampleVertices
  //----------------------------
  vtkSmartPointer<vtkPolyData> surface;
  surface.TakeReference(bender::IOUtils::ReadPolyData(InputSurface.c_str(),!IsSurfaceInRAS));
  vtkPoints* points = surface->GetPoints();
  IdArray sampleVertices;
  for(vtkIdType pi=0; pi<points->GetNumberOfPoints();++pi)
    {
    itk::Point<double,3> x(points->GetPoint(pi));
    itk::ContinuousIndex<double,3> coord;
    weight0->TransformPhysicalPointToContinuousIndex(x, coord);
    if(weight0->GetLargestPossibleRegion().IsInside(coord))
      {
      sampleVertices.push_back(pi);
      }
    }
  cout<<sampleVertices.size()<<" out of "<<points->GetNumberOfPoints()<<" vertices are in the weight image domain"<<endl;
  std::vector<Voxel> domainVoxels;
  ComputeDomainVoxels(weight0,points,sampleVertices,domainVoxels);
  cout<<domainVoxels.size()<<" voxels in the weight domain"<<endl;

  //----------------------------
  // Read Weights
  //----------------------------
  WeightMap weightMap;
  bender::ReadWeights(fnames,domainVoxels,weightMap);

  //----------------------------
  //Perform interpolation
  //----------------------------
  vtkIdType numPoints = points->GetNumberOfPoints();
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

  for(IdArray::iterator itr=sampleVertices.begin();itr!=sampleVertices.end();itr++)
    {
    vtkIdType pi = *itr;
    double xraw[3];
    points->GetPoint(pi,xraw);

    itk::Point<double,3> x(xraw);
    itk::ContinuousIndex<double,3> coord;
    weight0->TransformPhysicalPointToContinuousIndex(x, coord);

    bool res = bender::Lerp<WeightImage>(weightMap,coord,weight0, 0, w_pi);
    if(!res)
      {
      cerr<<"WARNING: Lerp failed for "<< pi
          << " l:[" << xraw[0] << ", " << xraw[1] << ", " << xraw[2] << "]"
          << " w:" << coord<<endl;
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
