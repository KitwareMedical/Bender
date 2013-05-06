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

#ifndef __ArmatureEdge_h
#define __ArmatureEdge_h

// ITK includes
#include <itkImage.h>

// VTK includes
#include <vtkObject.h>

class vtkPolyData;

// STD includes
#include <vector>

// Bender includes
#include "HeatDiffusionProblem.h"

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
typedef itk::Point<float, 3> PointType;

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
// VTK object so it can register the armature
//
class ArmatureWeightWriter : public vtkObject
{
public:
  static ArmatureWeightWriter *New();

  vtkTypeMacro(ArmatureWeightWriter,vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent);

  // \todo should be defined by the ComputeArmatureWeight.cxx
  // Enums the types of label used
  enum LabelTypes
    {
    BackgroundLabel = 0,
    DomainLabel,
    EdgeLabels
    };

  // Set/Get methods.
  void SetArmature(vtkPolyData* armature);
  vtkGetObjectMacro(Armature, vtkPolyData);

  void SetBodyPartition(LabelImageType::Pointer partition);
  LabelImageType::Pointer GetBodyPartition();

  void SetBones(LabelImageType::Pointer bones);
  LabelImageType::Pointer GetBones();

  vtkSetMacro(SmoothingIterations, int);
  vtkGetMacro(SmoothingIterations, int);

  void SetFilename(std::string dir);
  std::string GetFilename();

  vtkSetMacro(BinaryWeight, bool);
  vtkGetMacro(BinaryWeight, bool);

  vtkSetMacro(DebugInfo, bool);
  vtkGetMacro(DebugInfo, bool);

  void SetId(EdgeType id);
  EdgeType GetId() const;

  vtkSetMacro(ScaleFactor, double);
  vtkGetMacro(ScaleFactor, double);

  // Computation methods
  bool Write();

protected:
  ArmatureWeightWriter();
  ~ArmatureWeightWriter();

  // Input images and polydata
  vtkPolyData* Armature;
  LabelImageType::Pointer BodyPartition;
  LabelImageType::Pointer BonesPartition;

  // Edge Id
  EdgeType Id;
  CharType GetLabel() const;

  // Create weight domain based on the armature
  // and the given body and bones partitions.
  // The returned image contains 1 (DomainLabel) at each voxel when the Id edge
  // has weight, 0 otherwise.
  // The domain is later used to compute the interpolated and diffused  weights.
  CharImageType::Pointer CreateDomain(
    LabelImageType::Pointer bodyPartition,
    LabelImageType::Pointer bonesPartition);

  // Create weight based on the domain
  // and the given body and bones partitions
  WeightImageType::Pointer CreateWeight(
    CharImageType::Pointer domain,
    LabelImageType::Pointer bodyPartition,
    LabelImageType::Pointer bonesPartition);

  // Output necessary variables
  std::string Filename;
  int NumDigits;

  // Type of weight written
  bool BinaryWeight;
  int SmoothingIterations;
  double ScaleFactor;

  // Debug info
  bool DebugInfo;

private:
  ArmatureWeightWriter(const ArmatureWeightWriter&);  //Not implemented
  void operator=(const ArmatureWeightWriter&);  //Not implemented

  CharImageType::Pointer Domain;
  RegionType ROI;
};

//-------------------------------------------------------------------------------
class LocalizedBodyHeatDiffusionProblem: public HeatDiffusionProblem<3>
{
public:
  LocalizedBodyHeatDiffusionProblem(CharImageType::Pointer domain,
                                    LabelImageType::Pointer sourceMap,
                                    LabelType hotSourceLabel)
    :Domain(domain),
    SourceMap(sourceMap),
    HotSourceLabel(hotSourceLabel)
    {
    this->WholeDomain = this->Domain->GetLargestPossibleRegion();
    }

  //Is the voxel inside the problem domain?
  bool InDomain(const VoxelType& voxel) const
    {
    return this->WholeDomain.IsInside(voxel)
      && this->Domain->GetPixel(voxel)!=0;
    }

  bool IsBoundary(const VoxelType& voxel) const
    {
    return this->SourceMap->GetPixel(voxel)!=0;
    }

  float GetBoundaryValue(const VoxelType& voxel) const
    {
    LabelType label = this->SourceMap->GetPixel(voxel);
    return label==this->HotSourceLabel? 1.0 : 0.0;
    }

private:
  CharImageType::Pointer Domain;   //a binary image that describes the domain
  LabelImageType::Pointer SourceMap; //a label image that defines the heat sources
  LabelType HotSourceLabel; //any source voxel with this label will be assigned weight 1

  RegionType WholeDomain;
};

//-------------------------------------------------------------------------------
class GlobalBodyHeatDiffusionProblem: public HeatDiffusionProblem<3>
{
public:
  GlobalBodyHeatDiffusionProblem(
    LabelImageType::Pointer body, LabelImageType::Pointer bones)
      :Body(body),Bones(bones)
  {
  }

  //Is the voxel inside the problem domain?
  bool InDomain(const VoxelType& voxel) const
    {
    return this->Body->GetLargestPossibleRegion().IsInside(voxel)
      && this->Body->GetPixel(voxel)!=0;
    }

  bool IsBoundary(const VoxelType& p) const
    {
    return this->Bones->GetPixel(p)>=2;
    }

  float GetBoundaryValue(const VoxelType&) const
    {
    assert(false); //not needed now
    return 0;
    }

private:
  LabelImageType::Pointer Body;
  LabelImageType::Pointer Bones;
};

#endif
