/*=========================================================================

  Program:   Insight Segmentation & Registration Toolkit
  Module:    $HeadURL$
  Language:  C++
  Date:      $Date$
  Version:   $Revision$

  Copyright (c) Insight Software Consortium. All rights reserved.
  See ITKCopyright.txt or http://www.itk.org/HTML/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "ArmatureWeightCLP.h"

//-----------itk------------

#include "itkImage.h"
#include "itkImageFileWriter.h"
#include "itkStatisticsImageFilter.h"
#include "itkBinaryThresholdImageFilter.h"
#include "itkPluginUtilities.h"
#include "itkImageRegionIteratorWithIndex.h"
#include "itkBresenhamLine.h"
#include "itkMath.h"
#include "itkIndex.h"
#include "itkConnectedComponentImageFilter.h"

//-----------vtk------------
#include "vtkNew.h"
#include "vtkPolyDataReader.h"
#include "vtkPolyDataWriter.h"
#include "vtkPolyData.h"
#include "vtkCellArray.h"
#include "vtkTimerLog.h"

//-----------standard------------
#include <sstream>
#include <vector>
#include <iostream>

//-----------Eigen wrapper------------

#include "EigenSparseSolve.h"

using namespace std;

typedef unsigned char CharType;
typedef unsigned short LabelType;

typedef itk::Image<unsigned short, 3>  LabelImage;
typedef itk::Image<unsigned char, 3>  CharImage;
typedef itk::Image<float, 3>  WeightImage;

typedef itk::Index<3> Voxel;
typedef itk::Offset<3> VoxelOffset;
typedef itk::ImageRegion<3> Region;

bool DumpSegmentationImages = false;

template<unsigned int dimension>
class Neighborhood
{
public:
  Neighborhood()
  {
    for(unsigned int i=0; i<dimension; i++)
      {
      int lo = 2*i;
      int hi = 2*i+1;
      for(unsigned int j=0; j<dimension; j++)
        {
        this->Offsets[lo][j] = j==i? -1 : 0;
        this->Offsets[hi][j] = j==i?  1 : 0;
        }
      }
  }
  itk::Offset<dimension> Offsets[2*dimension];
};


//a 3D shape defined by a set of pixels
template<unsigned int dimension>
class PixelShape
{
public:
  typedef itk::Index<dimension> Pixel;
  PixelShape()
  {
  }
  virtual bool IsMember(Pixel p) const=0 ;
  virtual bool IsBoundary(Pixel p) const =0;
  virtual bool IsInterior(Pixel p) const=0;
};

class BodyDiffusionRegion: public PixelShape<3>
{
public:
  BodyDiffusionRegion(LabelImage::Pointer domain, LabelImage::Pointer labelMap)
    :Domain(domain),LabelMap(labelMap)
  {
    this->ImRegion = domain->GetLargestPossibleRegion();
  }
  bool IsMember(Voxel p) const
  {
    return this->ImRegion.IsInside(p) && this->Domain->GetPixel(p)>0;
  }
  virtual bool IsBoundary(Voxel p) const
  {
    if(!this->IsMember(p))
      {
      return false;
      }
    return this->LabelMap->GetPixel(p)!=0;
  }
  virtual bool IsInterior(Voxel p) const
  {
    if(!this->IsMember(p))
      {
      return false;
      }
    return this->LabelMap->GetPixel(p)==0;
  }
private:
  LabelImage::Pointer Domain;
  LabelImage::Pointer LabelMap;
  Region ImRegion;
};

template<class InImage, class OutImage>
void Allocate(typename InImage::Pointer in, typename OutImage::Pointer out)
{
  out->SetOrigin(in->GetOrigin());
  out->SetSpacing(in->GetSpacing());
  out->SetRegions(in->GetLargestPossibleRegion());
  out->Allocate();
}

template <class ImageType>
void WriteImage(typename ImageType::Pointer image,const char* fname)
{
  cout<<"Write image to "<<fname<<endl;
  typedef typename itk::ImageFileWriter<ImageType> WriterType;
  typename WriterType::Pointer writer = WriterType::New();
  writer->SetFileName(fname);
  writer->SetInput(image);
  writer->SetUseCompression(1);
  writer->Update();
}

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

template <class ImageType>
void Rasterize(const double* a, const double* b, const typename ImageType::Pointer image,
               std::vector<typename ImageType::IndexType>& outputPixels)
{
  typedef itk::Index<ImageType::ImageDimension> Pixel;

  typedef typename itk::Point<float, ImageType::ImageDimension> PointType;
  PointType pa,pb;
  unsigned int dimension =ImageType::ImageDimension;
  for(unsigned int i=0; i<dimension; i++)
    {
    pa[i] = a[i];
    pb[i] = b[i];
    }
  Pixel Ia,Ib;
  if(!image->TransformPhysicalPointToIndex(pa,Ia))
    {
    cerr<<"Fail to rasterize\n";
    return;
    }
  if(!image->TransformPhysicalPointToIndex(pb,Ib))
    {
    cerr<<"Fail to rasterize\n";
    return;
    }

  typedef itk::BresenhamLine<ImageType::ImageDimension> Bresenham;
  typedef typename itk::BresenhamLine<ImageType::ImageDimension>::LType DirType;
  Bresenham bresLine;
  DirType dir;
  unsigned int maxSteps(0);
  for(unsigned int i=0; i<dimension; i++)
    {
    dir[i]=b[i]-a[i];
    maxSteps+= (int)fabs(dir[i]);
    }

  float len = dir.GetNorm();
  typename Bresenham::OffsetArray line = bresLine.BuildLine(dir,maxSteps);
  outputPixels.clear();
  for(size_t i=0; i<line.size();i++)
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
    for(typename PixelVector::iterator i = bd.begin(); i!=bd.end();i++)
      {
      PixelValue siteLabel = siteMap->GetPixel(*i);
      Pixel pIndex = *i;
      for(int iOff=0; iOff<2*InputImageType::ImageDimension; iOff++)
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

template<class RealImageType>
void DiffuseHeatIteratively(typename RealImageType::Pointer heat,
                            const PixelShape<RealImageType::ImageDimension>& shape,
                            int numIterations)
{
  typedef typename RealImageType::IndexType Pixel;
  typedef typename RealImageType::PixelType Real;

  typename RealImageType::RegionType region = heat->GetLargestPossibleRegion();
  Neighborhood<RealImageType::ImageDimension> nbrs;
  typedef typename RealImageType::OffsetType OffsetType;
  OffsetType* offsets = nbrs.Offsets;

  std::vector<Pixel> interior;
  for(itk::ImageRegionIteratorWithIndex<RealImageType> it(heat,region); !it.IsAtEnd(); ++it)
    {
    Pixel p = it.GetIndex();
    if(shape.IsInterior(p))
      {
      interior.push_back(p);
      }
    }

  cout<<"Smooth iteratively: ";
  for(int iteration=0; iteration<numIterations; iteration++)
    {
    cout<<". "<<flush;
    for(typename std::vector<Pixel>::iterator pi = interior.begin(); pi!=interior.end(); pi++)
      {
      Pixel p = *pi;
      Real sum=0.0;
      Real diag = 0.0;
      for(OffsetType* offset = offsets; offset!=offsets+2*RealImageType::ImageDimension; offset++)
        {
        Pixel q = p + *offset;
        if(shape.IsMember(q))
          {
          assert(heat->GetPixel(q)>=0);
          sum+=heat->GetPixel(q);
          diag+=1.0;
          }
        }
      if(diag==0.0)
        {
        cerr<<p<<" has no neighbor"<<endl;
        }
      else
        {
        heat->SetPixel(p, sum/diag);
        }
      }
    }
  cout<<endl;
}

void DiffuseHeat(CharImage::Pointer domain, //a binary image that describes the domain
                 LabelImage::Pointer sourceMap, //a label image that defines the heat sources
                 LabelType hotSourceLabel, //any source voxel with this label will be assigned weight 1
                                          //other source voxels are assigned weight 0
                 WeightImage::Pointer heat)  ////output
{
  Region imDomain = domain->GetLargestPossibleRegion();

  Neighborhood<3> neighbors;
  VoxelOffset* offsets = neighbors.Offsets ;

  int m(0),n(0),numHotSource(0);
  itk::ImageRegionIteratorWithIndex<CharImage> it(domain,imDomain);
  for(it.GoToBegin(); !it.IsAtEnd(); ++it)
    {
    if(it.Get()>0)
      {
      n++;
      LabelType label = sourceMap->GetPixel(it.GetIndex());
      if(label==0) //interior
        {
        m++;
        }
      else
        {
        if(label==hotSourceLabel)
          {
          numHotSource++;
          }
        }
      }
    }
  cout<<"Problem dimension: "<<m<<" x "<<n<<endl;
  cout<<"num hot: "<<numHotSource<<endl;

  typedef itk::Image<int, 3> ImageIndexMap;
  ImageIndexMap::Pointer matrixIndex = ImageIndexMap::New();
  Allocate<CharImage,ImageIndexMap>(domain, matrixIndex);
  std::vector<Voxel> imageIndex(n);
  int i1(0),i2(m); //index is the image pixel index, i1 and i2 are matrix indices
  for(it.GoToBegin(); !it.IsAtEnd(); ++it)
    {
    Voxel voxel = it.GetIndex();
    if(it.Get()>0) //in domain
      {
      LabelType label = sourceMap->GetPixel(voxel);
      if(label==0) //interior
        {
        imageIndex[i1] = voxel;
        matrixIndex->SetPixel(voxel, i1);
        i1++;
        }
      else
        {
        assert(i2<n);
        imageIndex[i2] = voxel;
        matrixIndex->SetPixel(voxel,i2);
        i2++;
        }
      }
    else
      {
      matrixIndex->SetPixel(voxel,-1);//set to invalid value;
      }
    }
  assert(i1==m);
  assert(i2==n);

  typedef Eigen::SparseMatrix<float> SpMat;
  SpMat A(m,m);
  SpMat B(m,n-m);
    {
    typedef Eigen::Triplet<float> SpMatEntry;
    std::vector<SpMatEntry> entryA, entryB;
    for(int i=0; i<m; i++)
      {
      float Aii(0);
      for(int ii =0; ii<6; ii++)
        {
        Voxel voxel = imageIndex[i]+offsets[ii];
        if(imDomain.IsInside(voxel) && domain->GetPixel(voxel)!=0)
          {
          int j = matrixIndex->GetPixel(voxel);
          if (j<m)
            {
            entryA.push_back( SpMatEntry(i,j,-1));
            }
          else
            {
            entryB.push_back( SpMatEntry(i,j-m,-1));
            }
          Aii+=1.0;
          }
        }
      entryA.push_back(SpMatEntry(i,i,Aii));
      }
    A.setFromTriplets(entryA.begin(),entryA.end());
    B.setFromTriplets(entryB.begin(),entryB.end());
    }

  Eigen::VectorXf xB(n-m);
  int numOnes=0;
  for(int i=0,i0=m; i<n-m; i++,i0++)
    {
    Voxel voxel = imageIndex[i0];
    LabelType label = sourceMap->GetPixel(voxel);
    xB[i] = label==hotSourceLabel? 1.0 : 0.0;
    numOnes += xB[i]==1.0;
    }
  assert(numOnes==numHotSource);

  Eigen::VectorXf b(m);
    {
    b = B*xB;
    b*=-1.0;
    }

  // Solving:
  Eigen::VectorXf xI(m);
  vtkNew<vtkTimerLog> timer;
  timer->StartTimer();

  if(0)
    {
    }
  else
    {
    try
      {
      xI = Solve(A,b);
      }
    catch(std::bad_alloc)
      {
      cout << "Cholesky runs out of memory, switch to conjugate gradient instead\n";
      Eigen::ConjugateGradient<SpMat> solver(A);
      xI= solver.solve(b);
      }
    }

  timer->StopTimer();
  std::cout << "Simple heat diffusion of size "<<m<<", solve time: " << timer->GetElapsedTime() << std::endl;

  //set the interior pixels by xI
  for(int i=0; i<m; i++)
    {
    Voxel voxel = imageIndex[i];
    heat->SetPixel(voxel, xI[i]);
    }
  //set the boundary pixels by xB
  for(int i=m; i<n; i++)
    {
    heat->SetPixel(imageIndex[i],xB[i-m]);
    }

}

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
  for(PixelVector::iterator i = bd.begin(); i!=bd.end();i++)
    {
    if(labelMap->GetPixel(*i)>=foreGroundMin)
      {
      domain->SetPixel(*i,seedLabel);
      regionSize++;
      }
    }
  for(int dist=2; dist<=distance; dist++)
    {
    PixelVector newBd;
    for(PixelVector::iterator i = bd.begin(); i!=bd.end();i++)
      {
      Voxel pIndex = *i;
      for(int iOff=0; iOff<6; iOff++)
        {
        const VoxelOffset& offset = offsets[iOff];
        Voxel qIndex = pIndex + offset;
        if(      allRegion.IsInside(qIndex)
              && labelMap->GetPixel(qIndex)>=foreGroundMin
              && domain->GetPixel(qIndex)==0)
          {
          regionSize++;
          newBd.push_back(qIndex);
          domain->SetPixel(qIndex,domainLabel);
          }
        }
      }
    bd = newBd;
    }
  return regionSize;
}

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
  for(int dist=2; dist<=distance; dist++)
    {
    PixelVector newBd;
    for(PixelVector::iterator i = bd.begin(); i!=bd.end();i++)
      {
      Voxel pIndex = *i;
      for(int iOff=0; iOff<6; iOff++)
        {
        const VoxelOffset& offset = offsets[iOff];
        Voxel qIndex = pIndex + offset;
        if(allRegion.IsInside(qIndex)
           && labelMap->GetPixel(qIndex)>=forGroundMin
           && domain->GetPixel(qIndex)==0)
          {
          regionSize++;
          newBd.push_back(qIndex);
          domain->SetPixel(qIndex,domainLabel);
          }
        }
      }
    bd = newBd;
    }
  return regionSize;
}


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

  void Init(const char* fname, bool invertY)
  {
    vtkNew<vtkPolyDataReader> reader;
    reader->SetFileName(fname);
    reader->Update();
    this->InitSkeleton(reader->GetOutput(), invertY);

    this->InitBones();
  }
private:
  void InitSkeleton(vtkPolyData* armPoly, bool invertY)
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

      if(invertY)
        {
        ax[1]*=-1;
        bx[1]*=-1;
        }

      this->SkeletonVoxels.push_back(std::vector<Voxel>());
      std::vector<Voxel>& edgeVoxels(this->SkeletonVoxels.back());
      Rasterize<LabelImage>(ax,bx,this->BodyPartition, edgeVoxels);

      //XXX we really need to make sure that the rasterized edge is a connected component
      //just a hack here
      for(size_t i=0; i<edgeVoxels.size()-2; i++)
        {
        edgeVoxels[i] = edgeVoxels[i+1];
        }
      edgeVoxels.pop_back();
      edgeVoxels.pop_back();
      CharType label = ArmatureType::GetEdgeLabel(edgeId);
      int numOutside(0);
      for(std::vector<Voxel>::iterator vi = edgeVoxels.begin(); vi!=edgeVoxels.end(); vi++)
        {
        if(this->BodyMap->GetPixel(*vi)!=0 && this->BodyPartition->GetPixel(*vi)==0)
          {
          this->BodyPartition->SetPixel(*vi, label);
          }
        else
          {
          if(this->BodyMap->GetPixel(*vi)==0)
            {
            numOutside++;
            }
          }
        }
      if(numOutside>0)
        {
        cout<<"WARNING: armature edge "<<edgeId<<" has "<<numOutside<<" outside voxels out of "<<edgeVoxels.size()<<endl;
        }

      if(this->SkeletonVoxels.back().size()<2)
        {
        cout<<"WARNING: edge "<<edgeId<<" is very small"<<endl;
        }
      edgeId++;
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
      BonePartition = LabelImage::New();
      Allocate<LabelImage,LabelImage>(this->BodyMap,BonePartition);
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
      for(std::vector<Voxel>::iterator i=boneSeeds.begin();i!=boneSeeds.end();i++)
        {
        assert((*i)!=invalidVoxel);
        }

      //compute a map from the old to the new labels:
      //  newLabels[oldLabel] is the new label
      std::vector<LabelType> newLabels(numBones+1,0);
      for(std::vector<Voxel>::iterator seedIter=boneSeeds.begin();seedIter!=boneSeeds.end();seedIter++)
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
          numVisited++;
          CharType edgeLabel = this->BodyPartition->GetPixel(p);
          regionSize[static_cast<size_t>(edgeLabel)]++;
          visited->SetPixel(p,true);
          for(int i=0; i<6; i++)
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
        for(size_t i=0; i<regionSize.size();i++)
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
          cout<<"Visited: "<<numVisited<<endl;
          cout<<"Edges for bone: "<<seedLabel<<" ";
          for(size_t i=0; i<regionSize.size();i++)
            {
            if(regionSize[i]!=0)
              {
              cout<<i<<" ";
              }
            }
          cout<<endl;
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

      for(int i=0; i<this->GetNumberOfEdges(); i++)
        {
        LabelType edgeLabel = static_cast<LabelType>(this->GetEdgeLabel(i));
        if(std::find(newLabels.begin(), newLabels.end(),edgeLabel)==newLabels.end())
          {
          cout<<"No bones belong to edge "<<i<<" with label "<<edgeLabel<<endl;
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
        componentSize[boneIter.Get()]++;
        }
      int totalSize=0;
      for(size_t i=0;i<componentSize.size();i++)
        {
        totalSize+=componentSize[i];
        cout<<i<<": "<<componentSize[i]<<"\n";
        }
      }

  }
};


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
        regionSize++;
        }
      }
    cout<<"Voronoi region size: "<<regionSize<<endl;
    assert(NumConnectedComponents<CharImage>(this->Domain)==1);

    LabelType unknown = ArmatureType::GetEdgeLabel(-1);
    regionSize+=Expand(this->Armature.BodyPartition, this->GetLabel(), unknown, expansionDistance,
                       this->Domain,ArmatureType::DomainLabel);

    typedef itk::ImageFileWriter<CharImage> WriterType;
    WriterType::Pointer writer = WriterType::New();
    writer->SetFileName( "./region.mha");
    writer->SetInput(this->Domain);
    writer->SetUseCompression(1);
    writer->Update();

    //Debug
    cout<<"Region size after expanding "<<expansionDistance<<" : "<<regionSize<<endl;

    Voxel bbMin, bbMax;
    for(int i=0; i<3; i++)
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
      for(int i=0; i<3; i++)
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
    cout<<"Domain bounding box: "<<bbMin<<" "<<bbMax<<endl;
  }

  WeightImage::Pointer ComputeWeight(bool binaryWeight, int smoothingIterations)
  {
    cout<<"Compute weight for edge "<<this->Id<<" with label "<<(int)this->GetLabel()<<endl;
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
        numBackground++;
        }
      }
    cout<<numBackground<<" background voxel"<<endl;

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
      DiffuseHeat(this->Domain, this->Armature.BonePartition, this->GetLabel(), weight);
      BodyDiffusionRegion diffusionRegion(this->Armature.BodyMap,this->Armature.BonePartition);
      DiffuseHeatIteratively<WeightImage>(weight,diffusionRegion, smoothingIterations);
      }

    return weight;
  }
  CharType GetLabel() const{ return this->Armature.GetEdgeLabel(this->Id);}
private:
  const ArmatureType& Armature;
  int Id;
  CharImage::Pointer Domain;
  Region ROI;
};

inline int NumDigits(unsigned int a)
{
  int numDigits(0);
  while(a>0)
    {
    a = a/10;
    numDigits++;
    }
  return numDigits;
}

inline string ToFixedWidthNumber(unsigned int number, int n)
{
  stringstream str;
  unsigned int a = number;
  for(int i=0; i<n; i++, a/=10)
    {
    unsigned int b = a%10;
    str<<b;
    }
  string astr = str.str();
  std::reverse(astr.begin(),astr.end());
  return astr;

}

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
      for(int iOff=0; iOff<6; iOff++)
        {
        Voxel q = p + offsets[iOff];
        if( region.IsInside(q) && labelMap->GetPixel(q)>0)
          {
          numNeighbors++;
          }
        }
      if(numNeighbors==0)
        {
        cerr<<"Paint isolated voxel "<<p<<" to background"<<endl;
        labelMap->SetPixel(p, 0);
        }
      }
    }
}

int main( int argc, char * argv[] )
{
  PARSE_ARGS;

  if(BinaryWeight)
    {
    cout<<"Use binary weight: "<<endl;
    }

  if(InvertY)
    {
    cout<<"Y coordinate of the armature will be inverted"<<endl;
    }

  if(DumpDebugImages)
    {
    DumpSegmentationImages = true;
    }

  //----------------------------
  // Read label map
  //----------------------------
  typedef itk::ImageFileReader<LabelImage>  ReaderType;
  typename ReaderType::Pointer labelMapReader = ReaderType::New();
  labelMapReader->SetFileName(RestLabelmap.c_str() );
  labelMapReader->Update();

  typedef itk::StatisticsImageFilter<LabelImage>  StatisticsType;
  typename StatisticsType::Pointer statistics = StatisticsType::New();
  statistics->SetInput( labelMapReader->GetOutput() );
  statistics->Update();
  //----------------------------
  // Print out some statistics
  //----------------------------
  typename LabelImage::Pointer labelMap = labelMapReader->GetOutput();
  Region allRegion = labelMap->GetLargestPossibleRegion();
  itk::ImageRegionIteratorWithIndex<LabelImage> it(labelMap,labelMap->GetLargestPossibleRegion());
  LabelType bodyIntensity = 1;
  LabelType boneIntensity = 209; //marrow
  int numBodyVoxels(0),numBoneVoxels(0);
  for(it.GoToBegin(); !it.IsAtEnd(); ++it)
    {
    if(it.Get()>bodyIntensity)
      {
      numBodyVoxels++;
      }
    if(it.Get()>boneIntensity)
      {
      numBoneVoxels++;
      }
    }
  int totalVoxels = allRegion.GetSize()[0]*allRegion.GetSize()[1]*allRegion.GetSize()[2];

  cout<<"Image statistics\n";
  cout<<"  min: "<<(int)statistics->GetMinimum()<<" max: "<<(int)statistics->GetMaximum()<<endl;
  cout<<"  total # voxels  : "<<totalVoxels<<endl;
  cout<<"  num body voxels : "<<numBodyVoxels<<endl;
  cout<<"  num bone voxels : "<<numBoneVoxels<<endl;

  RemoveSingleVoxelIsland(labelMap);

  //----------------------------
  // Read armature information
  //----------------------------
  ArmatureType armature(labelMap);
  armature.Init(ArmaturePoly.c_str(),InvertY);

  if (LastEdge<0)
    {
    LastEdge = armature.GetNumberOfEdges()-1;
    }
  cout<<"Process armature edge from "<<FirstEdge<<" to "<<LastEdge<<endl;

  //--------------------------------------------
  // Compute the domain of reach armature part
  //--------------------------------------------
  int numDigits = NumDigits(armature.GetNumberOfEdges());
  for(int i=FirstEdge; i<=LastEdge; i++)
    {
    ArmatureEdge edge(armature,i);
    cout<<"Process armature edge "<<i<<" with label "<<(int)edge.GetLabel()<<endl;
    edge.Initialize(BinaryWeight? 0 : ExpansionDistance);
    WeightImage::Pointer weight = edge.ComputeWeight(BinaryWeight,SmoothingIteration);
    std::stringstream filename;
    filename<<WeightDirectory<<"/weight_"<<ToFixedWidthNumber(i,numDigits)<<".mha";
    WriteImage<WeightImage>(weight,filename.str().c_str());
    }

  return EXIT_SUCCESS;
}
