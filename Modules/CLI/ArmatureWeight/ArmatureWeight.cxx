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
#include "Armature.h"
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

typedef itk::Image<unsigned short, 3>  LabelImage;
typedef itk::Image<unsigned char, 3>  CharImage;

typedef itk::Image<WeightImagePixel, 3>  WeightImage;

typedef itk::Index<3> Voxel;
typedef itk::Offset<3> VoxelOffset;
typedef itk::ImageRegion<3> Region;

//-------------------------------------------------------------------------------
//Expand the foreground once. The new foreground pixels are assigned foreGroundMin
int ExpandForegroundOnce(LabelImage::Pointer labelMap, unsigned short foreGroundMin)
{
  int numNewVoxels=0;
  CharImage::RegionType region = labelMap->GetLargestPossibleRegion();
  itk::ImageRegionIteratorWithIndex<LabelImage> it(labelMap,region);
  Neighborhood<3> neighbors;
  VoxelOffset* offsets = neighbors.Offsets;

  std::vector<Voxel> front;
  for(it.GoToBegin(); !it.IsAtEnd(); ++it)
    {
    Voxel p = it.GetIndex();
    LabelImage::PixelType value = it.Get();
    if(value>=foreGroundMin)
      {
      for(int iOff=0; iOff<6; ++iOff)
        {
        const VoxelOffset& offset = offsets[iOff];
        Voxel q = p + offset;
        if(region.IsInside(q) && labelMap->GetPixel(q)<foreGroundMin)
          {
          front.push_back(q);
          }
        }
      }
    }
  for(std::vector<Voxel>::const_iterator i = front.begin(); i!=front.end();i++)
    {
    if(labelMap->GetPixel(*i)<foreGroundMin)
      {
      labelMap->SetPixel( *i, foreGroundMin);
      ++numNewVoxels;
      }
    }
  return numNewVoxels;
}

//-------------------------------------------------------------------------------
inline int NumDigits(unsigned int a)
{
  int numDigits(0);
  while(a>0)
    {
    a = a/10;
    ++numDigits;
    }
  return numDigits;
}

//-------------------------------------------------------------------------------
void RemoveSingleVoxelIsland(LabelImage::Pointer labelMap)
{
  Neighborhood<3> neighbors;
  const VoxelOffset* offsets = neighbors.Offsets;

  Region region = labelMap->GetLargestPossibleRegion();
  itk::ImageRegionIteratorWithIndex<LabelImage> it(labelMap,region);
  for(it.GoToBegin(); !it.IsAtEnd(); ++it)
    {
    if(it.Get()>0)
      {
      Voxel p = it.GetIndex();
      int numNeighbors(0);
      for(int iOff=0; iOff<6; ++iOff)
        {
        Voxel q = p + offsets[iOff];
        if( region.IsInside(q) && labelMap->GetPixel(q)>0)
          {
          ++numNeighbors;
          }
        }
      if(numNeighbors==0)
        {
        std::cerr<<"Paint isolated voxel "<<p<<" to background" << std::endl;
        labelMap->SetPixel(p, 0);
        }
      }
    }
}

//-------------------------------------------------------------------------------
int main( int argc, char * argv[] )
{
  PARSE_ARGS;

  if(BinaryWeight)
    {
    std::cout << "Use binary weight: " << std::endl;
    }

  if(!IsArmatureInRAS)
    {
    std::cout << "Input armature is not in RAS coordinate system; will convert it to RAS." << std::endl;
    }

  std::cout << "Padding distance: " << Padding << std::endl;

  bender::IOUtils::FilterStart("Read inputs");
  bender::IOUtils::FilterProgress("Read inputs", 0.01, 0.33, 0.);

  //----------------------------
  // Read label map
  //----------------------------
  typedef itk::ImageFileReader<LabelImage>  ReaderType;
  ReaderType::Pointer labelMapReader = ReaderType::New();
  labelMapReader->SetFileName(RestLabelmap.c_str() );
  labelMapReader->Update();
  LabelImage::Pointer labelMap = labelMapReader->GetOutput();
  bender::IOUtils::FilterProgress("Read inputs", 0.33, 0.33, 0.);

  typedef itk::StatisticsImageFilter<LabelImage>  StatisticsType;
  StatisticsType::Pointer statistics = StatisticsType::New();
  statistics->SetInput( labelMapReader->GetOutput() );
  statistics->Update();

  //----------------------------
  // Print out some statistics
  //----------------------------
  Region allRegion = labelMap->GetLargestPossibleRegion();
  itk::ImageRegionIteratorWithIndex<LabelImage> it(labelMap,labelMap->GetLargestPossibleRegion());
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
  int totalVoxels = allRegion.GetSize()[0]*allRegion.GetSize()[1]*allRegion.GetSize()[2];

  std::cout << "Image statistics\n";
  std::cout << "  min: "<<(int)statistics->GetMinimum()<<" max: "<<(int)statistics->GetMaximum() << std::endl;
  std::cout << "  total # voxels  : "<<totalVoxels << std::endl;
  std::cout << "  num body voxels : "<<numBodyVoxels << std::endl;
  std::cout << "  num bone voxels : "<<numBoneVoxels << std::endl;

  bender::IOUtils::FilterProgress("Read inputs", 0.66, 0.33, 0.);

  //----------------------------
  // Preprocess of the labelmap
  //----------------------------
  RemoveSingleVoxelIsland(labelMap);
  int numPaddedVoxels =0;
  for(int i=0; i<Padding; i++)
    {
    numPaddedVoxels+=ExpandForegroundOnce(labelMap,bodyIntensity);
    std::cout<<"Padded "<<numPaddedVoxels<<" voxels"<<std::endl;
    }
  bender::IOUtils::FilterProgress("Read inputs", 0.99, 0.33, 0.);
  bender::IOUtils::FilterEnd("Read inputs");
  bender::IOUtils::FilterStart("Segment bones");
  bender::IOUtils::FilterProgress("Segment bones", 0.01, 0.33, 0.33);

  //----------------------------
  // Read armature information
  //----------------------------
  ArmatureType armature(labelMap);
  armature.SetDebug(DumpDebugImages);
  armature.SetDumpDebugImages(DumpDebugImages);
  armature.Init(ArmaturePoly.c_str(),!IsArmatureInRAS);

  if (LastEdge<0)
    {
    LastEdge = armature.GetNumberOfEdges()-1;
    }
  std::cout << "Process armature edge from "<<FirstEdge<<" to "<<LastEdge << std::endl;
  bender::IOUtils::FilterProgress("Segment bones", 0.99, 0.33, 0.33);
  bender::IOUtils::FilterEnd("Segment bones");
  bender::IOUtils::FilterStart("Compute weights");
  bender::IOUtils::FilterProgress("Compute weights", 0.01, 0.33, 0.66);

  //--------------------------------------------
  // Compute the domain of reach armature part
  //--------------------------------------------
  int numDigits = NumDigits(armature.GetNumberOfEdges());
  for(int i=FirstEdge; i<=LastEdge; ++i)
    {
    ArmatureEdge edge(armature,i);
    edge.SetDebug(DumpDebugImages);
    edge.SetDumpDebugImages(DumpDebugImages);
    std::cout << "Process armature edge "<<i<<" with label "<<(int)edge.GetLabel() << std::endl;
    edge.Initialize(BinaryWeight? 0 : ExpansionDistance);
    WeightImage::Pointer weight = edge.ComputeWeight(BinaryWeight,SmoothingIteration);
    std::stringstream filename;
    filename << WeightDirectory << "/weight_"
             << std::setfill('0') << std::setw(numDigits) << i << ".mha";
    bender::IOUtils::WriteImage<WeightImage>(weight,filename.str().c_str());
    bender::IOUtils::FilterProgress("Compute weights",
                                    (static_cast<double>(i - FirstEdge + 1) / (LastEdge-FirstEdge +1)),
                                    0.33, 0.66);
    }
  bender::IOUtils::FilterEnd("Compute weights");

  return EXIT_SUCCESS;
}
