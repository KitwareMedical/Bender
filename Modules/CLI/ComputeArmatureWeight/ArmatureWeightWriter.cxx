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

// ComputeArmatureWeight includes
#include "ArmatureWeightWriter.h"
#include "SolveHeatDiffusionProblem.h"
#include <benderIOUtils.h>
#include "itkThresholdMedianImageFunction.h"

// ITK includes
#include <itkAddImageFilter.h>
#include <itkBinaryThresholdImageFilter.h>
#include <itkConnectedComponentImageFilter.h>
#include <itkImage.h>
#include <itkImageFileWriter.h>
#include <itkImageRegionIteratorWithIndex.h>
#include <itkIndex.h>
#include <itkLabelGeometryImageFilter.h>
#include <itkLinearInterpolateImageFunction.h>
#include <itkMath.h>
#include <itkResampleImageFilter.h>
#include <itkVotingBinaryHoleFillingImageFilter.h>

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
              double scaleFactor[3],
              typename InterpolatorType::Pointer interpolator)
{
  const typename ImageType::SizeType& inputSize =
    inputImage->GetLargestPossibleRegion().GetSize();
  typename ImageType::SizeType outSize;
  typedef typename ImageType::SizeType::SizeValueType SizeValueType;
  outSize[0] = static_cast<SizeValueType>(inputSize[0] / scaleFactor[0]);
  outSize[1] = static_cast<SizeValueType>(inputSize[1] / scaleFactor[1]);
  outSize[2] = static_cast<SizeValueType>(inputSize[2] / scaleFactor[2]);

  const typename ImageType::SpacingType& inputSpacing =
    inputImage->GetSpacing();
  typename ImageType::SpacingType outSpacing;
  outSpacing[0] = inputSpacing[0] * scaleFactor[0];
  outSpacing[1] = inputSpacing[1] * scaleFactor[1];
  outSpacing[2] = inputSpacing[2] * scaleFactor[2];

  double sign[3];
  sign[0] = inputImage->GetDirection()[0][0];
  sign[1] = inputImage->GetDirection()[1][1];
  sign[2] = inputImage->GetDirection()[2][2];
  const typename ImageType::PointType& inputOrigin =
    inputImage->GetOrigin();
  double outOrigin[3];
  outOrigin[0] = inputOrigin[0] + sign[0] * (outSpacing[0] - inputSpacing[0]) /2.;
  outOrigin[1] = inputOrigin[1] + sign[1] * (outSpacing[1] - inputSpacing[1]) /2.;
  outOrigin[2] = inputOrigin[2] + sign[2] * (outSpacing[2] - inputSpacing[2]) /2.;
  if (false)
    {
    std::cout << "ScaleFactor: " << scaleFactor[0] << "," << scaleFactor[1] << "," << scaleFactor[2] << std::endl;
    std::cout << "OutputOrigin: " << outOrigin[0] << "," << outOrigin[1] << "," << outOrigin[2] << std::endl;
    std::cout << "OutputSpacing: " << outSpacing[0] << "," << outSpacing[1] << "," << outSpacing[2] << std::endl;
    std::cout << "OutputSize: " << outSize[0] << "," << outSize[1] << "," << outSize[2] << std::endl;
    }

  typedef itk::ResampleImageFilter<ImageType, ImageType> ResampleFilterType;
  typename ResampleFilterType::Pointer resample = ResampleFilterType::New();
  resample->SetInput( inputImage );
  resample->SetInterpolator(interpolator);
  resample->SetOutputOrigin( outOrigin );
  resample->SetOutputSpacing( outSpacing );
  resample->SetOutputDirection( inputImage->GetDirection() );
  resample->SetSize( outSize );
  resample->Update();

  return resample->GetOutput();
}

//-----------------------------------------------------------------------------
template <class ImageType, class InterpolatorType> typename ImageType::Pointer
ResampleImage(typename ImageType::Pointer inputImage,
              double scaleFactor,
              typename InterpolatorType::Pointer interpolator)
{
  const typename ImageType::SizeType& inputSize =
    inputImage->GetLargestPossibleRegion().GetSize();
  typename ImageType::SizeType outSize;
  typedef typename ImageType::SizeType::SizeValueType SizeValueType;
  outSize[0] = static_cast<SizeValueType>(ceil(inputSize[0] / scaleFactor));
  outSize[1] = static_cast<SizeValueType>(ceil(inputSize[1] / scaleFactor));
  outSize[2] = static_cast<SizeValueType>(ceil(inputSize[2] / scaleFactor));

  double realScaleFactor[3];
  realScaleFactor[0] = static_cast<double>(inputSize[0]) / outSize[0];
  realScaleFactor[1] = static_cast<double>(inputSize[1]) / outSize[1];
  realScaleFactor[2] = static_cast<double>(inputSize[2]) / outSize[2];
  return ResampleImage<ImageType, InterpolatorType>(inputImage, realScaleFactor, interpolator);
}

//-----------------------------------------------------------------------------
template <class ImageType> typename ImageType::Pointer
DownsampleImage(typename ImageType::Pointer inputImage,
                double scaleFactor[3])
{
  //typedef itk::NearestNeighborInterpolateImageFunction<ImageType> InterpolatorType;
  typedef itk::ThresholdMedianImageFunction<ImageType> InterpolatorType;
  typename InterpolatorType::Pointer interpolator = InterpolatorType::New();
  interpolator->SetRejectPixel(ArmatureWeightWriter::BackgroundLabel);
  unsigned int radius = ceil(scaleFactor[0] / 2.);
  interpolator->SetNeighborhoodRadius( radius );

  return ResampleImage<ImageType, InterpolatorType>(
    inputImage, scaleFactor, interpolator);
}

//-----------------------------------------------------------------------------
template <class ImageType> typename ImageType::Pointer
UpsampleImage(typename ImageType::Pointer inputImage,
              double scaleFactor[3])
{
  typedef itk::LinearInterpolateImageFunction<ImageType> InterpolatorType;
  typename InterpolatorType::Pointer interpolator = InterpolatorType::New();

  double invertScaleFactor[3];
  invertScaleFactor[0] = 1. / scaleFactor[0];
  invertScaleFactor[1] = 1. / scaleFactor[1];
  invertScaleFactor[2] = 1. / scaleFactor[2];
  return ResampleImage<ImageType, InterpolatorType>(
    inputImage, invertScaleFactor, interpolator);
}

//-------------------------------------------------------------------------------
template <class ImageType> void
RemoveSingleVoxelIsland(typename ImageType::Pointer labelMap)
{
  typedef typename ImageType::IndexType VoxelType;

  Neighborhood<3> neighbors;
  const typename ImageType::OffsetType* offsets = neighbors.Offsets;

  typename ImageType::RegionType region = labelMap->GetLargestPossibleRegion();
  itk::ImageRegionIteratorWithIndex<ImageType> it(labelMap,region);
  for (it.GoToBegin(); !it.IsAtEnd(); ++it)
    {
    if (it.Get() == ArmatureWeightWriter::BackgroundLabel)
      {
      continue;
      }
    VoxelType p = it.GetIndex();
    int numNeighbors = 0;
    for (int iOff=0; iOff < 6; ++iOff)
      {
      VoxelType q = p + offsets[iOff];
      if ( region.IsInside(q) && labelMap->GetPixel(q) != ArmatureWeightWriter::BackgroundLabel)
        {
        ++numNeighbors;
        }
      }

    if (numNeighbors==0)
      {
      std::cout<<"Paint isolated voxel "<<p<<" to background" << std::endl;
      labelMap->SetPixel(p, ArmatureWeightWriter::BackgroundLabel);
      }
    }
}

//-------------------------------------------------------------------------------
template <class ImageType> void
RemoveVoxelIsland(typename ImageType::Pointer labelMap)
{
  // Identify all the connected components.
  //  _________
  // |   2     |
  // |_________|_
  //     0     |1|
  //           |_|
  typedef itk::Image<unsigned long, 3> ULongImageType;
  typedef itk::ConnectedComponentImageFilter<ImageType, ULongImageType> ConnectedFilter;
  typename ConnectedFilter::Pointer connectedFilter = ConnectedFilter::New();
  connectedFilter->SetInput(labelMap);
  connectedFilter->SetBackgroundValue(ArmatureWeightWriter::BackgroundLabel);
  connectedFilter->Update();
  ULongImageType::Pointer connectedImage = connectedFilter->GetOutput();

  if (connectedFilter->GetObjectCount() == 0)
    {
    return;
    }
  // Search the largest connected components
  // In the previous schema it was (2)
  std::vector<unsigned long> histogram(connectedFilter->GetObjectCount() + 1, 0);

  ULongImageType::RegionType connectedRegion = connectedImage->GetLargestPossibleRegion();
  itk::ImageRegionIteratorWithIndex<ULongImageType> connectedIt(connectedImage, connectedRegion);
  for (connectedIt.GoToBegin(); !connectedIt.IsAtEnd(); ++connectedIt)
    {
    if (connectedIt.Get() != ArmatureWeightWriter::BackgroundLabel)
      {
      ++histogram[connectedIt.Get()];
      }
    }
  ULongImageType::PixelType largestComponent =
    std::distance(histogram.begin(), std::max_element(histogram.begin(), histogram.end()));
  std::cout << "Histogram:" << histogram.size() << std::endl;
  for (size_t i = 0; i < histogram.size(); ++i)
    {
    std::cout << histogram[i] <<  ", ";
    }
  std::cout << std::endl << "Largest Component:" << static_cast<unsigned long>(largestComponent) << std::endl;

  // Only keep the largest connected components
  //  _________
  // |domainLbl|
  // |_________|
  //  backgrdLbl
  //
  typename ImageType::RegionType region = labelMap->GetLargestPossibleRegion();
  itk::ImageRegionIteratorWithIndex<ImageType> it(labelMap,region);
  for (it.GoToBegin(), connectedIt.GoToBegin(); !it.IsAtEnd(); ++it, ++connectedIt)
    {
    if (it.Get() == ArmatureWeightWriter::BackgroundLabel)
      {
      continue;
      }
    if (connectedIt.Get() != largestComponent)
      {
      it.Set(ArmatureWeightWriter::BackgroundLabel);
      }
    }
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
  this->DebugFilenamePrefix = "./DEBUG_";
  this->ScaleFactor = 2.0;
  this->UseEnvelopes = true;
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
  os << indent << "DebugFilenamePrefix: " << this->DebugFilenamePrefix << "\n";
  os << indent << "Domain: " << this->Domain << "\n";
  os << indent << "ROI: " << this->ROI << "\n";
  os << indent << "ScaleFactor: " << this->ScaleFactor << "\n";
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
void ArmatureWeightWriter::SetDebugFilenamePrefix(std::string prefix)
{
  if (prefix == this->DebugFilenamePrefix)
    {
    return;
    }

  this->DebugFilenamePrefix = prefix;
  this->Modified();
}

//-----------------------------------------------------------------------------
std::string ArmatureWeightWriter::GetDebugFilenamePrefix()
{
  return this->DebugFilenamePrefix;
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
  // Only downsample when not using weights
  bool downsample = !this->BinaryWeight && this->ScaleFactor != 1.0;

  const LabelImageType::SizeType& inputSize =
    this->BodyPartition->GetLargestPossibleRegion().GetSize();
  LabelImageType::SizeType outSize;
  typedef LabelImageType::SizeType::SizeValueType SizeValueType;
  outSize[0] = static_cast<SizeValueType>(ceil(inputSize[0] / this->ScaleFactor));
  outSize[1] = static_cast<SizeValueType>(ceil(inputSize[1] / this->ScaleFactor));
  outSize[2] = static_cast<SizeValueType>(ceil(inputSize[2] / this->ScaleFactor));

  double realScaleFactor[3];
  realScaleFactor[0] = static_cast<double>(inputSize[0]) / outSize[0];
  realScaleFactor[1] = static_cast<double>(inputSize[1]) / outSize[1];
  realScaleFactor[2] = static_cast<double>(inputSize[2]) / outSize[2];

  LabelImageType::Pointer downSampledBodyPartition;
  LabelImageType::Pointer downSampledBonesPartition;
  if (!downsample)
    {
    downSampledBodyPartition = this->BodyPartition;
    downSampledBonesPartition = this->BonesPartition;
    }
  else
    {
    downSampledBodyPartition = DownsampleImage<LabelImageType>(
      this->BodyPartition, realScaleFactor);

    downSampledBonesPartition = DownsampleImage<LabelImageType>(
      this->BonesPartition, realScaleFactor);
    }

  if ( downsample && this->GetDebugInfo() )
    {
    std::stringstream debugFilename;
    debugFilename << this->DebugFilenamePrefix
      << "_DownsampledBodyPartition.nrrd";
    bender::IOUtils::WriteImage<LabelImageType>(
      downSampledBodyPartition, debugFilename.str().c_str());

    debugFilename.str(std::string());
    debugFilename << this->DebugFilenamePrefix
      << "_DownsampledBonesPartition.nrrd";
    bender::IOUtils::WriteImage<LabelImageType>(
      downSampledBonesPartition, debugFilename.str().c_str());
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
  WeightImageType::Pointer weight;
  if (!downsample)
    {
    weight = downSampledWeight;
    }
  else
    {
    weight =
      UpsampleImage<WeightImageType>(downSampledWeight, realScaleFactor);
    }
  bender::IOUtils::WriteImage<WeightImageType>(
    weight, this->Filename.c_str());

  return true;
}

//-----------------------------------------------------------------------------
CharImageType::Pointer ArmatureWeightWriter
::CreateDomain(LabelImageType::Pointer bodyPartition,
               LabelImageType::Pointer bonesPartition)
{
  std::cout<<"Initalizing computation region for edge #"
    << this->Id << std::endl;

  if (! this->Armature)
    {
    std::cerr<< "Could not initialize domain, armature is NULL" <<std::endl;
    return 0;
    }

  vtkPoints* points = this->Armature->GetPoints();
  if (!points)
    {
    std::cerr << "Could not initialize domain, no armature point found."
              << std::endl;
    return 0;
    }

  vtkDoubleArray* radiuses = this->UseEnvelopes ?
    vtkDoubleArray::SafeDownCast(
      this->Armature->GetCellData()->GetArray("EnvelopeRadiuses")) : 0;
  double radius = radiuses ? radiuses->GetValue(this->Id) : 0;
  double squareRadius = radius * radius;
  if (!radiuses)
    {
    if (this->UseEnvelopes)
      {
      std::cerr << "WARNING: No envelopes found." << std::endl;
      }
    std::cout << "Not using envelopes." << std::endl;
    }

  double head[3], tail[3];
  points->GetPoint(this->Id * 2, head);
  points->GetPoint(this->Id * 2 + 1, tail);

  // Convert to IJK

  double cylinderCenterLine[3];
  vtkMath::Subtract(tail, head, cylinderCenterLine);
  double cylinderLength = vtkMath::Normalize(cylinderCenterLine);

  CharImageType::Pointer domain = CharImageType::New();
  Allocate<LabelImageType, CharImageType>(bodyPartition, domain);

  // Expand the region based on the bodypartition and optionally the envelopes.
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
      if (radiuses)
        {
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
        }

      domainIt.Set(ArmatureWeightWriter::BackgroundLabel); // Wasn't in the envelope
      }
    }

  if (this->GetDebugInfo())
    {
    std::stringstream debugFilename;
    debugFilename << this->DebugFilenamePrefix << "_region.nrrd";
    bender::IOUtils::WriteImage<CharImageType>(domain,
      debugFilename.str().c_str());
    }

  // Doesn't remove the case:
  //  ________
  // |        |
  // |________|__
  //          |__|
  //
  //RemoveSingleVoxelIsland<CharImageType>(domain);
  RemoveVoxelIsland<CharImageType>(domain);
//   Can't use it, it removes regions that are 1 slice thick
//   typedef itk::VotingBinaryHoleFillingImageFilter<CharImageType, CharImageType> VotingFilter;
//   VotingFilter::Pointer votingFilter = VotingFilter::New();
//   votingFilter->SetInput(domain);
//   votingFilter->SetBackgroundValue(ArmatureWeightWriter::DomainLabel);
//   votingFilter->SetForegroundValue(ArmatureWeightWriter::BackgroundLabel);
//   votingFilter->Update();
//   domain = votingFilter->GetOutput();

  if (this->GetDebugInfo())
    {
    std::stringstream debugFilename;
    debugFilename << this->DebugFilenamePrefix << "_region_cleaned.nrrd";
    bender::IOUtils::WriteImage<CharImageType>(domain,
      debugFilename.str().c_str());
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
    // Domain is 0 everywhere except on the edge region where it's 1.
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

    if ( this->GetDebugInfo() )
      {
      std::stringstream debugFilename;
      debugFilename << this->DebugFilenamePrefix  << "_localized.nrrd";
      bender::IOUtils::WriteImage<WeightImageType>(
        weight, debugFilename.str().c_str());
      }

    //Approximate the global solution by iterative solving
    GlobalBodyHeatDiffusionProblem globalProblem(
      bodyPartition, bonesPartition);
    SolveHeatDiffusionProblem<WeightImageType>::SolveIteratively(
      globalProblem,weight, this->SmoothingIterations);
    }

  if ( this->GetDebugInfo() )
    {
    std::stringstream debugFilename;
      debugFilename << this->DebugFilenamePrefix << "_global.nrrd";
    bender::IOUtils::WriteImage<WeightImageType>(
      weight, debugFilename.str().c_str());
    }

  return weight;
}

//-----------------------------------------------------------------------------
CharType ArmatureWeightWriter::GetLabel() const
{
  // 0 is background, 1 is body interior so Armature Edge must start with 2
  return static_cast<CharType>(this->Id + ArmatureWeightWriter::EdgeLabels);
}
