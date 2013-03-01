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

  This file was originally developed by Yuanxin Liu, Kitware Inc.

=========================================================================*/

// VolumeSkinning includes
#include "Armature.h"
#include "benderIOUtils.h"

// ITK includes
#include <itkImage.h>
#include <itkImageFileWriter.h>
#include <itkStatisticsImageFilter.h>
#include <itkImageRegionIteratorWithIndex.h>
#include <itkBresenhamLine.h>
#include <itkMath.h>
#include <itkIndex.h>
#include <itkConnectedComponentImageFilter.h>

// VTK includes
#include <vtkNew.h>
#include <vtkPolyDataReader.h>
#include <vtkPolyDataWriter.h>
#include <vtkPolyData.h>
#include <vtkCellArray.h>
#include <vtkTimerLog.h>

// STD includes
#include <iostream>
#include <iomanip>
#include <limits>
#include <vector>


namespace
{
//-------------------------------------------------------------------------------
template<class InImage, class OutImage>
void Allocate(typename InImage::Pointer in, typename OutImage::Pointer out)
{
  out->CopyInformation(in);
  out->SetRegions(in->GetLargestPossibleRegion());
  out->Allocate();
}

//-------------------------------------------------------------------------------
template <class ImageType>
void Rasterize(const double* a, const double* b,
               const typename ImageType::Pointer image,
               std::vector<typename ImageType::IndexType>& outputPixels)
{
  outputPixels.clear();

  typedef itk::Index<ImageType::ImageDimension> Pixel;
  typedef typename itk::Point<double, ImageType::ImageDimension> PointType;
  PointType pa(a);
  PointType pb(b);
  const unsigned int dimension = ImageType::ImageDimension;
  Pixel Ia,Ib;
  if(!image->TransformPhysicalPointToIndex(pa,Ia))
    {
    cerr<<"Fail to rasterize pa " << pa << "\n";
    cerr<<"  Image origin: " << image->GetOrigin() << "\n";
    cerr<<"  Image spacing: " << image->GetSpacing() << "\n";
    cerr<<"  Image region: " << image->GetLargestPossibleRegion() << "\n";
    cerr<< "You might need to convert coordinate system.\n";
    return;
    }
  if(!image->TransformPhysicalPointToIndex(pb,Ib))
    {
    cerr<<"Fail to rasterize pb " << pb << "\n";
    cerr<<"  Image origin: " << image->GetOrigin() << "\n";
    cerr<<"  Image spacing: " << image->GetSpacing() << "\n";
    cerr<<"  Image region: " << image->GetLargestPossibleRegion() << "\n";
    cerr<< "You might need to convert coordinate system.\n";
    return;
    }

  // TODO: Simplify with ITKv4 new signature for itk::BresenhamLine::BuildLine.
  typedef itk::BresenhamLine<ImageType::ImageDimension> Bresenham;
  typedef typename itk::BresenhamLine<ImageType::ImageDimension>::LType DirType;
  Bresenham bresLine;
  DirType Idir;
  DirType Pdir;
  unsigned int maxSteps(0);
  for(unsigned int i=0; i<dimension; ++i)
    {
    Idir[i]=Ib[i]-Ia[i];
    Pdir[i]=b[i]-a[i];
    maxSteps+= static_cast<int>(fabs(Idir[i]));
    }

  float len = Pdir.GetNorm();
  typename Bresenham::OffsetArray line = bresLine.BuildLine(Idir,maxSteps);
  for(size_t i=0; i<line.size();++i)
    {
    Pixel pIndex = Ia+line[i];
    PointType p;
    image->TransformIndexToPhysicalPoint(pIndex,p);
    if(p.EuclideanDistanceTo(pa)> len)
      {
      break;
      }
    assert(image->GetLargestPossibleRegion().IsInside(pIndex));
    outputPixels.push_back(pIndex);
    }
}

//-------------------------------------------------------------------------------
template<class InputImageType>
void ComputeManhattanVoronoi(typename InputImageType::Pointer siteMap,
                             typename InputImageType::PixelType background,
                             typename InputImageType::PixelType unknown,
                             int maxDist=INT_MAX)
{
  typedef typename InputImageType::PixelType PixelValue;
  typedef typename InputImageType::IndexType Pixel;
  typedef typename InputImageType::OffsetType PixelOffset;

  typedef std::vector<Pixel> PixelVector;
  typename InputImageType::RegionType allRegion = siteMap->GetLargestPossibleRegion();
  itk::ImageRegionIteratorWithIndex<InputImageType> it(siteMap,allRegion);

  Neighborhood<InputImageType::ImageDimension> neighbors;
  PixelOffset* offsets = neighbors.Offsets ;

  PixelVector bd;
  for(it.GoToBegin(); !it.IsAtEnd(); ++it)
    {
    PixelValue value = it.Get();
    if(value!=background && value!=unknown)
      {
      bd.push_back(it.GetIndex());
      }
    }
  int dist=1;
  while(!bd.empty())
    {
    dist++;
    if(dist>maxDist)
      {
      break;
      }
    PixelVector newBd;
    for(typename PixelVector::iterator i = bd.begin(); i!=bd.end();++i)
      {
      PixelValue siteLabel = siteMap->GetPixel(*i);
      Pixel pIndex = *i;
      for(int iOff=0; iOff<2*InputImageType::ImageDimension; ++iOff)
        {
        Pixel qIndex = pIndex + offsets[iOff];
        if( allRegion.IsInside(qIndex) && siteMap->GetPixel(qIndex)==unknown)
          {
          newBd.push_back(qIndex);
          siteMap->SetPixel(qIndex,siteLabel);
          }
        }
      }
    bd = newBd;
    }
}

} // end namespace

//-------------------------------------------------------------------------------
ArmatureType::ArmatureType(LabelImageType::Pointer image)
{
  this->BodyMap = image;

  this->BodyPartition = LabelImageType::New();
  Allocate<LabelImageType,LabelImageType>(image, this->BodyPartition);
  this->BodyPartition->FillBuffer(ArmatureType::BackgroundLabel);
}

//-------------------------------------------------------------------------------
CharType ArmatureType::GetEdgeLabel(EdgeType i) const
{
  // 0 is background, 1 is body interior so armature must start with 2
  return static_cast<CharType>(i + ArmatureType::EdgeLabels);
}

//-------------------------------------------------------------------------------
CharType ArmatureType::GetMaxEdgeLabel() const
{
  assert(this->GetNumberOfEdges() > 0 &&
         this->GetNumberOfEdges() <= std::numeric_limits<CharType>::max());
  //return the last edge id
  EdgeType lastEdge = static_cast<EdgeType>(this->GetNumberOfEdges() - 1);
  return static_cast<CharType>( this->GetEdgeLabel(lastEdge) );
}

//-------------------------------------------------------------------------------
size_t ArmatureType::GetNumberOfEdges() const
{
  return this->SkeletonVoxels.size();
}

//-------------------------------------------------------------------------------
void ArmatureType::SetDebug(bool debug)
{
  this->Debug = debug;
}

//-------------------------------------------------------------------------------
bool ArmatureType::GetDebug()const
{
  return this->Debug;
}

//-------------------------------------------------------------------------------
LabelImageType::Pointer ArmatureType::GetBodyPartition()
{
  return this->BodyPartition;
}

//-------------------------------------------------------------------------------
bool ArmatureType::InitSkeleton(vtkPolyData* armaturePolyData)
{
  bool success = true;

  vtkCellArray* armatureSegments = armaturePolyData->GetLines();
  this->SkeletonVoxels.reserve(armatureSegments->GetNumberOfCells());

  //iterate over the edges of the armature and rasterize them
  vtkNew<vtkIdList> cell;
  armatureSegments->InitTraversal();
  EdgeType edgeId=0;
  while ( armatureSegments->GetNextCell(cell.GetPointer()) )
    {
    assert(cell->GetNumberOfIds() == 2);
    vtkIdType a = cell->GetId(0);
    vtkIdType b = cell->GetId(1);

    double ax[3], bx[3];
    armaturePolyData->GetPoints()->GetPoint(a, ax);
    armaturePolyData->GetPoints()->GetPoint(b, bx);

    this->SkeletonVoxels.push_back(std::vector<VoxelType>());
    std::vector<VoxelType>& edgeVoxels(this->SkeletonVoxels.back());
    // Fill up edgeVoxels with all the voxels from a to b.
    Rasterize<LabelImageType>(ax,bx,this->BodyPartition, edgeVoxels);
    if (edgeVoxels.size() == 0)
      {
      /// \tbd is it an error ?
      std::cerr << "Can't rasterize segment " << edgeId << std::endl;
      /// \tbd should edgeId be incremented ?
      success = false;
      continue;
      }
    // XXX we really need to make sure that the rasterized edge is a connected component
    // just a hack here
    if (edgeVoxels.size() > 2)
      {
      // Discard points a and b
      edgeVoxels.erase(edgeVoxels.begin());
      edgeVoxels.erase(edgeVoxels.begin() + edgeVoxels.size() - 1);
      }
    const CharType label = ArmatureType::GetEdgeLabel(edgeId);
    size_t numOutside(0);
    for (std::vector<VoxelType>::iterator vi = edgeVoxels.begin();
         vi != edgeVoxels.end(); ++vi)
      {
      if (this->BodyMap->GetPixel(*vi) != ArmatureType::BackgroundLabel)
        {
        /// \tbd what to do when the pixel has already a label associated.
        if (this->BodyPartition->GetPixel(*vi) == ArmatureType::BackgroundLabel)
          {
          this->BodyPartition->SetPixel(*vi, label);
          }
        }
      else
        {
        ++numOutside;
        }
      }
    if ( numOutside > 0 )
      {
      std::cerr << "WARNING: armature edge "<< edgeId <<" has "
                << numOutside <<" outside voxels out of "<< edgeVoxels.size()
                << std::endl;
      std::cerr << "This probably means that the armature doesn't fit "
                << "perfectly inside the body labelmap." << std::endl;
      success = false;
      }

    if(this->SkeletonVoxels.back().size() < 2)
      {
      std::cerr << "WARNING: edge " << edgeId <<" is very small. "
                << "It is made of less than 2 voxels."<< std::endl;
      success = false;
      }
    ++edgeId;
    }

  // Compute the voronoi of the skeleton
  // Step 1: color the non-skeleton body voxels by value unkwn
  LabelType unknown = ArmatureType::DomainLabel;
  itk::ImageRegionIteratorWithIndex<LabelImageType> it(
    this->BodyMap, this->BodyMap->GetLargestPossibleRegion());
  for (it.GoToBegin(); !it.IsAtEnd(); ++it)
    {
    if (it.Get() != ArmatureType::BackgroundLabel)
      {
      /// \todo: GetIndex is expensive, use instead an iterator for BodyPartition
      VoxelType voxel = it.GetIndex();
      LabelType label = this->BodyPartition->GetPixel(voxel);
      if (label == ArmatureType::BackgroundLabel)
        {
        this->BodyPartition->SetPixel(voxel, unknown);
        }
      }
    }

  if (this->Debug)
    {
    bender::IOUtils::WriteImage<LabelImageType>(this->BodyPartition,
      "./DEBUG_bodybinary.mha");
    }

  ComputeManhattanVoronoi<LabelImageType>(
    this->BodyPartition, ArmatureType::BackgroundLabel, unknown);

  return success;
}
