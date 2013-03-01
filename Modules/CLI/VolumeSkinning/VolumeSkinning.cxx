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
#include "VolumeSkinningCLP.h"
#include "Armature.h"
#include <benderIOUtils.h>

// ITK includes
#include <itkImage.h>
#include <itkStatisticsImageFilter.h>
#include <itkPluginUtilities.h>
#include <itkImageRegionIteratorWithIndex.h>
#include <itkIndex.h>

// STD includes
#include <vector>
#include <iostream>

typedef itk::Image<unsigned short, 3>  LabelImageType;
typedef itk::Image<unsigned char, 3>  CharImageType;

typedef itk::ImageRegion<3> RegionType;

//-------------------------------------------------------------------------------
int main( int argc, char * argv[] )
{
  PARSE_ARGS;

  if(!IsArmatureInRAS)
    {
    std::cout << "Input armature is not in RAS coordinate system; it will be "
              << "converted to RAS: X and Y coordinates will be flipped."
              << std::endl;
    }

  bender::IOUtils::FilterStart("Read inputs");
  bender::IOUtils::FilterProgress("Read inputs", 0.01, 0.33, 0.);

  //----------------------------
  // Read Inputs
  //----------------------------

  typedef itk::ImageFileReader<LabelImageType>  ReaderType;
  ReaderType::Pointer labelMapReader = ReaderType::New();
  labelMapReader->SetFileName(RestLabelmap.c_str() );
  labelMapReader->Update();
  LabelImageType::Pointer labelMap = labelMapReader->GetOutput();
  if (!labelMap)
    {
    std::cerr << "Can't read labelmap " << RestLabelmap << std::endl;
    return EXIT_FAILURE;
    }

  vtkPolyData* armaturePolyData =
    bender::IOUtils::ReadPolyData(ArmaturePoly.c_str(), !IsArmatureInRAS);
  if (!armaturePolyData)
    {
    std::cerr << "Can't read armature " << ArmaturePoly << std::endl;
    return EXIT_FAILURE;
    }

  bender::IOUtils::FilterProgress("Read inputs", 0.33, 0.1, 0.0);

  if (Debug)
    {
    //----------------------------
    // Print out some statistics
    //----------------------------

    typedef itk::StatisticsImageFilter<LabelImageType>  StatisticsType;
    StatisticsType::Pointer statistics = StatisticsType::New();
    statistics->SetInput( labelMapReader->GetOutput() );
    statistics->Update();

    RegionType allRegion = labelMap->GetLargestPossibleRegion();
    itk::ImageRegionIteratorWithIndex<LabelImageType> it(labelMap,
                                                         labelMap->GetLargestPossibleRegion());
    LabelType bodyIntensity = 1;
    LabelType boneIntensity = 209; //marrow
    int numBodyVoxels(0),numBoneVoxels(0);
    for(it.GoToBegin(); !it.IsAtEnd(); ++it)
      {
      if(it.Get()>=bodyIntensity)
        {
        ++numBodyVoxels;
        }
      if(it.Get()>=boneIntensity)
        {
        ++numBoneVoxels;
        }
      }
    int totalVoxels =
      allRegion.GetSize()[0]*allRegion.GetSize()[1]*allRegion.GetSize()[2];

    std::cout << "Image statistics\n";
    std::cout << "  min: "<<static_cast<int>(statistics->GetMinimum())
              <<" max: "<<static_cast<int>(statistics->GetMaximum()) << std::endl;
    std::cout << "  total # voxels  : "<<totalVoxels << std::endl;
    std::cout << "  num body voxels : "<<numBodyVoxels << std::endl;
    std::cout << "  num bone voxels : "<<numBoneVoxels << std::endl;
    }

  bender::IOUtils::FilterProgress("Read inputs", 0.99, 0.1, 0.0);
  bender::IOUtils::FilterEnd("Read inputs");
  bender::IOUtils::FilterStart("Segment bones");
  bender::IOUtils::FilterProgress("Segment bones", 0.01, 0.89, 0.1);

  //----------------------------
  // Read armature information
  //----------------------------

  ArmatureType armature(labelMap);
  armature.SetDebug(Debug);
  bool success = armature.InitSkeleton(armaturePolyData);

  bender::IOUtils::FilterProgress("Segment bones", 0.99, 0.89, 0.1);
  bender::IOUtils::FilterEnd("Segment bones");

  bender::IOUtils::FilterStart("Write output partitions");
  bender::IOUtils::WriteImage<LabelImageType>(
    armature.GetBodyPartition(), BodyPartition.c_str());
  bender::IOUtils::FilterEnd("Write output partitions");

  // Don't forget to delete polydata :)
  armaturePolyData->Delete();

  return (success || IgnoreErrors ? EXIT_SUCCESS : EXIT_FAILURE);
}
