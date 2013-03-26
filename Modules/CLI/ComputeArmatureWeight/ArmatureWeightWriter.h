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

  void SetDebugFolder(std::string dir);
  std::string GetDebugFolder();

  void SetId(EdgeType id);
  EdgeType GetId() const;

  vtkSetMacro(ScaleFactor, double);
  vtkGetMacro(ScaleFactor, double);

  vtkSetMacro(UseEnvelopes, bool);
  vtkGetMacro(UseEnvelopes, bool);

  // Maximum parenthood distance prevent the heat diffusion to propagate
  // in regions associated with a bone related too far in the family tree.
  // Each bone has a distance of 1 with its direct parent and children.
  // For example, MaximumParenthoodDistance = 1 limits the heat diffusion
  // to the current bone, its children, and its parent. Default -1 means
  // no limitations.
  vtkSetMacro(MaximumParenthoodDistance, int);
  int GetMaximumParenthoodDistance() const;

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
  CharType GetLabel(EdgeType id) const;
  CharType GetLabel() const;
  EdgeType GetId(CharType label) const;

  // Create weight domain based on the armature
  // and the given body and bones partitions.
  // The returned image contains 1 (DomainLabel) at each voxel when the Id edge
  // has weight, 0 otherwise.
  // The domain is later used to compute the interpolated and diffused  weights.
  CharImageType::Pointer CreateDomain(
    const LabelImageType* bodyPartition,
    const LabelImageType* bonesPartition);

  // Create weight based on the domain
  // and the given body and bones partitions
  WeightImageType::Pointer CreateWeight(
    const CharImageType* domain,
    const LabelImageType* bodyPartition,
    const LabelImageType* bonesPartition);

  // "Mask" resampled image with the body partition
  // All the weight outside the body are marked off to -1.0
  // The bad weight ( < 0.0) or the weight in the area that belongs
  // to a bone too far in the family tree are attributed to
  // the proper value (-1.0 for outside and 0.0 inside).
  void CleanWeight(WeightImageType* weight,
    const LabelImageType* bodyPartition) const;

  // Uses Djikstra's algorithm to compute the map of distances
  // between the given edge and all the other edges.
  std::vector<int> GetParenthoodDistances(EdgeType boneID) const;

  // Creates a copy of the input image and restricts it to the
  // area within the maximum parenthood distance.
  // Any point outside this distance is assigned the BackgroundLabel
  // value. This basically acts as a custom mask filter.
  LabelImageType::Pointer ApplyDistanceMask(
    const LabelImageType* image,
    const std::vector<int>& distances) const;

  // Using the given weight image, restricts it to the area within the
  // maximum parenthood distance.
  // Any point outside this distance is assigned to -1.0f value.
  void ApplyDistanceMask(const LabelImageType* bodyPartition,
     WeightImageType::Pointer& weight,
     const std::vector<int>& distances) const;

  // Output necessary variables
  std::string Filename;
  int NumDigits;

  // Type of weight written
  bool BinaryWeight;
  int SmoothingIterations;
  double ScaleFactor;
  bool UseEnvelopes;

  // Debug info
  bool DebugInfo;
  std::string DebugFolder;

  int MaximumParenthoodDistance;

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
  LocalizedBodyHeatDiffusionProblem(const CharImageType* domain,
                                    const LabelImageType* sourceMap,
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
  CharImageType::ConstPointer Domain;   //a binary image that describes the domain
  LabelImageType::ConstPointer SourceMap; //a label image that defines the heat sources
  LabelType HotSourceLabel; //any source voxel with this label will be assigned weight 1

  RegionType WholeDomain;
};

//-------------------------------------------------------------------------------
class GlobalBodyHeatDiffusionProblem: public HeatDiffusionProblem<3>
{
public:
  GlobalBodyHeatDiffusionProblem(
    const LabelImageType* body, const LabelImageType* bones)
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
  LabelImageType::ConstPointer Body;
  LabelImageType::ConstPointer Bones;
};

#endif
