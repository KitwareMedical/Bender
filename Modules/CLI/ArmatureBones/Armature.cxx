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

// ArmatureWeight includes
#include "Armature.h"
#include <benderIOUtils.h>

// ITK includes
#include <itkImage.h>
#include <itkImageFileWriter.h>
#include <itkStatisticsImageFilter.h>
#include <itkBinaryThresholdImageFilter.h>
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
  out->SetOrigin(in->GetOrigin());
  out->SetSpacing(in->GetSpacing());
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
ArmatureType::ArmatureType(LabelImageType::Pointer image) : BodyMap(image)
{
  this->BodyPartition = LabelImageType::New();
  Allocate<LabelImageType,LabelImageType>(image, this->BodyPartition);
  this->BodyPartition->FillBuffer(ArmatureType::BackgroundLabel);

  this->BonesPartition = LabelImageType::New();
  Allocate<LabelImageType,LabelImageType>(image, this->BonesPartition);
  this->BonesPartition->FillBuffer(ArmatureType::BackgroundLabel);
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
bool ArmatureType::Init(vtkPolyData* armaturePolyData)
{
  if (!armaturePolyData)
    {
    std::cerr << "No armature"<< std::endl;
    return false;
    }

  if (! this->InitSkeleton(armaturePolyData))
    {
    std::cerr<< "Could not find the body partition."
      << " Stopping before segmenting the bones"<<std::endl;
    return false;
    }

  return this->InitBones();
}

//-------------------------------------------------------------------------------
LabelImageType::Pointer ArmatureType::GetBodyPartition()
{
  return this->BodyPartition;
}

//-------------------------------------------------------------------------------
LabelImageType::Pointer ArmatureType::GetBonesPartition()
{
  return this->BonesPartition;
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
      std::cerr << "This probably means that the armature doesn't fit"
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

//-------------------------------------------------------------------------------
bool ArmatureType::InitBones()
{
  bool success = true;

  RegionType imDomain = this->BodyMap->GetLargestPossibleRegion();
  Neighborhood<3> neighbors;
  const VoxelOffsetType* offsets = neighbors.Offsets;

  // Select the bones and label them by componnets
  // \todo not needed if the threshold is done manually when boneInside is used
  typedef itk::BinaryThresholdImageFilter<LabelImageType, CharImageType> Threshold;
  Threshold::Pointer threshold = Threshold::New();
  threshold->SetInput(this->BodyMap);
  threshold->SetLowerThreshold(209); //bone marrow
  threshold->SetInsideValue(ArmatureType::DomainLabel);
  threshold->SetOutsideValue(ArmatureType::BackgroundLabel);
  threshold->Update();
  CharImageType::Pointer boneInside = threshold->GetOutput();

  // Partition the bones by armature edges
  // Two goals:
  // no-split: Each natural bone should be assigned one label
  // split-joined: If a set of natural bones are connected in the voxel
  //  space, we would like to partition them.
  //
  if (1) //simple and stupid
    {
    this->BonesPartition->FillBuffer(ArmatureType::BackgroundLabel);
    itk::ImageRegionIteratorWithIndex<LabelImageType> boneIter(this->BonesPartition, imDomain);

    for (boneIter.GoToBegin(); !boneIter.IsAtEnd(); ++boneIter)
      {
      /// \todo GetIndex/GetPixel is expensive, use iterator instead
      VoxelType voxel = boneIter.GetIndex();
      if (boneInside->GetPixel(voxel) != ArmatureType::BackgroundLabel)
        {
        CharType edgeLabel = this->BodyPartition->GetPixel(voxel);
        boneIter.Set(edgeLabel);
        }
      }
    }
  else //satisfy only the first
    {
    typedef itk::ConnectedComponentImageFilter<CharImageType, LabelImageType>  ConnectedComponent;
    ConnectedComponent::Pointer connectedComponents = ConnectedComponent::New ();
    connectedComponents->SetInput(threshold->GetOutput());
    connectedComponents->SetBackgroundValue(ArmatureType::BackgroundLabel);
    connectedComponents->Update();
    LabelImageType::Pointer boneComponents =  connectedComponents->GetOutput();

    const int numBones = connectedComponents->GetObjectCount();

    //now relabel the bones by the skeleton part they belong to
    typedef itk::Image<bool,3> MarkImage;
    MarkImage::Pointer visited = MarkImage::New();
    Allocate<LabelImageType,MarkImage>(this->BodyMap,visited);
    VoxelType invalidVoxel;
    invalidVoxel[0]=-1;
    invalidVoxel[1]=-1;
    invalidVoxel[2]=-1;

    std::vector<VoxelType> boneSeeds(numBones,invalidVoxel);
    itk::ImageRegionIteratorWithIndex<LabelImageType> it(
      boneComponents,boneComponents->GetLargestPossibleRegion());
    for (it.GoToBegin(); !it.IsAtEnd(); ++it)
      {
      // i is the component id of the bone, with 0 being the background
      size_t i = static_cast<size_t>(it.Get());
      if(i>0)
        {
        assert(i>=1 && i<=boneSeeds.size());
        boneSeeds[i-1] = it.GetIndex();
        }
      }

    // verify that seeds are valid
    for (std::vector<VoxelType>::iterator i=boneSeeds.begin();
      i!=boneSeeds.end(); ++i)
      {
      assert((*i) != invalidVoxel);
      }

    // compute a map from the old to the new labels:
    //  newLabels[oldLabel] is the new label
    std::vector<LabelType> newLabels(numBones+1,0);
    for (std::vector<VoxelType>::iterator seedIter=boneSeeds.begin();
      seedIter!=boneSeeds.end();++seedIter)
      {
      VoxelType seed = *seedIter;
      LabelType seedLabel = boneComponents->GetPixel(seed);

      //count the # of voxels of the bone that belongs to each armature edge:
      // regionSize[i] gives the # of bone voxels that belong to armature edge i
      std::vector<int> regionSize(this->GetMaxEdgeLabel()+1,0);
      std::vector<VoxelType> bd;
      bd.push_back(seed);
      int numVisited(1);
      while(!bd.empty())
        {
        VoxelType p = bd.back();  bd.pop_back();
        ++numVisited;
        CharType edgeLabel = this->BodyPartition->GetPixel(p);
        regionSize[static_cast<size_t>(edgeLabel)]++;
        visited->SetPixel(p,true);
        for(int i=0; i<6; ++i)
          {
          VoxelType q = p + offsets[i];
          if( imDomain.IsInside(q) &&
              !visited->GetPixel(q) &&
              boneComponents->GetPixel(q)==seedLabel)
            {
            bd.push_back(q);
            }
          }
        }

      LabelType newLabel=0;
      int maxSize(0);
      bool printLabels(false);
      for(size_t i=0; i<regionSize.size();++i)
        {
        if(regionSize[6]>0)
          {
          printLabels = true;
          }
        if(regionSize[i]>maxSize)
          {
          maxSize = regionSize[i];
          newLabel = static_cast<LabelType>(i);
          }
        }
      newLabels[seedLabel] = newLabel;
      if(printLabels)
        {
        std::cout << "Visited: "<<numVisited << std::endl;
        std::cout << "Edges for bone: "<<seedLabel<<" ";
        for(size_t i=0; i<regionSize.size();++i)
          {
          if(regionSize[i]!=0)
            {
            std::cout << i<<" ";
            }
          }
        std::cout << endl;
        }

      }
    // Relabel the image
    itk::ImageRegionIterator<LabelImageType> boneComponentIter(
      boneComponents,boneComponents->GetLargestPossibleRegion());
    for(boneComponentIter.GoToBegin();
      !boneComponentIter.IsAtEnd(); ++boneComponentIter)
      {
      LabelType oldLabel = boneComponentIter.Get();
      LabelType newLabel = newLabels[static_cast<size_t>(oldLabel)];
      boneComponentIter.Set(newLabel);
      }

    for(EdgeType i=0; i<this->GetNumberOfEdges(); ++i)
      {
      LabelType edgeLabel = static_cast<LabelType>(this->GetEdgeLabel(i));
      if(std::find(newLabels.begin(), newLabels.end(),edgeLabel)
        == newLabels.end())
        {
        std::cout << "No bones belong to edge "
          <<i<<" with label "<<edgeLabel << std::endl;
        }
      }
    this->BonesPartition = boneComponents;
    }

  // for debugging
  if (this->Debug)
    {
    itk::ImageRegionIteratorWithIndex<LabelImageType> boneIter(
      this->BonesPartition,imDomain);

    std::vector<int> componentSize(this->GetMaxEdgeLabel()+1,0);
    for ( boneIter.GoToBegin(); !boneIter.IsAtEnd(); ++boneIter )
      {
      assert(boneIter.Get()<componentSize.size());
      componentSize[boneIter.Get()]++;
      }
    int totalSize=0;
    for ( size_t i = 0; i < componentSize.size(); ++i)
      {
      totalSize+=componentSize[i];
      std::cout << i<<": "<<componentSize[i]<<"\n";
      }
    }

  return success;
}

