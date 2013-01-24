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
#include "ArmatureEdge.h"
#include <benderIOUtils.h>

// ITK includes
#include <itkImage.h>
#include <itkStatisticsImageFilter.h>
#include <itkPluginUtilities.h>
#include <itkImageRegionIteratorWithIndex.h>
#include <itkIndex.h>

// STD includes
#include <sstream>
#include <vector>
#include <iostream>
#include <iomanip>

typedef itk::Image<unsigned short, 3>  LabelImageType;
typedef itk::Image<unsigned char, 3>  CharImageType;

typedef itk::Image<WeightImagePixelType, 3>  WeightImageType;

typedef itk::Index<3> VoxelType;
typedef itk::Offset<3> VoxelOffsetType;
typedef itk::ImageRegion<3> RegionType;

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
int main( int argc, char * argv[] )
{
  PARSE_ARGS;

  if(BinaryWeight)
    {
    std::cout << "Use binary weight: " << std::endl;
    }

  bender::IOUtils::FilterStart("Read inputs");
  bender::IOUtils::FilterProgress("Read inputs", 0.01, 0.1, 0.0);

  //----------------------------
  // Read label map
  //----------------------------
  typedef itk::ImageFileReader<LabelImageType>  ReaderType;
  ReaderType::Pointer bodyPartitionReader = ReaderType::New();
  bodyPartitionReader->SetFileName(BodyPartition.c_str() );
  bodyPartitionReader->Update();

  bender::IOUtils::FilterProgress("Read inputs", 0.33, 0.1, 0.0);

  ReaderType::Pointer bonesPartitionReader = ReaderType::New();
  bonesPartitionReader->SetFileName(BonesPartition.c_str() );
  bonesPartitionReader->Update();

  bender::IOUtils::FilterProgress("Read inputs", 0.66, 0.1, 0.0);

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

  LabelImageType::PixelType minLabel = statistics->GetMinimum();
  LabelImageType::PixelType maxLabel = statistics->GetMaximum();

  bender::IOUtils::FilterEnd("Read inputs");

  //--------------------------------------------
  // Compute the domain of reach Armature Edge part
  //--------------------------------------------

  bender::IOUtils::FilterStart("Compute weights");
  bender::IOUtils::FilterProgress("Compute weights", 0.01, 0.99, 0.1);

  if (LastEdge < 0)
    {
    LastEdge = maxLabel - 1;
    }

  int numDigits = NumDigits(maxLabel);
  for(int i = FirstEdge; i <= LastEdge; ++i)
    {
    // Init edge
    ArmatureEdge edge(bodyPartitionReader->GetOutput(),
      bonesPartitionReader->GetOutput(), i);
    edge.SetDebug(Debug);

    std::cout << "Processing armature edge "<<i<<" with label "
      <<static_cast<int>(edge.GetLabel()) << std::endl;

    // Compute weight
    if (! edge.Initialize(BinaryWeight? 0 : ExpansionDistance))
      {
      std::cerr<<"Could not initialize edge "<< i <<" correctly."
        << " Stopping."<<std::endl;
      return EXIT_FAILURE;
      }

    WeightImageType::Pointer weight =
      edge.ComputeWeight(BinaryWeight, SmoothingIteration);

    // Write weight file
    std::stringstream filename;
    filename << WeightDirectory << "/weight_"
             << std::setfill('0') << std::setw(numDigits) << i << ".mha";
    bender::IOUtils::WriteImage<WeightImageType>(weight, filename.str().c_str());

    double progress =
      static_cast<double>(i - FirstEdge + 1) / (LastEdge-FirstEdge +1);
    bender::IOUtils::FilterProgress("Compute weights", progress, 0.99, 0.1);
    }
  bender::IOUtils::FilterEnd("Compute weights");

  return EXIT_SUCCESS;
}
