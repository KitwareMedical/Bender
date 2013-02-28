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

=========================================================================*/

// Bender includes
#include "benderWeightMapIO.h"

// ITK includes
#include <itkImageRegionConstIteratorWithIndex.h>
#include <itkImageFileReader.h>

namespace bender
{

//-------------------------------------------------------------------------------
//create a weight map from a series of files
template <class T>
int ReadWeightsFromImage(const std::vector<std::string>& fnames,
                         const typename itk::Image<T, 3>::Pointer image,
                         bender::WeightMap& weightMap)
{
  typedef itk::ImageRegion<3> Region;
  Region region = image->GetLargestPossibleRegion();
  weightMap.Init<T>(image, region);

  size_t numSites(0);
  size_t numInserted(0);
  for (size_t i = 0; i < fnames.size(); ++i)
    {
    std::cout << "Read " << fnames[i] << "..." << std::endl;

    typedef itk::Image<float, 3>  WeightImage;
    typedef itk::ImageFileReader<WeightImage>  ReaderType;
    ReaderType::Pointer reader = ReaderType::New();
    reader->SetFileName(fnames[i].c_str());
    reader->Update();
    WeightImage::Pointer weight_i =  reader->GetOutput();

    if (weight_i->GetLargestPossibleRegion() !=
        image->GetLargestPossibleRegion())
      {
      std::cerr << "Weight maps regions different from image are not supported."
                << " Skip weight " << fnames[i] << std::endl;
      continue;
      }
    itk::ImageRegionConstIteratorWithIndex<itk::Image<T,3> > imageIt(image, region);
    itk::ImageRegionConstIteratorWithIndex<WeightImage> weightIt(weight_i, region);
    for (; !imageIt.IsAtEnd() || !weightIt.IsAtEnd(); ++imageIt, ++weightIt)
      {
      bool inserted = weightMap.Insert(
        //image->TransformIndexToPhysicalPoint(imageIt.GetIndex()),
        imageIt.GetIndex(),
        static_cast<bender::WeightMap::SiteIndex>(i),
        weightIt.Get());
      numInserted += (inserted ? 1 : 0);
      }
    std::cout << " " << numInserted << " voxels inserted to weight map" << std::endl;
    ++numSites;
    }
  return numSites;
}


};


