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
#include "ComputeArmatureWeightCLP.h"
#include "ArmatureWeightThreader.h"
#include "ArmatureWeightWriter.h"
#include <benderIOUtils.h>

// ITK includes
#include <itkBinaryBallStructuringElement.h>
#include <itkBinaryThresholdImageFilter.h>
#include <itkGrayscaleDilateImageFilter.h>
#include <itkIndex.h>
#include <itkImage.h>
#include <itkMaskImageFilter.h>
#include <itkMultiThreader.h>
#include <itkStatisticsImageFilter.h>
#include <itkSimpleFastMutexLock.h>
#include <itkPluginUtilities.h>
#include <itkImageRegionIteratorWithIndex.h>
#include <itksys/SystemTools.hxx>

// STD includes
#include <sstream>
#include <vector>
#include <iostream>
#include <iomanip>

// VTK includes
#include <vtkPolyData.h>
#include <vtkPolyDataReader.h>
#include <vtkSmartPointer.h>

typedef itk::Image<unsigned short, 3>  LabelImageType;
typedef itk::Image<unsigned char, 3>  CharImageType;

typedef itk::Image<WeightImagePixelType, 3>  WeightImageType;

typedef itk::Index<3> VoxelType;
typedef itk::Offset<3> VoxelOffsetType;
typedef itk::ImageRegion<3> RegionType;

namespace
{

//-------------------------------------------------------------------------------
inline int NumDigits(unsigned int a)
{
  int numDigits = 0;
  while(a>0)
    {
    a = a/10;
    ++numDigits;
    }
  return numDigits;
}

//-------------------------------------------------------------------------------
// \todo Move this segmentation to its own CLI
LabelImageType::Pointer SimpleBoneSegmentation(
  LabelImageType::Pointer body,
  LabelImageType::Pointer bodyPartition)
{
  // Select the bones and label them by componnets
  // \todo not needed if the threshold is done manually when boneInside is used
  typedef itk::BinaryThresholdImageFilter<LabelImageType, CharImageType>
    ThresholdFilterType;
  ThresholdFilterType::Pointer threshold = ThresholdFilterType::New();
  threshold->SetInput(body);
  threshold->SetLowerThreshold(209); //bone marrow
  threshold->SetInsideValue(ArmatureWeightWriter::DomainLabel);
  threshold->SetOutsideValue(ArmatureWeightWriter::BackgroundLabel);

  typedef itk::MaskImageFilter<LabelImageType, CharImageType> MaskFilterType;
  MaskFilterType::Pointer mask = MaskFilterType::New();
  mask->SetInput1(bodyPartition);
  mask->SetInput2(threshold->GetOutput());
  mask->Update();
  return mask->GetOutput();
}

//-------------------------------------------------------------------------------
//Expand the foreground once in place.
// The new foreground pixels are assigned foreGroundMin and the number of pixel
// pushed is returned
template <class ImageType> int ExpandForegroundOnce(
  typename ImageType::Pointer labelMap,
  typename ImageType::PixelType foreGroundMin)
{
  typedef typename ImageType::IndexType VoxelType;
  typedef std::pair<VoxelType, typename ImageType::PixelType> ImagePixelType;

  int numNewVoxels=0;
  typename ImageType::RegionType region = labelMap->GetLargestPossibleRegion();
  itk::ImageRegionIteratorWithIndex<ImageType> it(labelMap,region);
  Neighborhood<3> neighbors;

  typename ImageType::OffsetType* offsets = neighbors.Offsets;

  std::vector<ImagePixelType> front;
  for(it.GoToBegin(); !it.IsAtEnd(); ++it)
    {
    VoxelType p = it.GetIndex();
    if(it.Get() >= foreGroundMin)
      {
      for(int iOff=0; iOff<6; ++iOff)
        {
        const typename ImageType::OffsetType& offset = offsets[iOff];
        VoxelType q = p + offset;
        if(region.IsInside(q) && labelMap->GetPixel(q) < foreGroundMin)
          {
          front.push_back(std::make_pair(q, labelMap->GetPixel(p)));
          }
        }
      }
    }

  for (typename std::vector<ImagePixelType>::const_iterator i = front.begin();
       i!=front.end(); i++)
    {
    if(labelMap->GetPixel(i->first) < foreGroundMin)
      {
      labelMap->SetPixel(i->first, i->second);
      ++numNewVoxels;
      }
    }

  return numNewVoxels;
}

} // end namespace

//-------------------------------------------------------------------------------
static ArmatureWeightThreader ThreadHandler;

//-------------------------------------------------------------------------------
ITK_THREAD_RETURN_TYPE ThreaderCallback(void* arg)
{
  typedef itk::MultiThreader::ThreadInfoStruct  ThreadInfoType;
  ThreadInfoType * infoStruct = reinterpret_cast< ThreadInfoType* >( arg );

  ArmatureWeightWriter* writer =
    reinterpret_cast< ArmatureWeightWriter* >( infoStruct->UserData );
  if (!writer)
    {
    writer->Delete();
    ThreadHandler.Fail(infoStruct->ThreadID,
      "Could not find weight writer. Stopping.");
    return ITK_THREAD_RETURN_VALUE;
    }

  // Compute weight
  if (! writer->Write())
    {
    writer->Delete();
    ThreadHandler.Fail(infoStruct->ThreadID,
      "There was a problem while trying to write the weight. Stopping.");
    return ITK_THREAD_RETURN_VALUE;
    }

  writer->Delete();
  ThreadHandler.Success(infoStruct->ThreadID);
  return ITK_THREAD_RETURN_VALUE;
}

//-------------------------------------------------------------------------------
int main( int argc, char * argv[] )
{
  PARSE_ARGS;

  if(!IsArmatureInRAS)
    {
    std::cout << "Input armature is not in RAS coordinate system;"
      << "will convert it to RAS." << std::endl;
    }
  if(BinaryWeight)
    {
    std::cout << "Use binary weight: " << std::endl;
    }
  if(RunSequential)
    {
    std::cout << "Running Sequential: " << std::endl;
    }
  ThreadHandler.ClearThreads();

  bender::IOUtils::FilterStart("Read inputs");
  bender::IOUtils::FilterProgress("Read inputs", 0.01, 0.1, 0.0);

  //----------------------------------
  // Check output folder
  //----------------------------------
  if (!itksys::SystemTools::FileExists(WeightDirectory.c_str()))
    {
    std::cerr<<" Cannot find directory named: "<<WeightDirectory
      <<"."<<std::endl << "Make sure it is a valid directory."<<std::endl;
    return EXIT_FAILURE;
    }

  std::string debugDir = WeightDirectory;
  if (Debug)
    {
    debugDir += "/Debug/";
    }

  //----------------------------
  // Read label map
  //----------------------------
  typedef itk::ImageFileReader<LabelImageType>  ReaderType;
  ReaderType::Pointer bodyPartitionReader = ReaderType::New();
  bodyPartitionReader->SetFileName(SkinnedVolume.c_str() );
  try
    {
    bodyPartitionReader->Update();
    }
  catch (itk::ExceptionObject &e)
    {
    std::cerr<<"Could not read body partition, got error: "<<std::endl;
    e.Print(std::cout);
    return EXIT_FAILURE;
    }

  bender::IOUtils::FilterProgress("Read inputs", 0.25, 0.1, 0.0);

  ReaderType::Pointer bodyReader = ReaderType::New();
  bodyReader->SetFileName(RestLabelmap.c_str() );
  try
    {
    bodyReader->Update();
    }
  catch (itk::ExceptionObject &e)
    {
    std::cerr<<"Could not read body, got error: "<<std::endl;
    e.Print(std::cout);
    return EXIT_FAILURE;
    }

  bender::IOUtils::FilterProgress("Read inputs", 0.50, 0.1, 0.0);

  vtkSmartPointer<vtkPolyData> armaturePolyData;
  armaturePolyData.TakeReference(
    bender::IOUtils::ReadPolyData(ArmaturePoly.c_str(), !IsArmatureInRAS));
  if (!armaturePolyData)
    {
    std::cerr << "Can't read armature " << ArmaturePoly << std::endl;
    return EXIT_FAILURE;
    }

  //----------------------------
  // Get some statistics
  //----------------------------

  // \todo Look for all the labels in the image.
  // and Make sure there are 3 distinct type of label (background, unknown, bone[])
  // \todo Be able to process non-continous arrays of lables [1, 3, 4 ...]
  // \todo Define backgound and unknow label values

  typedef itk::StatisticsImageFilter<LabelImageType>  StatisticsType;
  StatisticsType::Pointer statistics = StatisticsType::New();
  itk::PluginFilterWatcher watchStatistics(statistics,
                                       "Get Statistics",
                                       CLPProcessInformation);
  statistics->SetInput( bodyPartitionReader->GetOutput() );
  statistics->Update();

  bender::IOUtils::FilterProgress("Read inputs", 0.75, 0.1, 0.0);

  //LabelImageType::PixelType minLabel = statistics->GetMinimum();
  LabelImageType::PixelType maxLabel = statistics->GetMaximum();

  bender::IOUtils::FilterEnd("Read inputs");

  //--------------------------------------------
  // Dilate the body partition
  //--------------------------------------------

  bender::IOUtils::FilterStart("Dilate body partition");

  LabelImageType::Pointer dilatedBodyPartition =
    bodyPartitionReader->GetOutput();
  bender::IOUtils::FilterProgress("Dilate body partition", 0.25, 1.0, 0.0);

  int numPaddedVoxels =0;
  for(int i = 0; i < Padding; i++)
    {
    numPaddedVoxels += ExpandForegroundOnce<LabelImageType>(
      dilatedBodyPartition,
      static_cast<LabelImageType::PixelType>(ArmatureWeightWriter::DomainLabel));
    std::cout<<"Padded "<<numPaddedVoxels<<" voxels"<<std::endl;

    bender::IOUtils::FilterProgress(
      "Dilate body partition", 0.75, 1.0 / Padding, 0.25);
    }

  if (Debug)
    {
    bender::IOUtils::WriteDebugImage<LabelImageType>(
      dilatedBodyPartition, "DEBUG_DilatedBodyPartition.mha", debugDir.c_str());
    }

  bender::IOUtils::FilterEnd("Dilate body partition");

  //--------------------------------------------
  // Compute the bone partition
  //--------------------------------------------

  bender::IOUtils::FilterStart("Compute Bones Partition");

  LabelImageType::Pointer bonesPartition =
    SimpleBoneSegmentation(bodyReader->GetOutput(),
      dilatedBodyPartition);
  if (Debug)
    {
    bender::IOUtils::WriteDebugImage<LabelImageType>(
      bonesPartition, "DEBUG_BonesPartition.mha", debugDir.c_str());
    }

  bender::IOUtils::FilterEnd("Compute Bones Partition");


  //--------------------------------------------
  // Compute the domain of reach Armature Edge part
  //--------------------------------------------

  bender::IOUtils::FilterStart("Compute weights");
  bender::IOUtils::FilterProgress("Compute weights", 0.01, 0.99, 0.1);

  if (LastEdge < 0)
    {
    LastEdge = maxLabel - 2;
    }

  int numDigits = NumDigits(maxLabel);

  // Compute the weight of each bones in a separate thread
  std::cout << "Compute from edge #" << FirstEdge << " to edge #" << LastEdge
            << " (Processing in parrallel ? " << !RunSequential<<" )"
            << std::endl;

  for(int i = FirstEdge; i <= LastEdge; ++i)
    {
    std::cout << "Setup edge #" << i << std::endl;
    if (CLPProcessInformation)
      {
      if (CLPProcessInformation->Abort)
        {
        ThreadHandler.KillAll();
        std::cout << "Aborted" << std::endl;
        return EXIT_FAILURE;
        }
      if (!RunSequential)
        {
        if (ThreadHandler.HasError())
          {
          std::cout << "Error:" << std::endl;
          ThreadHandler.PrintErrors();
          return EXIT_FAILURE;
          }
        }
      }

    ArmatureWeightWriter* writeWeight = ArmatureWeightWriter::New();

    // Inputs
    writeWeight->SetBodyPartition(dilatedBodyPartition);
    writeWeight->SetArmature(armaturePolyData);
    writeWeight->SetBones(bonesPartition);
    // Output filename
    std::stringstream filename;
    filename << WeightDirectory << "/weight_"
      << std::setfill('0') << std::setw(numDigits) << i << ".mha";
    writeWeight->SetFilename(filename.str());

    // Edge Id
    writeWeight->SetId(i);

    // Others
    writeWeight->SetBinaryWeight(BinaryWeight);
    writeWeight->SetSmoothingIterations(SmoothingIteration);
    writeWeight->SetScaleFactor(ScaleFactor);
    writeWeight->SetUseEnvelopes(UseEnvelopes);
    writeWeight->SetDebugInfo(Debug);
    std::stringstream debugFolder;
    debugFolder << debugDir << "/weight_"
      << std::setfill('0') << std::setw(numDigits) << i << "_DEBUG";
    writeWeight->SetDebugFolder(debugFolder.str());

    std::cout<<"Start Weight computation for edge #"<<i<<std::endl;
    if (! RunSequential)
      {
      ThreadHandler.AddThread(ThreaderCallback, writeWeight);
      }
    else
      {
      if (! writeWeight->Write())
        {
        std::cerr<<"There was a problem while trying to write the weight."
          <<" Stopping"<<std::endl;
        }

      writeWeight->Delete();
      }
    }

  // Wait for all the threads to finish.
  if (!RunSequential)
    {
    while (ThreadHandler.GetNumberOfRunningThreads() != 0)
      {
      if (CLPProcessInformation)
        {
        if (CLPProcessInformation->Abort)
          {
          ThreadHandler.KillAll();
          return EXIT_FAILURE;
          }
        if (ThreadHandler.HasError())
          {
          ThreadHandler.PrintErrors();
          return EXIT_FAILURE;
          }
        }
      itksys::SystemTools::Delay(100);
      }
    }

  bender::IOUtils::FilterEnd("Compute weights");
  return EXIT_SUCCESS;
}
