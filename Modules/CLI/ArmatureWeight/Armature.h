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

typedef itk::Image<LabelType, 3>  LabelImage;
typedef itk::Image<CharType, 3>  CharImage;

typedef itk::Image<WeightImagePixel, 3>  WeightImage;

typedef itk::Index<3> Voxel;
typedef itk::Offset<3> VoxelOffset;
typedef itk::ImageRegion<3> Region;

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
  enum
  {
    BackgroundLabel = 0,
    DomainLabel,
    EdgeLabels
  };

  ArmatureType(LabelImage::Pointer image);

  static CharType GetEdgeLabel(EdgeType edgeId);

  CharType GetMaxEdgeLabel() const;
  size_t GetNumberOfEdges() const;

  void SetDebug(bool);
  bool GetDebug()const;

  void SetDumpDebugImages(bool);
  bool GetDumpDebugImages()const;

  void Init(const char* armatureFileName, bool invertXY);

  LabelImage::Pointer BodyMap;
  LabelImage::Pointer BodyPartition; //the partition of body by armature edges
  LabelImage::Pointer BonePartition; //the partition of bones by armature edges

  std::vector<std::vector<Voxel> > SkeletonVoxels;
  std::vector<CharImage::Pointer> Domains;
  std::vector<Voxel> Fixed;
  std::vector<WeightImage::Pointer> Weights;

protected:
  void InitSkeleton(vtkPolyData* armPoly);
  void InitBones();

  bool Debug;
  bool DumpDebugImages;
};

//-------------------------------------------------------------------------------
class ArmatureEdge
{
public:
  ArmatureEdge(const ArmatureType& armature, EdgeType id);

  void Initialize(int expansionDistance);

  WeightImage::Pointer ComputeWeight(bool binaryWeight, int smoothingIterations);
  CharType GetLabel() const;

  void SetDebug(bool);
  bool GetDebug()const;

  void SetDumpDebugImages(bool);
  bool GetDumpDebugImages()const;

protected:

  const ArmatureType& Armature;
  EdgeType Id;
  CharImage::Pointer Domain;
  Region ROI;

  bool Debug;
  bool DumpDebugImages;
};

#endif
