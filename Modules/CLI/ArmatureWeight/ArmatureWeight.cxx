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
#include "ArmatureWeightCLP.h"
#include "ArmatureWeightWriter.h"
#include <benderIOUtils.h>

// ITK includes
#include <itkBinaryThresholdImageFilter.h>
#include <itkIndex.h>
#include <itkImage.h>
#include <itkMaskImageFilter.h>
#include <itkMultiThreader.h>
#include <itkStatisticsImageFilter.h>
#include <itkSimpleFastMutexLock.h>
#include <itkPluginUtilities.h>
#include <itkImageRegionIteratorWithIndex.h>

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
#if defined(__WIN32__) || defined(WIN32)

#include <windows.h>

inline void delay( unsigned long ms )
  {
  Sleep( ms );
  }

#else  /* presume POSIX */

#include <unistd.h>

inline void delay( unsigned long ms )
  {
  usleep( ms * 1000 );
  }

#endif

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

} // end namepaces

//-------------------------------------------------------------------------------
static int NumberOfRunningThreads = 0;
static itk::SimpleFastMutexLock mutex;

//-------------------------------------------------------------------------------
ITK_THREAD_RETURN_TYPE ThreaderCallback(void* arg)
{
  typedef itk::MultiThreader::ThreadInfoStruct  ThreadInfoType;
  ThreadInfoType * infoStruct = static_cast< ThreadInfoType* >( arg );
  if (!infoStruct)
    {
    std::cerr<<"Could not find appropriate arguments. Stopping."<<std::endl;

    mutex.Lock();
    --NumberOfRunningThreads;
    mutex.Unlock();
    return ThreadInfoType::ITK_PROCESS_ABORTED_EXCEPTION;
    }

  ArmatureWeightWriter* writer =
    static_cast< ArmatureWeightWriter* >( infoStruct->UserData );
  if (!writer)
    {
    std::cerr<<"Could not find weight writer. Stopping."<<std::endl;

    mutex.Lock();
    --NumberOfRunningThreads;
    mutex.Unlock();
    return ThreadInfoType::ITK_PROCESS_ABORTED_EXCEPTION;
    }

  // Compute weight
  if (! writer->Write())
    {
    std::cerr<<"There was a problem while trying to write the weight."
      <<" Stopping"<<std::endl;

    mutex.Lock();
    --NumberOfRunningThreads;
    mutex.Unlock();
    return ThreadInfoType::ITK_PROCESS_ABORTED_EXCEPTION;
    }

  mutex.Lock();
  --NumberOfRunningThreads;
  mutex.Unlock();
  return ThreadInfoType::SUCCESS;
}

//-------------------------------------------------------------------------------
int main( int argc, char * argv[] )
{
  PARSE_ARGS;

  if(BinaryWeight)
    {
    std::cout << "Use binary weight: " << std::endl;
    }
  if(RunSequential)
    {
    std::cout << "Running Sequential: " << std::endl;
    }

  bender::IOUtils::FilterStart("Read inputs");
  bender::IOUtils::FilterProgress("Read inputs", 0.01, 0.1, 0.0);

  //----------------------------
  // Read label map
  //----------------------------
  typedef itk::ImageFileReader<LabelImageType>  ReaderType;
  ReaderType::Pointer bodyPartitionReader = ReaderType::New();
  bodyPartitionReader->SetFileName(BodyPartition.c_str() );
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
    bodyReader->Update();;
    }
  catch (itk::ExceptionObject &e)
    {
    std::cerr<<"Could not read body, got error: "<<std::endl;
    e.Print(std::cout);
    return EXIT_FAILURE;
    }

  bender::IOUtils::FilterProgress("Read inputs", 0.50, 0.1, 0.0);

  vtkSmartPointer<vtkPolyDataReader> polyDataReader =
    vtkSmartPointer<vtkPolyDataReader>::New();
  polyDataReader->SetFileName(ArmaturePoly.c_str());
  polyDataReader->Update();

  if (! polyDataReader->GetOutput())
    {
    std::cerr<<"Could not read the poly data given: "
      << ArmaturePoly <<". Stopping."<<std::endl;
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
  statistics->SetInput( bodyPartitionReader->GetOutput() );
  statistics->Update();

  bender::IOUtils::FilterProgress("Read inputs", 0.75, 0.1, 0.0);

  LabelImageType::PixelType minLabel = statistics->GetMinimum();
  LabelImageType::PixelType maxLabel = statistics->GetMaximum();

  bender::IOUtils::FilterEnd("Read inputs");

  //--------------------------------------------
  // Compute the bone partition
  //--------------------------------------------

  bender::IOUtils::FilterStart("Compute Bones Partition");

  LabelImageType::Pointer bonesPartition =
    SimpleBoneSegmentation(bodyReader->GetOutput(),
      bodyPartitionReader->GetOutput());
  if (Debug)
    {
    bender::IOUtils::WriteImage<LabelImageType>(bonesPartition,
      "./DEBUG_BonesPartition.mha");
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
  typedef itk::MultiThreader ThreaderType;
  ThreaderType::Pointer threader = ThreaderType::New();
  std::vector<ArmatureWeightWriter*> writers; // Memory management

  std::cout<<"Compute from edge #"<<FirstEdge<<" to edge #"<<LastEdge
    <<" (Processing in parrallel ? "<<! RunSequential<<" )"<<std::endl;

  for(int i = FirstEdge; i <= LastEdge; ++i)
    {
    // Wait if too many thread started
    while (NumberOfRunningThreads >= threader->GetNumberOfThreads())
      {
      delay(50);
      }

    ArmatureWeightWriter* writeWeight = ArmatureWeightWriter::New();
    // Inputs
    writeWeight->SetBodyPartition(bodyPartitionReader->GetOutput());
    writeWeight->SetArmature(polyDataReader->GetOutput());
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
    writeWeight->SetDebugInfo(Debug);

    std::cout<<"Start Weight computation for edge #"<<i<<std::endl;
    if (! RunSequential)
      {
      ++NumberOfRunningThreads;
      threader->SpawnThread(ThreaderCallback, writeWeight);
      }
    else
      {
      if (! writeWeight->Write())
        {
        std::cerr<<"There was a problem while trying to write the weight."
          <<" Stopping"<<std::endl;
        }
      }

    writers.push_back(writeWeight); //Memory manangement
    }

  // Wait for all the threads to finish
  while (NumberOfRunningThreads != 0)
    {
    delay(50);
    }

  // Delete all the writers
  for(std::vector<ArmatureWeightWriter*>::iterator it = writers.begin();
    it != writers.end(); ++it)
    {
    (*it)->Delete();
    }

  bender::IOUtils::FilterEnd("Compute weights");

  return EXIT_SUCCESS;
}
