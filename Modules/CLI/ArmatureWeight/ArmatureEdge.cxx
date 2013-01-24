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
#include "ArmatureEdge.h"
#include "SolveHeatDiffusionProblem.h"
#include <benderIOUtils.h>

// ITK includes
#include <itkImage.h>
#include <itkImageFileWriter.h>
#include <itkStatisticsImageFilter.h>
#include <itkBinaryThresholdImageFilter.h>
#include <itkImageRegionIteratorWithIndex.h>
#include <itkMath.h>
#include <itkIndex.h>
#include <itkConnectedComponentImageFilter.h>

// VTK includes
#include <vtkNew.h>
#include <vtkCellArray.h>
#include <vtkTimerLog.h>

// STD includes
#include <iostream>
#include <iomanip>
#include <limits>
#include <vector>

typedef unsigned char CharType;
typedef unsigned short LabelType;
typedef unsigned int EdgeType;
typedef float WeightImagePixelType;

typedef itk::Image<LabelType, 3>  LabelImageType;
typedef itk::Image<CharType, 3>  CharImageType;

typedef itk::Image<WeightImagePixelType, 3>  WeightImageType;

typedef itk::Index<3> VoxelType;
typedef itk::Offset<3> VoxelOffsetType;
typedef itk::ImageRegion<3> RegionType;

namespace
{
//-----------------------------------------------------------------------------
template<class InImage, class OutImage>
void Allocate(typename InImage::Pointer in, typename OutImage::Pointer out)
{
  out->SetOrigin(in->GetOrigin());
  out->SetSpacing(in->GetSpacing());
  out->SetRegions(in->GetLargestPossibleRegion());
  out->Allocate();
}

//-----------------------------------------------------------------------------
template<class T>
inline int NumConnectedComponents(typename T::Pointer domain)
{
  typedef typename itk::Image<unsigned short, T::ImageDimension> OutImageType;
  typedef itk::ConnectedComponentImageFilter<T, OutImageType> ConnectedComponent;
  typename ConnectedComponent::Pointer connectedComponents
    = ConnectedComponent::New();
  connectedComponents->SetInput(domain);
  connectedComponents->SetBackgroundValue(0);
  connectedComponents->Update();
  return connectedComponents->GetObjectCount();
}

//-----------------------------------------------------------------------------
int Expand(const std::vector<VoxelType>& seeds,
           CharImageType::Pointer domain, int distance,
           LabelImageType::Pointer labelMap, unsigned short foreGroundMin,
           unsigned char seedLabel,
           unsigned char domainLabel)
{
  CharImageType::RegionType allRegion = labelMap->GetLargestPossibleRegion();
  Neighborhood<3> neighbors;
  VoxelOffsetType* offsets = neighbors.Offsets ;

  //grow by distance
  typedef std::vector<VoxelType> PixelVector;
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
      VoxelType pIndex = *i;
      for(int iOff=0; iOff<6; ++iOff)
        {
        const VoxelOffsetType& offset = offsets[iOff];
        VoxelType qIndex = pIndex + offset;
        if(allRegion.IsInside(qIndex)
            && labelMap->GetPixel(qIndex) >= foreGroundMin
            && domain->GetPixel(qIndex) ==0 )
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

//-----------------------------------------------------------------------------
int Expand(const LabelImageType::Pointer labelMap,
           LabelType label,
           LabelType forGroundMin,
           int distance, //input
           CharImageType::Pointer domain,  //output
           unsigned char domainLabel)
{
  RegionType allRegion = labelMap->GetLargestPossibleRegion();
  Neighborhood<3> neighbors;
  VoxelOffsetType* offsets = neighbors.Offsets ;

  //grow by distance
  typedef std::vector<VoxelType> PixelVector;
  PixelVector bd;
  int regionSize(0);
  itk::ImageRegionIteratorWithIndex<LabelImageType> it(labelMap,allRegion);
  for(it.GoToBegin(); !it.IsAtEnd(); ++it)
    {
    VoxelType p = it.GetIndex();
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
      VoxelType pIndex = *i;
      for(int iOff=0; iOff<6; ++iOff)
        {
        const VoxelOffsetType& offset = offsets[iOff];
        VoxelType qIndex = pIndex + offset;
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

} // end namespace

//-----------------------------------------------------------------------------
ArmatureEdge::ArmatureEdge(LabelImageType::Pointer bodyPartition,
                           LabelImageType::Pointer bonesPartition,
                           EdgeType id)
{
  this->BodyPartition = bodyPartition;
  this->BonesPartition = bonesPartition;
  this->Debug = false;
  this->Id = id;
  this->Domain = CharImageType::New();
  Allocate<LabelImageType, CharImageType>(bodyPartition, this->Domain);
  this->Domain->FillBuffer(ArmatureEdge::BackgroundLabel);
}

//-----------------------------------------------------------------------------
void ArmatureEdge::SetDebug(bool debug)
{
  this->Debug = debug;
}

//-----------------------------------------------------------------------------
bool ArmatureEdge::GetDebug()const
{
  return this->Debug;
}

//-----------------------------------------------------------------------------
bool ArmatureEdge::Initialize(int expansionDistance)
{
  RegionType imDomain = this->BodyPartition->GetLargestPossibleRegion();

  // Compute the "domain" of the armature edge
  // by expanding fixed distance around the Voronoi region
  itk::ImageRegionIteratorWithIndex<CharImageType> it(this->Domain, imDomain);
  size_t regionSize = 0;

  // Start with the Vorononi region
  CharType label = this->GetLabel();
  for (it.GoToBegin(); !it.IsAtEnd(); ++it)
    {
    // \todo Don't use GetIndex/GetPixel, use iterator instead.
    if (this->BodyPartition->GetPixel(it.GetIndex()) == label)
      {
      it.Set(ArmatureEdge::DomainLabel);
      ++regionSize;
      }
    }
  std::cout << "Voronoi region size: "<<regionSize << std::endl;

  int ncc = NumConnectedComponents<CharImageType>(this->Domain);
  if (ncc != 1)
    {
    std::cout<<"Number of Connected Components "
      << ncc <<"!= 1 in domain."<<std::endl;
    return false;
    }

  LabelType unknown = ArmatureEdge::DomainLabel;
  regionSize += Expand(this->BodyPartition, this->GetLabel(), unknown,
                       expansionDistance,
                       this->Domain, ArmatureEdge::DomainLabel);

  // Debug
  if (this->GetDebug())
    {
    bender::IOUtils::WriteImage<CharImageType>(this->Domain, "./region.mha");

    std::cout << "Region size after expanding " << expansionDistance
              << " : " << regionSize << std::endl;

    VoxelType bbMin, bbMax;
    for(int i=0; i<3; ++i)
      {
      bbMin[i] = imDomain.GetSize()[i]-1;
      bbMax[i] = 0;
      }
    for (it.GoToBegin(); !it.IsAtEnd(); ++it)
      {
      VoxelType p  = it.GetIndex();
      if (it.Get() == ArmatureEdge::BackgroundLabel)
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

  return true;
}

//-----------------------------------------------------------------------------
WeightImageType::Pointer ArmatureEdge
::ComputeWeight(bool binaryWeight, int smoothingIterations)
{
  if (this->GetDebug())
    {
    std::cout << "Compute weight for edge "<< this->Id
              <<" with label "
              << static_cast<int>(this->GetLabel()) << std::endl;
    }

  WeightImageType::Pointer weight = WeightImageType::New();
  Allocate<LabelImageType,WeightImageType>(this->BodyPartition, weight);

  size_t numBackground(0);
  itk::ImageRegionIteratorWithIndex<LabelImageType> it(
    this->BodyPartition, this->BodyPartition->GetLargestPossibleRegion());
  for (it.GoToBegin();!it.IsAtEnd(); ++it)
    {
    if(it.Get() != ArmatureEdge::BackgroundLabel)
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
    itk::ImageRegionIteratorWithIndex<CharImageType> domainIt(
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
      this->Domain, this->BonesPartition, this->GetLabel());
    SolveHeatDiffusionProblem<WeightImageType>::Solve(localizedProblem, weight);

    //Approximate the global solution by iterative solving
    GlobalBodyHeatDiffusionProblem globalProblem(
      this->BodyPartition, this->BonesPartition);
    SolveHeatDiffusionProblem<WeightImageType>::SolveIteratively(
      globalProblem,weight,smoothingIterations);
    }
  return weight;
}

//-----------------------------------------------------------------------------
CharType ArmatureEdge::GetLabel() const
{
  // 0 is background, 1 is body interior so Armature Edge must start with 2
  return static_cast<CharType>(this->Id + ArmatureEdge::EdgeLabels);
}
