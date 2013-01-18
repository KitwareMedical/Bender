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
#include "HeatDiffusionProblem.h"
#include "SolveHeatDiffusionProblem.h"
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

typedef float WeightImagePixel;

typedef itk::Image<unsigned short, 3>  LabelImage;
typedef itk::Image<unsigned char, 3>  CharImage;

typedef itk::Image<WeightImagePixel, 3>  WeightImage;

typedef itk::Index<3> Voxel;
typedef itk::Offset<3> VoxelOffset;
typedef itk::ImageRegion<3> Region;

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
void Rasterize(const double* a, const double* b, const typename ImageType::Pointer image,
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

//-------------------------------------------------------------------------------
ArmatureType::ArmatureType(LabelImage::Pointer image)
  : BodyMap(image)
{
  this->BodyPartition = LabelImage::New();
  Allocate<LabelImage,LabelImage>(image, this->BodyPartition);
  this->BodyPartition->FillBuffer(0);
}

//-------------------------------------------------------------------------------
CharType ArmatureType::GetEdgeLabel(EdgeType i)
{
  // 0 is background, 1 is body interior so armature must start with 2
  return static_cast<CharType>(i+EdgeLabels);
}

//-------------------------------------------------------------------------------
CharType ArmatureType::GetMaxEdgeLabel() const
{
  assert(this->GetNumberOfEdges() > 0 &&
         this->GetNumberOfEdges() <= std::numeric_limits<CharType>::max());
  //return the last edge id
  EdgeType lastEdge = static_cast<EdgeType>(this->GetNumberOfEdges() - 1);
  return static_cast<CharType>(ArmatureType::GetEdgeLabel(lastEdge));
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
void ArmatureType::SetDumpDebugImages(bool dump)
{
  this->DumpDebugImages = dump;
}

//-------------------------------------------------------------------------------
bool ArmatureType::GetDumpDebugImages()const
{
  return this->DumpDebugImages;
}

//-------------------------------------------------------------------------------
void ArmatureType::Init(const char* fname, bool invertXY)
{
  vtkPolyData* armaturePolyData = bender::IOUtils::ReadPolyData(fname, invertXY);
  if (!armaturePolyData)
    {
    std::cerr << "Can't read armature " << fname << std::endl;
    return;
    }
  this->InitSkeleton(armaturePolyData);
  armaturePolyData->Delete();

  this->InitBones();
}

//-------------------------------------------------------------------------------
void ArmatureType::InitSkeleton(vtkPolyData* armPoly)
{
  vtkCellArray* armatureSegments = armPoly->GetLines();
  this->SkeletonVoxels.reserve(armatureSegments->GetNumberOfCells());

  //iterate over the edges of the armature and rasterize them
  vtkNew<vtkIdList> cell;
  armatureSegments->InitTraversal();
  EdgeType edgeId=0;
  while ( armatureSegments->GetNextCell(cell.GetPointer()) )
    {
    assert(cell->GetNumberOfIds()==2);
    vtkIdType a = cell->GetId(0);
    vtkIdType b = cell->GetId(1);

    double ax[3], bx[3];
    armPoly->GetPoints()->GetPoint(a, ax);
    armPoly->GetPoints()->GetPoint(b, bx);

    this->SkeletonVoxels.push_back(std::vector<Voxel>());
    std::vector<Voxel>& edgeVoxels(this->SkeletonVoxels.back());
    // Fill up edgeVoxels with all the voxels from a to b.
    Rasterize<LabelImage>(ax,bx,this->BodyPartition, edgeVoxels);
    if (edgeVoxels.size() == 0)
      {
      /// \tbd is it an error ?
      std::cerr << "Can't rasterize segment " << edgeId << std::endl;
      /// \tbd should edgeId be incremented ?
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
    for (std::vector<Voxel>::iterator vi = edgeVoxels.begin();
         vi != edgeVoxels.end(); ++vi)
      {
      if (this->BodyMap->GetPixel(*vi) != BackgroundLabel)
        {
        /// \tbd what to do when the pixel has already a label associated.
        if (this->BodyPartition->GetPixel(*vi) == BackgroundLabel)
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
      }

    if(this->SkeletonVoxels.back().size() < 2)
      {
      std::cerr << "WARNING: edge " << edgeId <<" is very small. "
                << "It is made of less than 2 voxels."<< std::endl;
      }
    ++edgeId;
    }

  // Compute the voronoi of the skeleton
  // Step 1: color the non-skeleton body voxels by value unkwn
  LabelType unknown = ArmatureType::DomainLabel;
  itk::ImageRegionIteratorWithIndex<LabelImage> it(
    this->BodyMap, this->BodyMap->GetLargestPossibleRegion());
  for (it.GoToBegin(); !it.IsAtEnd(); ++it)
    {
    if ( it.Get() != BackgroundLabel )
      {
      /// \todo: GetIndex is expensive, use instead an iterator for BodyPartition
      Voxel voxel = it.GetIndex();
      LabelType label = this->BodyPartition->GetPixel(voxel);
      if (label == BackgroundLabel)
        {
        this->BodyPartition->SetPixel(voxel, unknown);
        }
      }
    }

  if (this->GetDumpDebugImages())
    {
    bender::IOUtils::WriteImage<LabelImage>(this->BodyPartition,"./bodybinary.mha");
    }

  //Step 2:
  ComputeManhattanVoronoi<LabelImage>(this->BodyPartition, BackgroundLabel, unknown);
  if (this->GetDumpDebugImages())
    {
    bender::IOUtils::WriteImage<LabelImage>(this->BodyPartition,"./bodypartition.mha");
    }
}

//-------------------------------------------------------------------------------
void ArmatureType::InitBones()
{
  Region imDomain = this->BodyMap->GetLargestPossibleRegion();
  Neighborhood<3> neighbors;
  const VoxelOffset* offsets = neighbors.Offsets;

  // Select the bones and label them by componnets
  // \todo not needed if the threshold is done manually when boneInside is used
  typedef itk::BinaryThresholdImageFilter<LabelImage, CharImage> Threshold;
  Threshold::Pointer threshold = Threshold::New();
  threshold->SetInput(this->BodyMap);
  threshold->SetLowerThreshold(209); //bone marrow
  threshold->SetInsideValue(DomainLabel);
  threshold->SetOutsideValue(BackgroundLabel);
  threshold->Update();
  CharImage::Pointer boneInside = threshold->GetOutput();

  // Partition the bones by armature edges
  // Two goals:
  // no-split: Each natural bone should be assigned one label
  // split-joined: If a set of natural bones are connected in the voxel
  //  space, we would like to partition them.
  //
  if (1) //simple and stupid
    {
    this->BonePartition = LabelImage::New();
    Allocate<LabelImage,LabelImage>(this->BodyMap, this->BonePartition);
    // \todo not needed if all the pixels are browse later on
    this->BonePartition->FillBuffer(BackgroundLabel);
    itk::ImageRegionIteratorWithIndex<LabelImage> boneIter(this->BonePartition, imDomain);

    for (boneIter.GoToBegin(); !boneIter.IsAtEnd(); ++boneIter)
      {
      /// \todo GetIndex/GetPixel is expensive, use iterator instead
      Voxel voxel = boneIter.GetIndex();
      if (boneInside->GetPixel(voxel) != BackgroundLabel)
        {
        CharType edgeLabel = this->BodyPartition->GetPixel(voxel);
        boneIter.Set(edgeLabel);
        }
      }
    }
  else //satisfy only the first
    {
    typedef itk::ConnectedComponentImageFilter<CharImage, LabelImage>  ConnectedComponent;
    ConnectedComponent::Pointer connectedComponents = ConnectedComponent::New ();
    connectedComponents->SetInput(threshold->GetOutput());
    connectedComponents->SetBackgroundValue(BackgroundLabel);
    connectedComponents->Update();
    LabelImage::Pointer boneComponents =  connectedComponents->GetOutput();

    const int numBones = connectedComponents->GetObjectCount();

    //now relabel the bones by the skeleton part they belong to
    typedef itk::Image<bool,3> MarkImage;
    MarkImage::Pointer visited = MarkImage::New();
    Allocate<LabelImage,MarkImage>(this->BodyMap,visited);
    Voxel invalidVoxel;
    invalidVoxel[0]=-1;
    invalidVoxel[1]=-1;
    invalidVoxel[2]=-1;

    std::vector<Voxel> boneSeeds(numBones,invalidVoxel);
    itk::ImageRegionIteratorWithIndex<LabelImage> it(boneComponents,boneComponents->GetLargestPossibleRegion());
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
    for (std::vector<Voxel>::iterator i=boneSeeds.begin();i!=boneSeeds.end(); ++i)
      {
      assert((*i) != invalidVoxel);
      }

    // compute a map from the old to the new labels:
    //  newLabels[oldLabel] is the new label
    std::vector<LabelType> newLabels(numBones+1,0);
    for (std::vector<Voxel>::iterator seedIter=boneSeeds.begin();seedIter!=boneSeeds.end();++seedIter)
      {
      Voxel seed = *seedIter;
      LabelType seedLabel = boneComponents->GetPixel(seed);

      //count the # of voxels of the bone that belongs to each armature edge:
      // regionSize[i] gives the # of bone voxels that belong to armature edge i
      std::vector<int> regionSize(this->GetMaxEdgeLabel()+1,0);
      std::vector<Voxel> bd;
      bd.push_back(seed);
      int numVisited(1);
      while(!bd.empty())
        {
        Voxel p = bd.back();  bd.pop_back();
        ++numVisited;
        CharType edgeLabel = this->BodyPartition->GetPixel(p);
        regionSize[static_cast<size_t>(edgeLabel)]++;
        visited->SetPixel(p,true);
        for(int i=0; i<6; ++i)
          {
          Voxel q = p + offsets[i];
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
    itk::ImageRegionIterator<LabelImage> boneComponentIter(boneComponents,boneComponents->GetLargestPossibleRegion());
    for(boneComponentIter.GoToBegin(); !boneComponentIter.IsAtEnd(); ++boneComponentIter)
      {
      LabelType oldLabel = boneComponentIter.Get();
      LabelType newLabel = newLabels[static_cast<size_t>(oldLabel)];
      boneComponentIter.Set(newLabel);
      }

    for(EdgeType i=0; i<this->GetNumberOfEdges(); ++i)
      {
      LabelType edgeLabel = static_cast<LabelType>(this->GetEdgeLabel(i));
      if(std::find(newLabels.begin(), newLabels.end(),edgeLabel)==newLabels.end())
        {
        std::cout << "No bones belong to edge "<<i<<" with label "<<edgeLabel << std::endl;
        }
      }
    this->BonePartition = boneComponents;
    }

  // for debugging
  if (this->GetDumpDebugImages())
    {
    bender::IOUtils::WriteImage<LabelImage>(this->BonePartition,"./bonepartition.mha");
    itk::ImageRegionIteratorWithIndex<LabelImage> boneIter(BonePartition,imDomain);
    bender::IOUtils::WriteImage<LabelImage>(BonePartition,"./bones.mha");
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
}

//-------------------------------------------------------------------------------
template<class T>
inline int NumConnectedComponents(typename T::Pointer domain)
{
  typedef typename itk::Image<unsigned short, T::ImageDimension> OutImageType;
  typedef itk::ConnectedComponentImageFilter<T, OutImageType>  ConnectedComponent;
  typename ConnectedComponent::Pointer connectedComponents = ConnectedComponent::New ();
  connectedComponents->SetInput(domain);
  connectedComponents->SetBackgroundValue(0);
  connectedComponents->Update();
  return connectedComponents->GetObjectCount();
}

//-------------------------------------------------------------------------------
int Expand(const std::vector<Voxel>& seeds,
           CharImage::Pointer domain, int distance,
           LabelImage::Pointer labelMap, unsigned short foreGroundMin,
           unsigned char seedLabel,
           unsigned char domainLabel)
{
  CharImage::RegionType allRegion = labelMap->GetLargestPossibleRegion();
  Neighborhood<3> neighbors;
  VoxelOffset* offsets = neighbors.Offsets ;

  //grow by distance
  typedef std::vector<Voxel> PixelVector;
  PixelVector bd = seeds;
  int regionSize(0);
  for(PixelVector::iterator i = bd.begin(); i!=bd.end();++i)
    {
    if(labelMap->GetPixel(*i)>=foreGroundMin)
      {
      domain->SetPixel(*i,seedLabel);
      ++regionSize;
      }
    }
  for(int dist=2; dist<=distance; ++dist)
    {
    PixelVector newBd;
    for(PixelVector::iterator i = bd.begin(); i!=bd.end();++i)
      {
      Voxel pIndex = *i;
      for(int iOff=0; iOff<6; ++iOff)
        {
        const VoxelOffset& offset = offsets[iOff];
        Voxel qIndex = pIndex + offset;
        if(      allRegion.IsInside(qIndex)
              && labelMap->GetPixel(qIndex)>=foreGroundMin
              && domain->GetPixel(qIndex)==0)
          {
          ++regionSize;
          newBd.push_back(qIndex);
          domain->SetPixel(qIndex,domainLabel);
          }
        }
      }
    bd = newBd;
    }
  return regionSize;
}

//-------------------------------------------------------------------------------
int Expand(const LabelImage::Pointer labelMap,
           LabelType label,
           LabelType forGroundMin,
           int distance, //input
           CharImage::Pointer domain,  //output
           unsigned char domainLabel)
{
  Region allRegion = labelMap->GetLargestPossibleRegion();
  Neighborhood<3> neighbors;
  VoxelOffset* offsets = neighbors.Offsets ;

  //grow by distance
  typedef std::vector<Voxel> PixelVector;
  PixelVector bd;
  int regionSize(0);
  itk::ImageRegionIteratorWithIndex<LabelImage> it(labelMap,allRegion);
  for(it.GoToBegin(); !it.IsAtEnd(); ++it)
    {
    Voxel p = it.GetIndex();
    if(it.Get()==label)
      {
      bd.push_back(p);
      if(domain->GetPixel(p)==0)
        {
        domain->SetPixel(it.GetIndex(),domainLabel);
        regionSize++;
        }
      }
    }
  for(int dist=2; dist<=distance; ++dist)
    {
    PixelVector newBd;
    for(PixelVector::iterator i = bd.begin(); i!=bd.end();++i)
      {
      Voxel pIndex = *i;
      for(int iOff=0; iOff<6; ++iOff)
        {
        const VoxelOffset& offset = offsets[iOff];
        Voxel qIndex = pIndex + offset;
        if(allRegion.IsInside(qIndex) && domain->GetPixel(qIndex)==0
            && labelMap->GetPixel(qIndex)>=forGroundMin)
          {
          ++regionSize;
          newBd.push_back(qIndex);
          domain->SetPixel(qIndex,domainLabel);
          }
        }
      }
    bd = newBd;
    }
  return regionSize;
}

//-------------------------------------------------------------------------------
class GlobalBodyHeatDiffusionProblem: public HeatDiffusionProblem<3>
{
public:
  GlobalBodyHeatDiffusionProblem(LabelImage::Pointer body,LabelImage::Pointer bones)
    :Body(body),Bones(bones)
  {
  }

  //Is the voxel inside the problem domain?
  bool InDomain(const Voxel& voxel) const
  {
    return this->Body->GetLargestPossibleRegion().IsInside(voxel)
      && this->Body->GetPixel(voxel)!=0;
  }

  bool IsBoundary(const Voxel& p) const
  {
    return this->Bones->GetPixel(p)>=2;
  }

  float GetBoundaryValue(const Voxel&) const
  {
    assert(false); //not needed now
    return 0;
  }
private:
  LabelImage::Pointer Body;
  LabelImage::Pointer Bones;
};

//-------------------------------------------------------------------------------
class LocalizedBodyHeatDiffusionProblem: public HeatDiffusionProblem<3>
{
public:
  LocalizedBodyHeatDiffusionProblem(CharImage::Pointer domain,
                                    LabelImage::Pointer sourceMap,
                                    LabelType hotSourceLabel
  ):Domain(domain),
    SourceMap(sourceMap),
    HotSourceLabel(hotSourceLabel)
  {
    this->WholeDomain = this->Domain->GetLargestPossibleRegion();
  }

  //Is the voxel inside the problem domain?
  bool InDomain(const Voxel& voxel) const
  {
    return this->WholeDomain.IsInside(voxel) && this->Domain->GetPixel(voxel)!=0;
  }

  bool IsBoundary(const Voxel& voxel) const
  {
    return this->SourceMap->GetPixel(voxel)!=0;
  }

  float GetBoundaryValue(const Voxel& voxel) const
  {
    LabelType label = this->SourceMap->GetPixel(voxel);
    return label==this->HotSourceLabel? 1.0 : 0.0;
  }

private:
  CharImage::Pointer Domain;   //a binary image that describes the domain
  LabelImage::Pointer SourceMap; //a label image that defines the heat sources
  LabelType HotSourceLabel; //any source voxel with this label will be assigned weight 1

  Region WholeDomain;
};

//-------------------------------------------------------------------------------
ArmatureEdge::ArmatureEdge(const ArmatureType& armature, EdgeType id)
  : Armature(armature)
  , Id(id)
  , Debug(false)
  , DumpDebugImages(false)
{
  this->Domain = CharImage::New();
  Allocate<LabelImage,CharImage>(armature.BodyMap, this->Domain);
  this->Domain->FillBuffer(ArmatureType::BackgroundLabel);
}

//-------------------------------------------------------------------------------
void ArmatureEdge::SetDebug(bool debug)
{
  this->Debug = debug;
}

//-------------------------------------------------------------------------------
bool ArmatureEdge::GetDebug()const
{
  return this->Debug;
}

//-------------------------------------------------------------------------------
void ArmatureEdge::SetDumpDebugImages(bool dump)
{
  this->DumpDebugImages = dump;
}

//-------------------------------------------------------------------------------
bool ArmatureEdge::GetDumpDebugImages()const
{
  return this->DumpDebugImages;
}

//-------------------------------------------------------------------------------
void ArmatureEdge::Initialize(int expansionDistance)
{
  Region imDomain = this->Armature.BodyMap->GetLargestPossibleRegion();

  // Compute the "domain" of the armature edge
  // by expanding fixed distance around the Voronoi region
  itk::ImageRegionIteratorWithIndex<CharImage> it(this->Domain, imDomain);
  size_t regionSize = 0;

  // Start with the Vorononi region
  CharType label = this->GetLabel();
  for (it.GoToBegin(); !it.IsAtEnd(); ++it)
    {
    // \todo Don't use GetIndex/GetPixel, use iterator instead.
    if (this->Armature.BodyPartition->GetPixel(it.GetIndex()) == label)
      {
      it.Set(ArmatureType::DomainLabel);
      ++regionSize;
      }
    }
  std::cout << "Voronoi region size: "<<regionSize << std::endl;
  assert(NumConnectedComponents<CharImage>(this->Domain)==1);

  LabelType unknown = ArmatureType::DomainLabel;
  regionSize += Expand(this->Armature.BodyPartition, this->GetLabel(), unknown,
                       expansionDistance,
                       this->Domain, ArmatureType::DomainLabel);

  if (this->GetDumpDebugImages())
    {
    bender::IOUtils::WriteImage<CharImage>(this->Domain, "./region.mha");
    }

  // Debug
  if (this->GetDebug())
    {
    std::cout << "Region size after expanding " << expansionDistance
              << " : " << regionSize << std::endl;

    Voxel bbMin, bbMax;
    for(int i=0; i<3; ++i)
      {
      bbMin[i] = imDomain.GetSize()[i]-1;
      bbMax[i] = 0;
      }
    for (it.GoToBegin(); !it.IsAtEnd(); ++it)
      {
      Voxel p  = it.GetIndex();
      if (it.Get() == ArmatureType::BackgroundLabel)
        {
        continue;
        }
      for(int i=0; i<3; ++i)
        {
        if(p[i]<bbMin[i])
          {
          bbMin[i] = p[i];
          }
        if(p[i]>bbMax[i])
          {
          bbMax[i] = p[i];
          }
        }
      }
    std::cout << "Domain bounding box: "<<bbMin<<" "<<bbMax << std::endl;
    }
}

//-------------------------------------------------------------------------------
WeightImage::Pointer ArmatureEdge
::ComputeWeight(bool binaryWeight, int smoothingIterations)
{
  if (this->GetDebug())
    {
    std::cout << "Compute weight for edge "<< this->Id
              <<" with label "<<static_cast<int>(this->GetLabel()) << std::endl;
    }
  WeightImage::Pointer weight = WeightImage::New();
  Allocate<LabelImage,WeightImage>(this->Armature.BodyMap, weight);

  size_t numBackground(0);
  itk::ImageRegionIteratorWithIndex<LabelImage> it(
    this->Armature.BodyMap, this->Armature.BodyMap->GetLargestPossibleRegion());
  for (it.GoToBegin();!it.IsAtEnd(); ++it)
    {
    if(it.Get() != ArmatureType::BackgroundLabel)
      {
      // \todo GetIndex/SetPixel is slow, use iterator instead.
      weight->SetPixel(it.GetIndex(),0.0);
      }
    else
      {
      weight->SetPixel(it.GetIndex(),-1.0f);
      ++numBackground;
      }
    }
  std::cout << numBackground <<" background voxel" << std::endl;

  if (binaryWeight)
    {
    itk::ImageRegionIteratorWithIndex<CharImage> domainIt(
      this->Domain,this->Domain->GetLargestPossibleRegion());
    for (domainIt.GoToBegin(); !domainIt.IsAtEnd(); ++domainIt)
      {
      if (domainIt.Get()>0)
        {
        weight->SetPixel(domainIt.GetIndex(), 1.0);
        }
      }
    }
  else
    {
    //First solve a localized verison of the problme exactly
    LocalizedBodyHeatDiffusionProblem localizedProblem(
      this->Domain, this->Armature.BonePartition, this->GetLabel());
    SolveHeatDiffusionProblem<WeightImage>::Solve(localizedProblem, weight);

    //Approximate the global solution by iterative solving
    GlobalBodyHeatDiffusionProblem globalProblem(
      this->Armature.BodyMap, this->Armature.BonePartition);
    SolveHeatDiffusionProblem<WeightImage>::SolveIteratively(
      globalProblem,weight,smoothingIterations);
    }
  return weight;
}

//-------------------------------------------------------------------------------
CharType ArmatureEdge::GetLabel() const
{
  return this->Armature.GetEdgeLabel(this->Id);
}
