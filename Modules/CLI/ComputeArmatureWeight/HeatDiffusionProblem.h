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

#ifndef __HeatDiffusionProblem_h
#define __HeatDiffusionProblem_h

// itk includes
#include <itkIndex.h>

// .NAME HeatDiffusionProblem
// .SECTION General Description
//  An abstract that describes an image-based heat diffusion problem
template<unsigned int dimension>
class HeatDiffusionProblem
{
public:
  typedef itk::Index<dimension> Pixel;
  HeatDiffusionProblem() {};
  //Is the pixel inside the problem domain?
  virtual bool InDomain(const Pixel&) const = 0;

  //Is the pixel on the boundary of the heat diffusion?
  virtual bool IsBoundary(const Pixel&) const = 0;

  //What is the given value of the boundary pixel.
  //Pre: IsBoundary(pixel) must be true
  virtual float GetBoundaryValue(const Pixel&) const = 0;
};

#endif
