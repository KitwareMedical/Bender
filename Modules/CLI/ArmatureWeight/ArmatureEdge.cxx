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
#include <itkAddImageFilter.h>
#include <itkBinaryThresholdImageFilter.h>
#include <itkLabelGeometryImageFilter.h>
#include <itkImage.h>
#include <itkImageFileWriter.h>
#include <itkImageRegionIteratorWithIndex.h>
#include <itkIndex.h>
#include <itkMath.h>

// VTK includes
#include <vtkCellArray.h>
#include <vtkDoubleArray.h>
#include <vtkCellData.h>
#include <vtkNew.h>
#include <vtkMath.h>
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
void ConvertToIJK(double x[3])
{
  x[0] = -x[0];
  x[1] = -x[1];
  x[2] = x[2];
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
  this->Domain = 0;
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
bool ArmatureEdge::Initialize(vtkPolyData* armature)
{
  if (! armature)
    {
    std::cerr<< "Could not initialize domain, armature is NULL" <<std::endl;
    return false;
    }

  vtkPoints* points = armature->GetPoints();
  vtkDoubleArray* radiuses = vtkDoubleArray::SafeDownCast(
    armature->GetCellData()->GetArray("EnvelopeRadiuses"));
  if (!points || !radiuses)
    {
    std::cerr<< "Could not initialize domain, armature point "
      << "and/or envelope radiuses are null"<<std::endl;
    return false;
    }

  double head[3], tail[3];
  points->GetPoint(this->Id * 2, head);
  points->GetPoint(this->Id * 2 + 1, tail);

  // Convert to IJK
  ConvertToIJK(head);
  ConvertToIJK(tail);

  double radius = radiuses->GetValue(this->Id);
  double squareRadius = radius * radius;

  double cylinderCenterLine[3];
  vtkMath::Subtract(tail, head, cylinderCenterLine);
  double cylinderLength = vtkMath::Normalize(cylinderCenterLine);

  this->Domain = CharImageType::New();
  Allocate<LabelImageType, CharImageType>(this->BodyPartition, this->Domain);

  // Expand the region based on the envelope and the bodypartition
  CharType edgeLabel = this->GetLabel();

  // Scan through Domain and BodyPartition at the same time. (Same size)
  itk::ImageRegionIteratorWithIndex<CharImageType> domainIt(
    this->Domain, this->Domain->GetLargestPossibleRegion());
  itk::ImageRegionIteratorWithIndex<LabelImageType> bodyPartitionIt(
    this->BodyPartition, this->BodyPartition->GetLargestPossibleRegion());
  for (domainIt.GoToBegin(); !domainIt.IsAtEnd();
    ++domainIt, ++bodyPartitionIt)
    {
    // Most likely/simple operation done first to prevent overhead

    LabelType label = bodyPartitionIt.Get();
    if (label == ArmatureEdge::BackgroundLabel) // Is it backgtound ?
      {
      domainIt.Set(ArmatureEdge::BackgroundLabel);
      }
    else // Not background pixel
      {
      if (label == edgeLabel) // Correct label, no need to go further
        {
        domainIt.Set(ArmatureEdge::DomainLabel);
        continue;
        }

      //
      // Check if in envelope

      // Create world position
      double pos[3];
      for (int i = 0; i < 3; ++i)
        {
        pos[i] = domainIt.GetIndex()[i] * this->Domain->GetSpacing()[i]
          + this->Domain->GetOrigin()[i];
        }

      // Is the current pixel in the sphere around head ?
      double headToPos[3];
      vtkMath::Subtract(pos, head, headToPos);
      if (vtkMath::Dot(headToPos, headToPos) <= squareRadius)
        {
        domainIt.Set(ArmatureEdge::DomainLabel);
        continue;
        }

      // Is the current pixel the sphere around tail ?
      double tailToPos[3];
      vtkMath::Subtract(pos, tail, tailToPos);
      if (vtkMath::Dot(tailToPos, tailToPos) <= squareRadius)
        {
        domainIt.Set(ArmatureEdge::DomainLabel);
        continue;
        }

      // Is  the current pixel in the cylinder ?
      double scale = vtkMath::Dot(cylinderCenterLine, headToPos);
      if (scale >= 0 && scale <= cylinderLength) // Check in between lids
        {
        // Check distance from center
        double distanceVect[3];
        for (int i = 0; i < 3; ++i)
          {
          distanceVect[i] = pos[i] - (head[i] + cylinderCenterLine[i] * scale);
          }

        if (vtkMath::Dot(distanceVect, distanceVect) <= squareRadius)
          {
          domainIt.Set(ArmatureEdge::DomainLabel);
          continue;
          }
        }

      domainIt.Set(ArmatureEdge::BackgroundLabel); // Wasn't in the envelope
      }
    }

  // Debug
  if (this->GetDebug())
    {
    std::stringstream filename;
    filename << "./region" << this->Id << ".mha";
    bender::IOUtils::WriteImage<CharImageType>(this->Domain,
      filename.str().c_str());
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

  // Attribute -1.0 to outside of the body, 0 inside.
  typedef itk::BinaryThresholdImageFilter<LabelImageType, WeightImageType>
    ThresholdFilterType;
  ThresholdFilterType::Pointer threshold = ThresholdFilterType::New();
  threshold->SetInput(this->BodyPartition);
  threshold->SetLowerThreshold(ArmatureEdge::DomainLabel);
  threshold->SetInsideValue(0.0f);
  threshold->SetOutsideValue(-1.0f);
  threshold->Update();
  WeightImageType::Pointer weight = threshold->GetOutput();

  if (binaryWeight)
    {
    // Domain is 0 everywhere expect on the edge region where it's 1.
    // Weight is 0 in the body and -1 outside.
    // Adding the two gives:
    // -1 outside, 0 in the (body  and NOT Domain) and 1 in (body  and Domain)

    typedef itk::AddImageFilter<WeightImageType, CharImageType> AddFilterType;
    AddFilterType::Pointer add = AddFilterType::New();
    add->SetInput1(weight);
    add->SetInput2(this->Domain);
    add->Update();
    weight = add->GetOutput();
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
