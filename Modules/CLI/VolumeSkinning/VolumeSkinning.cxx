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

template<class T> int DoIt(int argc, char* argv[]);

//-------------------------------------------------------------------------------
int main( int argc, char * argv[] )
{
  PARSE_ARGS;

  try
    {
    itk::ImageIOBase::IOPixelType     pixelType;
    itk::ImageIOBase::IOComponentType componentType;

    itk::GetImageType(RestVolume, pixelType, componentType);

    // This filter handles all types on input, but only produces
    // signed types
    switch (componentType)
      {
      case itk::ImageIOBase::UCHAR:
        return DoIt<unsigned char>( argc, argv );
        break;
      case itk::ImageIOBase::CHAR:
        return DoIt<char>( argc, argv );
        break;
      case itk::ImageIOBase::USHORT:
        return DoIt<unsigned short>( argc, argv );
        break;
      case itk::ImageIOBase::SHORT:
        return DoIt<short>( argc, argv );
        break;
      case itk::ImageIOBase::UINT:
        return DoIt<unsigned int>( argc, argv );
        break;
      case itk::ImageIOBase::INT:
        return DoIt<int>( argc, argv );
        break;
      case itk::ImageIOBase::ULONG:
        return DoIt<unsigned long>( argc, argv );
        break;
      case itk::ImageIOBase::LONG:
        return DoIt<long>( argc, argv );
        break;
      case itk::ImageIOBase::FLOAT:
        return DoIt<float>( argc, argv );
        break;
      case itk::ImageIOBase::DOUBLE:
        return DoIt<double>( argc, argv );
        break;
      case itk::ImageIOBase::UNKNOWNCOMPONENTTYPE:
      default:
        std::cerr << "Unknown component type: " << componentType << std::endl;
        break;
      }
    }

  catch( itk::ExceptionObject & excep )
    {
    std::cerr << argv[0] << ": exception caught !" << std::endl;
    std::cerr << excep << std::endl;
    return EXIT_FAILURE;
    }
  return EXIT_SUCCESS;
}

//-------------------------------------------------------------------------------
template<class T>
int DoIt( int argc, char * argv[] )
{
  PARSE_ARGS;

  typedef itk::Image<T, 3>  ImageType;
  typedef itk::Image<unsigned short, 3>  LabelImageType;


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

  typedef itk::ImageFileReader<ImageType>  ReaderType;
  typename ReaderType::Pointer volumeReader = ReaderType::New();
  volumeReader->SetFileName(RestVolume.c_str() );
  volumeReader->Update();
  typename ImageType::Pointer volume = volumeReader->GetOutput();
  if (!volume)
    {
    std::cerr << "Can't read volume " << RestVolume << std::endl;
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

    typedef itk::StatisticsImageFilter<ImageType>  StatisticsType;
    typename StatisticsType::Pointer statistics = StatisticsType::New();
    statistics->SetInput( volume );
    statistics->Update();

    itk::ImageRegion<3> allRegion = volume->GetLargestPossibleRegion();
    itk::ImageRegionIteratorWithIndex<ImageType> it(volume,
                                                    volume->GetLargestPossibleRegion());
    const LabelType backgroundIntensity = 0;
    const LabelType boneIntensity = 253; //cancellous
    size_t numBodyVoxels(0),numBoneVoxels(0);
    for (it.GoToBegin(); !it.IsAtEnd(); ++it)
      {
      if (static_cast<LabelType>(it.Get()) > backgroundIntensity)
        {
        ++numBodyVoxels;
        }
      if (static_cast<LabelType>(it.Get()) == boneIntensity)
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

  ArmatureType<T> armature(volume);
  armature.SetBackgroundValue(BackgroundValue);
  armature.SetDebug(Debug);
  bool success = armature.InitSkeleton(armaturePolyData);

  bender::IOUtils::FilterProgress("Segment bones", 0.99, 0.89, 0.1);
  bender::IOUtils::FilterEnd("Segment bones");

  bender::IOUtils::FilterStart("Write skinned volume");
  bender::IOUtils::WriteImage<LabelImageType>(
    armature.GetBodyPartition(), SkinnedVolume.c_str());
  bender::IOUtils::FilterEnd("Write skinned volume");

  // Don't forget to delete polydata :)
  armaturePolyData->Delete();

  return (success || IgnoreErrors ? EXIT_SUCCESS : EXIT_FAILURE);
}
