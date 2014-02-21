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

#include "LabelMapField.h"


namespace Cleaver {

//----------------------------------------------------------------------------
template< class TPixel>
LabelMapField<TPixel>::LabelMapField(typename ImageType::Pointer labelImage)
{
  this->LabelMap = labelImage;

  this->Interpolant = InterpolationType::New();
  this->Interpolant->SetInputImage(this->LabelMap);

  typename ImageType::SizeType size =
    this->LabelMap->GetLargestPossibleRegion().GetSize();
  //ImageType::PointType origin = this->LabelMap->GetOrigin();

  // TODO: Investigate the setup of these parameters in Cleaver default to origin (0,0,0)
//     ImageType::SpacingType  spacing = LabelImage->GetSpacing();
//     vec3 o(origin[0],origin[1],origin[2]);
  vec3 o(0,0,0);
  vec3 s(size[0], size[1], size[2]);
  this->Bounds = BoundingBox(o,s);
}

//----------------------------------------------------------------------------
template< class TPixel>
LabelMapField<TPixel>::~LabelMapField()
{
}

//----------------------------------------------------------------------------
template< class TPixel>
float LabelMapField<TPixel>::valueAt(float x, float y, float z) const
{
  double p[3] = {x - 0.5,y - 0.5,z - 0.5};
  typename InterpolationType::ContinuousIndexType index(p);
#ifndef NDEBUG
  if (! this->LabelMap->GetLargestPossibleRegion().IsInside(index))
    {
    std::cerr << "Value at (" << x << ", " << y << ", " << z << ") is outside volume boundaries."
              << index << " -> " << this->Interpolant->EvaluateAtContinuousIndex(index) << std::endl;
    return -1.;
    }
#endif
  float res = this->Interpolant->EvaluateAtContinuousIndex(index);
  //std::cout << this << "(" << x << ", " << y << ", " << z << "): " << res << std::endl;
  return res;
}

//----------------------------------------------------------------------------
template< class TPixel>
BoundingBox LabelMapField<TPixel>::bounds() const
{
  return this->Bounds;
}

} // namespace

