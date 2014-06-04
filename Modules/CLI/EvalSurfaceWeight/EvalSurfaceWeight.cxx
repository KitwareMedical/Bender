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

#include "EvalSurfaceWeightCLP.h"

//------- Bender-----------
#include "benderIOUtils.h"
#include "benderWeightMap.h"
#include "benderWeightMapMath.h"
#include "benderWeightMapIO.h"

//--------ITK --------------
#include <itkContinuousIndex.h>
#include <itkDirectory.h>
#include <itkImage.h>
#include <itkImageFileWriter.h>
#include <itkImageRegionIteratorWithIndex.h>
#include <itkIndex.h>
#include <itkMath.h>
#include <itkMatrix.h>
#include <itkPluginUtilities.h>
#include <itkVariableLengthVector.h>

//--------VTK --------------
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkFloatArray.h>
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkPolyDataWriter.h>
#include <vtkSmartPointer.h>
#include <vtksys/SystemTools.hxx>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>

//--------standard-------------
#include <iostream>
#include <limits>
#include <math.h>
#include <sstream>
#include <vector>

typedef itk::Image<float, 3>  WeightImage;
typedef itk::Image<bool, 3>  BoolImage;

typedef itk::Index<3> Voxel;
typedef itk::Offset<3> VoxelOffset;
typedef itk::ImageRegion<3> Region;

//-------------------------------------------------------------------------------
template<class T>
void PrintVector(T* a, int n)
{
  std::cout<<"[";
  for(int i=0; i<n; ++i)
    {
    std::cout<<a[i]<<(i==n-1?"": ", ");
    }
  std::cout<<"]"<<std::endl;
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

// This function provides a more robust way of getting the voxel coordinates
// by returning the nearest neighbor coordinates if the given point is outside
// the weight map (Note outside the image itself, not the weight region).
// This is useful beacause surface sometimes lay outside of the weight images
// but we still want to attribute weight to these pixel.
//-------------------------------------------------------------------------------
bool PhysicalPointToContinuousIndex(WeightImage::Pointer weight,
                                    itk::Point<double,3>&x,
                                    itk::ContinuousIndex<double,3>& coord)
{
  if(weight->TransformPhysicalPointToContinuousIndex(x, coord))
    {
    return true;
    }
  else
    {
    WeightImage::RegionType region = weight->GetLargestPossibleRegion();

    for (int i = 0; i < 3; ++i)
      {
      coord[i] =
        std::min<double>(
          std::max<double>(coord[i], region.GetIndex(i)),
          region.GetSize(i) - 1.0);
      }
    return region.IsInside(coord);
    }
}

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
  domain->CopyInformation(image);

  WeightImage::RegionType region = image->GetLargestPossibleRegion();
  domain->SetRegions(region);
  domain->Allocate();
  domain->FillBuffer(false);

  for(size_t i = 0; i<selection.size(); i++)
    {
    vtkIdType pi = selection[i];
    double xraw[3];
    points->GetPoint(pi,xraw);

    itk::Point<double,3> x(xraw);
    itk::ContinuousIndex<double,3> coord;
    PhysicalPointToContinuousIndex(image, x, coord);

    Voxel p;
    p.CopyWithCast(coord);

    for(int iOff=0; iOff<8; ++iOff)
      {
      Voxel q = p + offsets[iOff];

      if(region.IsInside(q) && !domain->GetPixel(q))
        {
        domain->SetPixel(q,true);
        domainVoxels.push_back(q);
        }
      }
    }
}

//-------------------------------------------------------------------------------
int main( int argc, char * argv[] )
{
  PARSE_ARGS;
  typedef bender::WeightMap WeightMap;
  typedef std::vector<vtkIdType> IdArray;

  if (Debug)
    {
    std::cout<<"Evaluate weight in  "<<WeightDirectory<<std::endl;
    std::cout<<"Evaluating surface: "<<InputSurface<<std::endl;
    if(!IsSurfaceInRAS)
      {
      std::cout<<"Invert x,y coordinates\n";
      }
    std::cout<<"Output to "<<OutputSurface<<std::endl;
    }

  //----------------------------
  // Read armature
  //----------------------------
  // Armature is optional.
  vtkSmartPointer<vtkPolyData> armature;
  if (!ArmaturePoly.empty())
    {
    armature.TakeReference(
      bender::IOUtils::ReadPolyData(ArmaturePoly.c_str(),!IsArmatureInRAS));
    }

  //----------------------------
  // Read the first weight image
  // and all file names
  //----------------------------
  std::vector<std::string> fnames;
  bender::GetWeightFileNames(WeightDirectory, fnames);
  int numSites = fnames.size();
  if(numSites<1)
    {
    std::cerr<<"No weight file is found."<<std::endl;
    return 1;
    }

  typedef itk::ImageFileReader<WeightImage>  ReaderType;
  ReaderType::Pointer reader = ReaderType::New();
  reader->SetFileName(fnames[0].c_str());
  reader->Update();

  WeightImage::Pointer weight0 = reader->GetOutput();
  Region weightRegion = weight0->GetLargestPossibleRegion();

  if (Debug)
    {
    std::cout<<"Weight volume description: "<<std::endl;
    std::cout<<weightRegion<<std::endl;
    std::cout<<" origin: "<<weight0->GetOrigin()<<std::endl;
    std::cout<<" spacing: "<<weight0->GetSpacing()<<std::endl;


    int numForeGround(0);
    for(itk::ImageRegionIterator<WeightImage> it(weight0,weightRegion);
      !it.IsAtEnd(); ++it)
      {
      numForeGround+= it.Get()>=0;
      }
    std::cout<<numForeGround<<" foreground voxels"<<std::endl;
    }

  //----------------------------
  // Read in the surface input file
  // , pick only the vertices that are
  // inside the image and store them in sampleVertices
  //----------------------------
  vtkSmartPointer<vtkPolyData> surface;
  surface.TakeReference(
    bender::IOUtils::ReadPolyData(InputSurface.c_str(), !IsSurfaceInRAS));

  // Create output surface just like the input surface
  vtkSmartPointer<vtkPolyData> outputSurface =
    vtkSmartPointer<vtkPolyData>::NewInstance(surface);
  outputSurface->DeepCopy(surface);

  vtkPoints* points = outputSurface->GetPoints();
  IdArray sampleVertices;
  for(vtkIdType pi=0; pi<points->GetNumberOfPoints();++pi)
    {
    itk::Point<double,3> x(points->GetPoint(pi));
    itk::ContinuousIndex<double,3> coord;

    if(PhysicalPointToContinuousIndex(weight0, x, coord))
      {
      sampleVertices.push_back(pi);
      }
    else
      {
      return EXIT_FAILURE;
      }
    }

  if (sampleVertices.size() <= 0
      || sampleVertices.size() != static_cast<size_t>(points->GetNumberOfPoints()))
    {
    std::cout<<"FAILURE: "<<sampleVertices.size()<<" out of "
      <<points->GetNumberOfPoints()
      <<" vertices are in the weight image domain."<<std::endl;
    return EXIT_FAILURE;
    }


  std::vector<Voxel> domainVoxels;
  ComputeDomainVoxels(weight0,points,sampleVertices,domainVoxels);

  if (Debug)
    {
    std::cout<<domainVoxels.size()<<" voxels in the weight domain"<<std::endl;
    }

  //----------------------------
  // Read Weights
  //----------------------------
  WeightMap weightMap;
  bender::ReadWeights(fnames, domainVoxels, weightMap);
  weightMap.SetMaskImage(weight0, 0.);
  vtkIdTypeArray* filiation = vtkIdTypeArray::SafeDownCast(
    armature.GetPointer() ? armature->GetCellData()->GetArray("Parenthood") : 0);
  if (filiation)
    {
    weightMap.SetWeightsFiliation(filiation, MaximumParenthoodDistance);
    if (Debug)
      {
      std::cout << "No more than " << MaximumParenthoodDistance
                << " degrees of separation" << std::endl;
      }
    }

  //----------------------------
  //Perform interpolation
  //----------------------------

  // Create the field arrays
  vtkIdType numPoints = points->GetNumberOfPoints();
  vtkPointData* pointData = outputSurface->GetPointData();
  pointData->Initialize();

  std::vector<vtkFloatArray*> outputSurfaceVertexWeights;
  for(int i=0; i<numSites; ++i)
    {
    vtkFloatArray* arr = vtkFloatArray::New();
    arr->SetNumberOfTuples(numPoints);
    arr->SetNumberOfComponents(1);
    for(vtkIdType j=0; j<static_cast<vtkIdType>(numPoints); ++j)
      {
      arr->SetValue(j,0.0);
      }

    std::string name =
      vtksys::SystemTools::GetFilenameWithoutExtension(fnames[i]);
    arr->SetName(name.c_str());
    pointData->AddArray(arr);
    outputSurfaceVertexWeights.push_back(arr);
    arr->Delete();
    assert(pointData->GetArray(i)->GetNumberOfTuples()==numPoints);
    }

  // Insert weights
  int numZeros = 0;
  WeightMap::WeightVector w_pi(numSites);
  WeightMap::WeightVector w_corner(numSites);

  for(IdArray::iterator itr=sampleVertices.begin();itr!=sampleVertices.end();itr++)
    {
    vtkIdType pi = *itr;
    double xraw[3];
    points->GetPoint(pi,xraw);

    itk::Point<double,3> x(xraw);
    itk::ContinuousIndex<double,3> coord;
    PhysicalPointToContinuousIndex(weight0, x, coord);

    bool res = weightMap.Lerp(coord, w_pi);
    if(!res)
      {
      std::cerr<<"WARNING: Lerp failed for "<< pi
          << " l:[" << xraw[0] << ", " << xraw[1] << ", " << xraw[2] << "]"
          << " w:" << coord<<std::endl;
      }
    else
      {
      numZeros+=w_pi.GetNorm()==0;
      for(int i=0; i<numSites;++i)
        {
        outputSurfaceVertexWeights[i]->SetValue(pi, w_pi[i]);
        }
      }
    }
  if (Debug)
    {
    std::cout<<numZeros<<" points have zero weight"<<std::endl;
    }

  if (!IsSurfaceInRAS)
    {
    vtkSmartPointer<vtkTransform> transform =
      vtkSmartPointer<vtkTransform>::New();
    transform->RotateZ(180.0);

    vtkSmartPointer<vtkTransformPolyDataFilter> transformer =
      vtkSmartPointer<vtkTransformPolyDataFilter>::New();
    transformer->SetInput(outputSurface);
    transformer->SetTransform(transform);
    transformer->Update();

    bender::IOUtils::WritePolyData(transformer->GetOutput(), OutputSurface);
    }
  else
    {
    bender::IOUtils::WritePolyData(outputSurface, OutputSurface);
    }

  return EXIT_SUCCESS;
}
