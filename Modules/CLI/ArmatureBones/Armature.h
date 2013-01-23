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

#ifndef __Armature_h
#define __Armature_h

// ITK includes
#include <itkImage.h>

// VTK includes
class vtkPolyData;

// STD includes
#include <vector>

typedef unsigned char CharType;
typedef unsigned short LabelType;
typedef unsigned int EdgeType;
typedef float WeightImagePixel;

typedef itk::Image<LabelType, 3>  LabelImageType;
typedef itk::Image<CharType, 3>  CharImageType;

typedef itk::Image<WeightImagePixel, 3>  WeightImage;

typedef itk::Index<3> Voxel;
typedef itk::Offset<3> VoxelOffsetType;
typedef itk::ImageRegion<3> RegionType;

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
class ArmatureType
{
public:

  // Enums the types of label used
  enum LabelTypes
    {
    BackgroundLabel = 0,
    DomainLabel,
    EdgeLabels
    };

  // Typedefs:
  // Pixels:
  typedef unsigned char CharType;
  typedef unsigned short LabelType;
  typedef unsigned int EdgeType;
  typedef float WeightImagePixel;
  // Images:
  typedef itk::Image<LabelType, 3>  LabelImageType;
  typedef itk::Image<CharType, 3>  CharImageType;
  typedef itk::Image<WeightImagePixel, 3>  WeightImage;
  // Others:
  typedef itk::Index<3> VoxelType;
  typedef itk::Offset<3> VoxelOffsetType;
  typedef itk::ImageRegion<3> RegionType;

  // Constructor
  ArmatureType(LabelImageType::Pointer image);

  // Label functions:
  CharType GetEdgeLabel(EdgeType edgeId) const;
  CharType GetMaxEdgeLabel() const;
  size_t GetNumberOfEdges() const;

  // Set/Get dump debug information
  void SetDebug(bool);
  bool GetDebug()const;

  // Create the body partition and the bones partition
  // from the armature file name.
  // Return success/failure.
  bool Init(vtkPolyData* armaturePolyData);

  // Get body/bones partition.
  // Should be called after Init() otherwise this will return empty volumes.
  LabelImageType::Pointer GetBodyPartition();
  LabelImageType::Pointer GetBonesPartition();

protected:

  LabelImageType::Pointer BodyMap; // Reference on the input volume
  LabelImageType::Pointer BodyPartition; //the partition of body by armature edges
  LabelImageType::Pointer BonesPartition; //the partition of bones by armature edges

  std::vector<std::vector<Voxel>> SkeletonVoxels;
  std::vector<typename CharImageType::Pointer> Domains;
  std::vector<Voxel> Fixed;
  std::vector<typename WeightImage::Pointer> Weights;

  bool InitSkeleton(vtkPolyData* armaturePolyData);
  bool InitBones();

  bool Debug;
};
#endif
