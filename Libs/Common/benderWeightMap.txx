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
#include "benderWeightMap.h"

// ITK includes
#include <itkImageRegionConstIteratorWithIndex.h>

// STD includes
#include <limits>

namespace bender
{

//-------------------------------------------------------------------------------
template <class T>
void WeightMap::Init(const typename itk::Image<T, 3>::Pointer image,
                     const itk::ImageRegion<3>& region)
{
  this->Cols = region.GetSize(0) * region.GetSize(1) * region.GetSize(2);
  this->RowSize.resize(this->Cols, 0);

  this->LUTIndex = WeightLUTIndex::New();
  this->LUTIndex->SetRegions(region);
  this->LUTIndex->Allocate();
  this->LUTIndex->FillBuffer(std::numeric_limits<std::size_t>::max());

  itk::ImageRegionConstIteratorWithIndex<itk::Image<T,3> > imageIt(image, region);
  for (size_t i = 0; !imageIt.IsAtEnd() ; ++imageIt, ++i)
    {
    this->LUTIndex->SetPixel(
      //image->TransformIndexToPhysicalPoint(imageIt->GetIndex()),
      imageIt.GetIndex(),
      i);
    }

  this->UpdateMaskRegion();
}

};


