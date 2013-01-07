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

// Bender includes
#include "ArmatureWeightCLP.h"

// ArmatureWeight includes
#include "HeatDiffusionProblem.h"
#include "SolveHeatDiffusionProblem.h"

// ITK includes
#include <itkImage.h>
#include <itkImageFileWriter.h>
#include <itkStatisticsImageFilter.h>
#include <itkBinaryThresholdImageFilter.h>
#include <itkPluginUtilities.h>
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
#include <sstream>
#include <vector>
#include <iostream>
#include <iomanip>

typedef unsigned char CharType;
typedef unsigned short LabelType;
typedef float WeightImagePixel;

typedef itk::Image<unsigned short, 3>  LabelImage;
typedef itk::Image<unsigned char, 3>  CharImage;

typedef itk::Image<WeightImagePixel, 3>  WeightImage;

typedef itk::Index<3> Voxel;
typedef itk::Offset<3> VoxelOffset;
typedef itk::ImageRegion<3> Region;

static bool DumpSegmentationImages = false;
//-------------------------------------------------------------------------------
inline void InvertXY(double* x)
{
  x[0]*=-1;
  x[1]*=-1;
}

//-------------------------------------------------------------------------------
template<unsigned int dimension>
class Neighborhood
{
public:
  Neighborhood()
  {
    for(unsigned int i=0; i<dimension; ++i)
      {
      int lo = 2*i;
      int hi = 2*i+1;
      for(unsigned int j=0; j<dimension; ++j)
        {
        this->Offsets[lo][j] = j==i? -1 : 0;
        this->Offsets[hi][j] = j==i?  1 : 0;
        }
      }
  }
  itk::Offset<dimension> Offsets[2*dimension];
};

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
void WriteImage(typename ImageType::Pointer image,const char* fname)
{
  std::cout << "Write image to "<<fname << std::endl;
  typedef typename itk::ImageFileWriter<ImageType> WriterType;
  typename WriterType::Pointer writer = WriterType::New();
  writer->SetFileName(fname);
  writer->SetInput(image);
  writer->SetUseCompression(1);
  writer->Update();
}

//-------------------------------------------------------------------------------
template <class ImageType>
void WriteImageRegion(typename ImageType::Pointer image, typename ImageType::PixelType value, const char* fname)
{
  typedef typename itk::Point<float, ImageType::ImageDimension> PointType;
  vtkNew<vtkPolyData> poly;
  vtkNew<vtkPoints> points;
  itk::ImageRegionIteratorWithIndex<ImageType> it(image,image->GetLargestPossibleRegion());
  for(it.GoToBegin(); !it.IsAtEnd(); ++it)
    {
    if(it.Get()==value)
      {
      PointType p;
      image->TransformIndexToPhysicalPoint(it.GetIndex(),p);
      points->InsertNextPoint(p[0],p[1],p[2]);
      }
    }

  poly->SetPoints(points.GetPointer());
  vtkNew<vtkPolyDataWriter> writer;
  writer->SetFileName(fname);
  writer->SetInput(poly.GetPointer());
  writer->Update();

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
template<class T>
inline int NumConnectedComponents(typename T::Pointer image, typename T::PixelType value)
{
  typedef itk::BinaryThresholdImageFilter<T, T> Threshold;
  typename Threshold::Pointer threshold = Threshold::New();
  threshold->SetInput(image);
  threshold->SetLowerThreshold(value);
  threshold->SetUpperThreshold(value);
  threshold->SetInsideValue(1);
  threshold->SetOutsideValue(0);

  typedef itk::ConnectedComponentImageFilter<T, T>  ConnectedComponent;
  typename ConnectedComponent::Pointer connectedComponents = ConnectedComponent::New ();
  connectedComponents->SetInput(threshold->GetOutput());
  connectedComponents->SetBackgroundValue(0);
  connectedComponents->Update();
  return connectedComponents->GetObjectCount();

}

//-------------------------------------------------------------------------------
//Expand the foreground once. The new foreground pixels are assigned foreGroundMin
int ExpandForegroundOnce(LabelImage::Pointer labelMap, unsigned short foreGroundMin)
{
  int numNewVoxels=0;
  CharImage::RegionType region = labelMap->GetLargestPossibleRegion();
  itk::ImageRegionIteratorWithIndex<LabelImage> it(labelMap,region);
  Neighborhood<3> neighbors;
  VoxelOffset* offsets = neighbors.Offsets;

  std::vector<Voxel> front;
  for(it.GoToBegin(); !it.IsAtEnd(); ++it)
    {
    Voxel p = it.GetIndex();
    LabelImage::PixelType value = it.Get();
    if(value>=foreGroundMin)
      {
      for(int iOff=0; iOff<6; ++iOff)
        {
        const VoxelOffset& offset = offsets[iOff];
        Voxel q = p + offset;
        if(region.IsInside(q) && labelMap->GetPixel(q)<foreGroundMin)
          {
          front.push_back(q);
          }
        }
      }
    }
  for(std::vector<Voxel>::const_iterator i = front.begin(); i!=front.end();i++)
    {
    if(labelMap->GetPixel(*i)<foreGroundMin)
      {
      labelMap->SetPixel( *i, foreGroundMin);
      ++numNewVoxels;
      }
    }
  return numNewVoxels;
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
class ArmatureType
{
public:
  enum
  {
    DomainLabel = 1
  };

  ArmatureType(LabelImage::Pointer image): BodyMap(image)
  {
    this->BodyPartition = LabelImage::New();
    Allocate<LabelImage,LabelImage>(image, this->BodyPartition);
    this->BodyPartition->FillBuffer(0);

  }
  static CharType GetEdgeLabel(int i)
  {
    //0 is background, 1 is body interior so armature must start with 2
    return static_cast<CharType>(i+2);
  }

  CharType GetMaxEdgeLabel() const
  {
    //return the last edge id
    int lastEdge = this->GetNumberOfEdges()-1;
    return static_cast<CharType>(ArmatureType::GetEdgeLabel(lastEdge));
  }

  LabelImage::Pointer BodyMap;
  LabelImage::Pointer BodyPartition; //the partition of body by armature edges
  LabelImage::Pointer BonePartition; //the partition of bones by armature edges

  std::vector<std::vector<Voxel> > SkeletonVoxels;
  int GetNumberOfEdges() const { return SkeletonVoxels.size(); }
  std::vector<CharImage::Pointer> Domains;
  std::vector<Voxel> Fixed;
  std::vector<WeightImage::Pointer> Weights;

  void Init(const char* fname, bool invertXY)
  {
    vtkNew<vtkPolyDataReader> reader;
    reader->SetFileName(fname);
    reader->Update();
    this->InitSkeleton(reader->GetOutput(), invertXY);

    this->InitBones();
  }
private:
  void InitSkeleton(vtkPolyData* armPoly, bool invertXY)
  {
    vtkCellArray* armatureSegments = armPoly->GetLines();

    //iterate over the edges of the armature and rasterize them
    vtkNew<vtkIdList> cell;
    armatureSegments->InitTraversal();
    int edgeId=0;
    while(armatureSegments->GetNextCell(cell.GetPointer()))
      {
      assert(cell->GetNumberOfIds()==2);
      vtkIdType a = cell->GetId(0);
      vtkIdType b = cell->GetId(1);

      double ax[3], bx[3];
      armPoly->GetPoints()->GetPoint(a, ax);
      armPoly->GetPoints()->GetPoint(b, bx);

      if(invertXY)
        {
        InvertXY(ax);
        InvertXY(bx);
        }

      this->SkeletonVoxels.push_back(std::vector<Voxel>());
      std::vector<Voxel>& edgeVoxels(this->SkeletonVoxels.back());
      Rasterize<LabelImage>(ax,bx,this->BodyPartition, edgeVoxels);
      if (edgeVoxels.size() == 0)
        {
        continue;
        }
      //XXX we really need to make sure that the rasterized edge is a connected component
      //just a hack here
      if (edgeVoxels.size() > 2)
        {
        // Discard points a and b
        edgeVoxels.erase(edgeVoxels.begin());
        edgeVoxels.erase(edgeVoxels.begin() + edgeVoxels.size() - 1);
        }
      CharType label = ArmatureType::GetEdgeLabel(edgeId);
      int numOutside(0);
      for(std::vector<Voxel>::iterator vi = edgeVoxels.begin(); vi!=edgeVoxels.end(); ++vi)
        {
        if(this->BodyMap->GetPixel(*vi)!=0 && this->BodyPartition->GetPixel(*vi)==0)
          {
          this->BodyPartition->SetPixel(*vi, label);
          }
        else
          {
          if(this->BodyMap->GetPixel(*vi)==0)
            {
            ++numOutside;
            }
          }
        }
      if(numOutside>0)
        {
        std::cout << "WARNING: armature edge "<<edgeId<<" has "<<numOutside<<" outside voxels out of "<<edgeVoxels.size() << std::endl;
        }

      if(this->SkeletonVoxels.back().size()<2)
        {
        std::cout << "WARNING: edge "<<edgeId<<" is very small" << std::endl;
        }
      ++edgeId;
      }

    //Compute the voronoi of the skeleton
    //Step 1: color the non-skeleton body voxels by value unkwn
    LabelType unknown = ArmatureType::GetEdgeLabel(-1);
    itk::ImageRegionIteratorWithIndex<LabelImage> it(this->BodyMap,this->BodyMap->GetLargestPossibleRegion());
    for(it.GoToBegin(); !it.IsAtEnd(); ++it)
      {
      if(it.Get()>0)
        {
        Voxel voxel = it.GetIndex();
        LabelType label = this->BodyPartition->GetPixel(voxel);
        if(label==0)
          {
          this->BodyPartition->SetPixel(voxel,unknown);
          }
        }
      }

    //Step 2:
    ComputeManhattanVoronoi<LabelImage>(this->BodyPartition,0,unknown);
    if(DumpSegmentationImages)
      {
      WriteImage<LabelImage>(this->BodyPartition,"./bodypartition.mha");
      }
  }

  void InitBones()
  {
    Region imDomain = this->BodyMap->GetLargestPossibleRegion();
    Neighborhood<3> neighbors;
    const VoxelOffset* offsets = neighbors.Offsets;

    //select the bones and label them by componnets
    typedef itk::BinaryThresholdImageFilter<LabelImage, CharImage> Threshold;
    Threshold::Pointer threshold = Threshold::New();
    threshold->SetInput(this->BodyMap);
    threshold->SetLowerThreshold(209); //bone marrow
    threshold->SetInsideValue(1);
    threshold->SetOutsideValue(0);
    threshold->Update();

    //Partition the bones by armature edges
    //Two goals:
    // no-split: Each natural bone should be assigned one label
    // split-joined: If a set of natural bones are connected in the voxel
    //  space, we would like to partition them.
    //
    if(1) //simple and stupid
      {
      this->BonePartition = LabelImage::New();
      Allocate<LabelImage,LabelImage>(this->BodyMap,BonePartition);
      this->BonePartition->FillBuffer(0);
      itk::ImageRegionIteratorWithIndex<LabelImage> boneIter(BonePartition,imDomain);
      CharImage::Pointer boneInside = threshold->GetOutput();

      for(boneIter.GoToBegin(); !boneIter.IsAtEnd(); ++boneIter)
        {
        Voxel voxel = boneIter.GetIndex();
        if(boneInside->GetPixel(voxel)!=0)
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
      connectedComponents->SetBackgroundValue(0);
      connectedComponents->Update();
      LabelImage::Pointer boneComponents =  connectedComponents->GetOutput();

      int numBones = connectedComponents->GetObjectCount();

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
      for(it.GoToBegin(); !it.IsAtEnd(); ++it)
        {
        //i is the component id of the bone, with 0 being the background
        size_t i = static_cast<size_t>(it.Get());
        if(i>0)
          {
          assert(i>=1 && i<=boneSeeds.size());
          boneSeeds[i-1] = it.GetIndex();
          }
        }

      //verify that seeds are valid
      for(std::vector<Voxel>::iterator i=boneSeeds.begin();i!=boneSeeds.end();++i)
        {
        assert((*i)!=invalidVoxel);
        }

      //compute a map from the old to the new labels:
      //  newLabels[oldLabel] is the new label
      std::vector<LabelType> newLabels(numBones+1,0);
      for(std::vector<Voxel>::iterator seedIter=boneSeeds.begin();seedIter!=boneSeeds.end();++seedIter)
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
      //Relabel the image
      itk::ImageRegionIterator<LabelImage> boneComponentIter(boneComponents,boneComponents->GetLargestPossibleRegion());
      for(boneComponentIter.GoToBegin(); !boneComponentIter.IsAtEnd(); ++boneComponentIter)
        {
        LabelType oldLabel = boneComponentIter.Get();
        LabelType newLabel = newLabels[static_cast<size_t>(oldLabel)];
        boneComponentIter.Set(newLabel);
        }

      for(int i=0; i<this->GetNumberOfEdges(); ++i)
        {
        LabelType edgeLabel = static_cast<LabelType>(this->GetEdgeLabel(i));
        if(std::find(newLabels.begin(), newLabels.end(),edgeLabel)==newLabels.end())
          {
          std::cout << "No bones belong to edge "<<i<<" with label "<<edgeLabel << std::endl;
          }
        }
      BonePartition = boneComponents;
      }

//for debugging
    if(DumpSegmentationImages)
      {
      WriteImage<LabelImage>(this->BonePartition,"./bonepartition.mha");
          itk::ImageRegionIteratorWithIndex<LabelImage> boneIter(BonePartition,imDomain);
      WriteImage<LabelImage>(BonePartition,"./bones.mha");
      std::vector<int> componentSize(this->GetMaxEdgeLabel()+1,0);
      for(boneIter.GoToBegin(); !boneIter.IsAtEnd(); ++boneIter)
        {
        assert(boneIter.Get()<componentSize.size());
        componentSize[boneIter.Get()]++;
        }
      int totalSize=0;
      for(size_t i=0;i<componentSize.size();++i)
        {
        totalSize+=componentSize[i];
        std::cout << i<<": "<<componentSize[i]<<"\n";
        }
      }

  }
};


//-------------------------------------------------------------------------------
class ArmatureEdge
{
public:
  ArmatureEdge(ArmatureType& armature, int id): Armature(armature),Id(id)
  {
    this->Domain = CharImage::New();
    Allocate<LabelImage,CharImage>(armature.BodyMap, this->Domain);
    this->Domain->FillBuffer(0);
  };

  void Initialize(int expansionDistance)
  {
    Region imDomain = Armature.BodyMap->GetLargestPossibleRegion();

    //Compute the "domain" of the armature edge
    // by expanding fixed distance around the Voronoi region
    itk::ImageRegionIteratorWithIndex<CharImage> it(this->Domain,imDomain);
    int regionSize = 0;

    //start with the Vorononi region
    CharType label = this->GetLabel();
    for(it.GoToBegin(); !it.IsAtEnd(); ++it)
      {
      if(this->Armature.BodyPartition->GetPixel(it.GetIndex())==label)
        {
        it.Set(ArmatureType::DomainLabel);
        ++regionSize;
        }
      }
    std::cout << "Voronoi region size: "<<regionSize << std::endl;
    assert(NumConnectedComponents<CharImage>(this->Domain)==1);

    LabelType unknown = ArmatureType::GetEdgeLabel(-1);
    regionSize+=Expand(this->Armature.BodyPartition, this->GetLabel(), unknown,
                       expansionDistance,
                       this->Domain,ArmatureType::DomainLabel);

    if(DumpSegmentationImages)
      {
      typedef itk::ImageFileWriter<CharImage> WriterType;
      WriterType::Pointer writer = WriterType::New();
      writer->SetFileName( "./region.mha");
      writer->SetInput(this->Domain);
      writer->SetUseCompression(1);
      writer->Update();
      }

    //Debug
    std::cout << "Region size after expanding "<<expansionDistance<<" : "<<regionSize << std::endl;

    Voxel bbMin, bbMax;
    for(int i=0; i<3; ++i)
      {
      bbMin[i] = imDomain.GetSize()[i]-1;
      bbMax[i] = 0;
      }
    for(it.GoToBegin(); !it.IsAtEnd(); ++it)
      {
      Voxel p  = it.GetIndex();
      if(it.Get()==0)
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

  WeightImage::Pointer ComputeWeight(bool binaryWeight, int smoothingIterations)
  {
    std::cout << "Compute weight for edge "<<this->Id<<" with label "<<(int)this->GetLabel() << std::endl;
    WeightImage::Pointer weight = WeightImage::New();
    Allocate<LabelImage,WeightImage>(this->Armature.BodyMap, weight);

    int numBackground(0);
    for(itk::ImageRegionIteratorWithIndex<LabelImage> it(this->Armature.BodyMap,this->Armature.BodyMap->GetLargestPossibleRegion());
        !it.IsAtEnd(); ++it)
      {
      if(it.Get()>0)
        {
        weight->SetPixel(it.GetIndex(),0.0);
        }
      else
        {
        weight->SetPixel(it.GetIndex(),-1.0f);
        ++numBackground;
        }
      }
    std::cout << numBackground<<" background voxel" << std::endl;

    if(binaryWeight)
      {
      itk::ImageRegionIteratorWithIndex<CharImage> it(this->Domain,this->Domain->GetLargestPossibleRegion());
      for(it.GoToBegin(); !it.IsAtEnd(); ++it)
        {
        if(it.Get()>0)
          {
          weight->SetPixel(it.GetIndex(), 1.0);
          }
        }
      }
    else
      {
      //First solve a localized verison of the problme exactly
      LocalizedBodyHeatDiffusionProblem localizedProblem(this->Domain, this->Armature.BonePartition, this->GetLabel());
      SolveHeatDiffusionProblem<WeightImage>::Solve(localizedProblem, weight);

      //Approximate the global solution by iterative solving
      GlobalBodyHeatDiffusionProblem globalProblem(this->Armature.BodyMap, this->Armature.BonePartition);
      SolveHeatDiffusionProblem<WeightImage>::SolveIteratively(globalProblem,weight,smoothingIterations);
      }

    return weight;
  }
  CharType GetLabel() const{ return this->Armature.GetEdgeLabel(this->Id);}

private:
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

  const ArmatureType& Armature;
  int Id;
  CharImage::Pointer Domain;
  Region ROI;
};

//-------------------------------------------------------------------------------
inline int NumDigits(unsigned int a)
{
  int numDigits(0);
  while(a>0)
    {
    a = a/10;
    ++numDigits;
    }
  return numDigits;
}

//-------------------------------------------------------------------------------
void RemoveSingleVoxelIsland(LabelImage::Pointer labelMap)
{
  Neighborhood<3> neighbors;
  const VoxelOffset* offsets = neighbors.Offsets;

  Region region = labelMap->GetLargestPossibleRegion();
  itk::ImageRegionIteratorWithIndex<LabelImage> it(labelMap,region);
  for(it.GoToBegin(); !it.IsAtEnd(); ++it)
    {
    if(it.Get()>0)
      {
      Voxel p = it.GetIndex();
      int numNeighbors(0);
      for(int iOff=0; iOff<6; ++iOff)
        {
        Voxel q = p + offsets[iOff];
        if( region.IsInside(q) && labelMap->GetPixel(q)>0)
          {
          ++numNeighbors;
          }
        }
      if(numNeighbors==0)
        {
        cerr<<"Paint isolated voxel "<<p<<" to background" << std::endl;
        labelMap->SetPixel(p, 0);
        }
      }
    }
}

//-------------------------------------------------------------------------------
int main( int argc, char * argv[] )
{
  PARSE_ARGS;

  if(BinaryWeight)
    {
    std::cout << "Use binary weight: " << std::endl;
    }

  if(!IsArmatureInRAS)
    {
    std::cout << "Input armature is not in RAS coordinate system; will convert it to RAS." << std::endl;
    }

  std::cout<<"Padding distance: "<<Padding<<endl;

  if(DumpDebugImages)
    {
    DumpSegmentationImages = true;
    }

  //----------------------------
  // Read label map
  //----------------------------
  typedef itk::ImageFileReader<LabelImage>  ReaderType;
  ReaderType::Pointer labelMapReader = ReaderType::New();
  labelMapReader->SetFileName(RestLabelmap.c_str() );
  labelMapReader->Update();

  typedef itk::StatisticsImageFilter<LabelImage>  StatisticsType;
  StatisticsType::Pointer statistics = StatisticsType::New();
  statistics->SetInput( labelMapReader->GetOutput() );
  statistics->Update();

  //----------------------------
  // Print out some statistics
  //----------------------------
  LabelImage::Pointer labelMap = labelMapReader->GetOutput();
  Region allRegion = labelMap->GetLargestPossibleRegion();
  itk::ImageRegionIteratorWithIndex<LabelImage> it(labelMap,labelMap->GetLargestPossibleRegion());
  LabelType bodyIntensity = 1;
  LabelType boneIntensity = 209; //marrow
  int numBodyVoxels(0),numBoneVoxels(0);
  for(it.GoToBegin(); !it.IsAtEnd(); ++it)
    {
    if(it.Get()>=bodyIntensity)
      {
      ++numBodyVoxels;
      }
    if(it.Get()>=boneIntensity)
      {
      ++numBoneVoxels;
      }
    }
  int totalVoxels = allRegion.GetSize()[0]*allRegion.GetSize()[1]*allRegion.GetSize()[2];

  std::cout << "Image statistics\n";
  std::cout << "  min: "<<(int)statistics->GetMinimum()<<" max: "<<(int)statistics->GetMaximum() << std::endl;
  std::cout << "  total # voxels  : "<<totalVoxels << std::endl;
  std::cout << "  num body voxels : "<<numBodyVoxels << std::endl;
  std::cout << "  num bone voxels : "<<numBoneVoxels << std::endl;

  //----------------------------
  // Preprocess of the labelmap
  //----------------------------
  RemoveSingleVoxelIsland(labelMap);
  int numPaddedVoxels =0;
  for(int i=0; i<Padding; i++)
    {
    numPaddedVoxels+=ExpandForegroundOnce(labelMap,bodyIntensity);
    cout<<"Padded "<<numPaddedVoxels<<" voxels"<<endl;
    }

  //----------------------------
  // Read armature information
  //----------------------------
  ArmatureType armature(labelMap);
  armature.Init(ArmaturePoly.c_str(),!IsArmatureInRAS);

  if (LastEdge<0)
    {
    LastEdge = armature.GetNumberOfEdges()-1;
    }
  std::cout << "Process armature edge from "<<FirstEdge<<" to "<<LastEdge << std::endl;

  //--------------------------------------------
  // Compute the domain of reach armature part
  //--------------------------------------------
  int numDigits = NumDigits(armature.GetNumberOfEdges());
  for(int i=FirstEdge; i<=LastEdge; ++i)
    {
    ArmatureEdge edge(armature,i);
    std::cout << "Process armature edge "<<i<<" with label "<<(int)edge.GetLabel() << std::endl;
    edge.Initialize(BinaryWeight? 0 : ExpansionDistance);
    WeightImage::Pointer weight = edge.ComputeWeight(BinaryWeight,SmoothingIteration);
    std::stringstream filename;
    filename<<WeightDirectory<<"/weight_"<<setfill('0')<<setw(numDigits)<<i<<".mha";
    WriteImage<WeightImage>(weight,filename.str().c_str());
    }

  return EXIT_SUCCESS;
}
