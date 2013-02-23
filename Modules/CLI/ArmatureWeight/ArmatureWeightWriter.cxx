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
#include "ArmatureWeightWriter.h"
#include "SolveHeatDiffusionProblem.h"
#include <benderIOUtils.h>

// ITK includes
#include <itkAddImageFilter.h>
#include <itkBinaryThresholdImageFilter.h>
#include <itkImage.h>
#include <itkImageFileWriter.h>
#include <itkImageRegionIteratorWithIndex.h>
#include <itkIndex.h>
#include <itkLabelGeometryImageFilter.h>
#include <itkLinearInterpolateImageFunction.h>
#include <itkMath.h>
#include <itkNearestNeighborInterpolateImageFunction.h>
#include <itkResampleImageFilter.h>

// VTK includes
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkDoubleArray.h>
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
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
  out->CopyInformation(in);
  out->SetRegions(in->GetLargestPossibleRegion());
  out->Allocate();
}

//-----------------------------------------------------------------------------
template <class ImageType, class InterpolatorType> typename ImageType::Pointer
ResampleImage(typename ImageType::Pointer inputImage,
              typename ImageType::SpacingType newSpacing,
              typename InterpolatorType::Pointer interpolator)
{
  typedef itk::ResampleImageFilter<ImageType, ImageType> ResampleFilterType;
  ResampleFilterType::Pointer resample = ResampleFilterType::New();
  resample->SetInput( inputImage );

  resample->SetInterpolator(interpolator);

  const typename ImageType::SpacingType& inputSpacing =
    inputImage->GetSpacing();
  ImageType::SpacingType outSpacing;
  for( unsigned int i = 0; i < 3; i++ )
    {
    outSpacing[i] = newSpacing[i];
    if( outSpacing[i] == 0.0 )
      {
      outSpacing[i] = inputSpacing[i];
      }
    }

  const typename ImageType::SizeType& inputSize =
    inputImage->GetLargestPossibleRegion().GetSize();
  typename ImageType::SizeType outSize;

  typedef typename ImageType::SizeType::SizeValueType SizeValueType;
  outSize[0] = static_cast<SizeValueType>(
    inputSize[0] * inputSpacing[0] / newSpacing[0] + .5);
  outSize[1] = static_cast<SizeValueType>(
    inputSize[1] * inputSpacing[1] / newSpacing[1] + .5);
  outSize[2] = static_cast<SizeValueType>(
    inputSize[2] * inputSpacing[2] / newSpacing[2] + .5);

  resample->SetOutputOrigin( inputImage->GetOrigin() );
  resample->SetOutputSpacing( outSpacing );
  resample->SetOutputDirection( inputImage->GetDirection() );
  resample->SetSize( outSize );
  resample->Update();

  return resample->GetOutput();
}

//-----------------------------------------------------------------------------
template <class ImageType> typename ImageType::Pointer
DownsampleImage(typename ImageType::Pointer inputImage,
              typename ImageType::SpacingType newSpacing)
{
  typedef itk::NearestNeighborInterpolateImageFunction<ImageType> InterpolatorType;
  InterpolatorType::Pointer interpolator = InterpolatorType::New();

  return ResampleImage<ImageType, InterpolatorType>(
    inputImage, newSpacing, interpolator);
}

//-----------------------------------------------------------------------------
template <class ImageType> typename ImageType::Pointer
UpsampleImage(typename ImageType::Pointer inputImage,
              typename ImageType::SpacingType newSpacing)
{
  typedef itk::LinearInterpolateImageFunction<ImageType> InterpolatorType;
  InterpolatorType::Pointer interpolator = InterpolatorType::New();

  return ResampleImage<ImageType, InterpolatorType>(
    inputImage, newSpacing, interpolator);
}

} // end namespace


vtkStandardNewMacro(ArmatureWeightWriter);

//-----------------------------------------------------------------------------
ArmatureWeightWriter::ArmatureWeightWriter()
{
  // Input images and polydata
  this->Armature = 0;
  this->BodyPartition = 0;
  this->BonesPartition = 0;
  this->Id = 0;
  this->Filename = "./Weight";
  this->BinaryWeight = false;
  this->SmoothingIterations = 10;
  this->Debug = false;
  this->WeightComputationSpacing = 5.0;
}

//-----------------------------------------------------------------------------
ArmatureWeightWriter::~ArmatureWeightWriter()
{
  if (this->Armature)
    {
    this->Armature->Delete();
    }
}

//-----------------------------------------------------------------------------
void ArmatureWeightWriter::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "Id: " << this->Id << "\n";
  os << indent << "Filename: " << this->Filename << "\n";
  os << indent << "NumDigits: " << this->NumDigits << "\n";
  os << indent << "Binary: " << this->BinaryWeight << "\n";
  os << indent << "Smoothing Iterations" << this->SmoothingIterations << "\n";
  os << indent << "Debug: " << this->Debug << "\n";
  os << indent << "Domain: " << this->Domain << "\n";
  os << indent << "ROI: " << this->ROI << "\n";
  os << indent << "WeightComputationSpacing: "
    << this->WeightComputationSpacing << "\n";
}

//-----------------------------------------------------------------------------
void ArmatureWeightWriter::SetArmature(vtkPolyData* arm)
{
  if (arm == this->Armature)
    {
    return;
    }

  arm->Register(this);
  if (this->Armature)
    {
    this->Armature->Delete();
    }
  this->Armature = arm;
  this->Modified();
}

//-----------------------------------------------------------------------------
void ArmatureWeightWriter::SetBodyPartition(LabelImageType::Pointer partition)
{
  if (this->BodyPartition == partition)
    {
    return;
    }

  this->BodyPartition = partition;
  this->Modified();
}

//-----------------------------------------------------------------------------
LabelImageType::Pointer ArmatureWeightWriter::GetBodyPartition()
{
  return this->BodyPartition;
}

//-----------------------------------------------------------------------------
void ArmatureWeightWriter::SetBones(LabelImageType::Pointer bones)
{
  if (this->BonesPartition == bones)
    {
    return;
    }

  this->BonesPartition = bones;
  this->Modified();
}

//-----------------------------------------------------------------------------
LabelImageType::Pointer ArmatureWeightWriter::GetBones()
{
  return this->BonesPartition;
}

//-----------------------------------------------------------------------------
void ArmatureWeightWriter::SetFilename(std::string name)
{
  if (name == this->Filename)
    {
    return;
    }

  this->Filename = name;
  this->Modified();
}

//-----------------------------------------------------------------------------
std::string ArmatureWeightWriter::GetFilename()
{
  return this->Filename;
}

//-----------------------------------------------------------------------------
void ArmatureWeightWriter::SetId(EdgeType id)
{
  if (id == this->Id)
    {
    return;
    }

  this->Id = id;
  this->Modified();
}

//-----------------------------------------------------------------------------
EdgeType ArmatureWeightWriter::GetId()const
{
  return this->Id;
}

//-----------------------------------------------------------------------------
bool ArmatureWeightWriter::Write()
{
  LabelImageType::SpacingType originalImageSpacing;
  for (int i = 0; i < 3; ++i)
    {
    originalImageSpacing = this->BodyPartition->GetSpacing()[i];

    LabelImageType::SpacingValueType spacing =
      this->BonesPartition->GetSpacing()[i];
    if (fabs(spacing - originalImageSpacing[i]))
      {
      std::cerr<<"Error, the Bones and the Body partition "
        "do not have the same spacing"<<std::endl;
      return false;
      }
    }

  LabelImageType::Pointer downSampledBodyPartition;
  LabelImageType::Pointer downSampledBonesPartition;
  if (this->BinaryWeight) // no need to downsample
    {
    downSampledBodyPartition = this->BodyPartition;
    downSampledBonesPartition = this->BonesPartition;
    }
  else
    {
    downSampledBodyPartition = DownsampleImage<LabelImageType>(
      this->BodyPartition, this->WeightComputationSpacing);

    downSampledBonesPartition = DownsampleImage<LabelImageType>(
      this->BonesPartition, this->WeightComputationSpacing);
    }

  // Compute weight
  CharImageType::Pointer domain =
    this->CreateDomain(downSampledBodyPartition, downSampledBonesPartition);
  if (! domain)
    {
    std::cerr<<"Could not initialize edge correctly. Stopping."<<std::endl;
    return false;
    }

  WeightImageType::Pointer downSampledWeight =
    this->CreateWeight(domain, downSampledBodyPartition, downSampledBonesPartition);

  if (! this->BinaryWeight)
    {
    bender::IOUtils::WriteImage<WeightImageType>(
      downSampledWeight, this->Filename.c_str());
    }
  else
    {
    WeightImageType::Pointer weight =
      UpsampleImage<WeightImageType>(downSampledWeight, originalImageSpacing);
    bender::IOUtils::WriteImage<WeightImageType>(weight, this->Filename.c_str());
    }

  return true;
}

//-----------------------------------------------------------------------------
CharImageType::Pointer ArmatureWeightWriter
::CreateDomain(LabelImageType::Pointer bodyPartition,
               LabelImageType::Pointer bonesPartition)
{
  if (! this->Armature)
    {
    std::cerr<< "Could not initialize domain, armature is NULL" <<std::endl;
    return false;
    }

  vtkPoints* points = this->Armature->GetPoints();
  vtkDoubleArray* radiuses = vtkDoubleArray::SafeDownCast(
    this->Armature->GetCellData()->GetArray("EnvelopeRadiuses"));
  if (!points || !radiuses)
    {
    std::cerr<< "Could not initialize domain, armature point "
      << "and/or envelope radiuses are null"<<std::endl;
    return false;
    }

  std::cout<<"Initalizing computation region for edge #"
    <<this->Id<<std::endl;

  double head[3], tail[3];
  points->GetPoint(this->Id * 2, head);
  points->GetPoint(this->Id * 2 + 1, tail);

  // Convert to IJK

  double radius = radiuses->GetValue(this->Id);
  double squareRadius = radius * radius;

  double cylinderCenterLine[3];
  vtkMath::Subtract(tail, head, cylinderCenterLine);
  double cylinderLength = vtkMath::Normalize(cylinderCenterLine);

  CharImageType::Pointer domain = CharImageType::New();
  Allocate<LabelImageType, CharImageType>(bodyPartition, domain);

  // Expand the region based on the envelope and the bodypartition
  CharType edgeLabel = this->GetLabel();

  // Scan through Domain and BodyPartition at the same time. (Same size)
  itk::ImageRegionIteratorWithIndex<CharImageType> domainIt(
    domain, domain->GetLargestPossibleRegion());
  itk::ImageRegionIteratorWithIndex<LabelImageType> bodyPartitionIt(
    bodyPartition,bodyPartition->GetLargestPossibleRegion());
  for (domainIt.GoToBegin(); !domainIt.IsAtEnd();
    ++domainIt, ++bodyPartitionIt)
    {
    // Most likely/simple operation done first to prevent overhead

    LabelType label = bodyPartitionIt.Get();
    if (label == ArmatureWeightWriter::BackgroundLabel) // Is it backgtound ?
      {
      domainIt.Set(ArmatureWeightWriter::BackgroundLabel);
      }
    else // Not background pixel
      {
      if (label == edgeLabel) // Correct label, no need to go further
        {
        domainIt.Set(ArmatureWeightWriter::DomainLabel);
        continue;
        }

      //
      // Check if in envelope

      // Create world position
      double pos[3];
      for (int i = 0; i < 3; ++i)
        {
        pos[i] = domainIt.GetIndex()[i] * domain->GetSpacing()[i]
          + domain->GetOrigin()[i];
        }

      // Is the current pixel in the sphere around head ?
      double headToPos[3];
      vtkMath::Subtract(pos, head, headToPos);
      if (vtkMath::Dot(headToPos, headToPos) <= squareRadius)
        {
        domainIt.Set(ArmatureWeightWriter::DomainLabel);
        continue;
        }

      // Is the current pixel the sphere around tail ?
      double tailToPos[3];
      vtkMath::Subtract(pos, tail, tailToPos);
      if (vtkMath::Dot(tailToPos, tailToPos) <= squareRadius)
        {
        domainIt.Set(ArmatureWeightWriter::DomainLabel);
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
          domainIt.Set(ArmatureWeightWriter::DomainLabel);
          continue;
          }
        }

      domainIt.Set(ArmatureWeightWriter::BackgroundLabel); // Wasn't in the envelope
      }
    }

  // Debug
  if (this->GetDebugInfo())
    {
    std::stringstream filename;
    filename<< this->Filename << "_region" << this->Id << ".mha";
    bender::IOUtils::WriteImage<CharImageType>(domain,
      filename.str().c_str());
    }

  return domain;
}

//-----------------------------------------------------------------------------
WeightImageType::Pointer ArmatureWeightWriter
::CreateWeight(CharImageType::Pointer domain,
               LabelImageType::Pointer bodyPartition,
               LabelImageType::Pointer bonesPartition)
{
  if (this->GetDebugInfo())
    {
    std::cout << "Compute weight for edge "<< this->Id
              <<" with label "
              << static_cast<int>(this->GetLabel()) << std::endl;
    }

  // Attribute -1.0 to outside of the body, 0 inside.
  typedef itk::BinaryThresholdImageFilter<LabelImageType, WeightImageType>
    ThresholdFilterType;
  ThresholdFilterType::Pointer threshold = ThresholdFilterType::New();
  threshold->SetInput(bodyPartition);
  threshold->SetLowerThreshold(ArmatureWeightWriter::DomainLabel);
  threshold->SetInsideValue(0.0f);
  threshold->SetOutsideValue(-1.0f);
  threshold->Update();

  WeightImageType::Pointer weight = threshold->GetOutput();

  if (this->BinaryWeight)
    {
    // Domain is 0 everywhere expect on the edge region where it's 1.
    // Weight is 0 in the body and -1 outside.
    // Adding the two gives:
    // -1 outside, 0 in the (body  and NOT Domain) and 1 in (body  and Domain)

    typedef itk::AddImageFilter<WeightImageType, CharImageType> AddFilterType;
    AddFilterType::Pointer add = AddFilterType::New();
    add->SetInput1(weight);
    add->SetInput2(domain);
    add->Update();
    weight = add->GetOutput();
    }
  else
    {
    std::cout<<"Solve localized version of the problem for edge #"<<this->Id<<std::endl;

    //First solve a localized verison of the problme exactly
    LocalizedBodyHeatDiffusionProblem localizedProblem(
      domain, bonesPartition, this->GetLabel());
    SolveHeatDiffusionProblem<WeightImageType>::Solve(localizedProblem, weight);

    std::cout<<"Solve global solution problem for edge #"<<this->Id<<std::endl;

    //Approximate the global solution by iterative solving
    GlobalBodyHeatDiffusionProblem globalProblem(
      bodyPartition, bonesPartition);
    SolveHeatDiffusionProblem<WeightImageType>::SolveIteratively(
      globalProblem,weight, this->SmoothingIterations);
    }

  return weight;
}

//-----------------------------------------------------------------------------
CharType ArmatureWeightWriter::GetLabel() const
{
  // 0 is background, 1 is body interior so Armature Edge must start with 2
  return static_cast<CharType>(this->Id + ArmatureWeightWriter::EdgeLabels);
}
