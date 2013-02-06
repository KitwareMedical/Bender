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

#ifndef __SolveHeatDiffusionProblem_h__
#define __SolveHeatDiffusionProblem_h__

// Bender includes
#include "HeatDiffusionProblem.h"

// Eigen includes
#include "EigenSparseSolve.h"

// ITK includes
#include <itkImage.h>
#include <itkMath.h>
#include <itkIndex.h>
#include <itkImageRegionIteratorWithIndex.h>

// STD includes
#include <iostream>
#include <assert.h>

template<class Image>
class SolveHeatDiffusionProblem
{
 public:
  typedef typename Image::IndexType Pixel;
  typedef typename Image::PixelType Real;
  typedef typename Image::OffsetType PixelOffset;
  typedef typename Image::RegionType Region;

  //Description:
  // Solve the heat diffusion problem
  static void Solve(const HeatDiffusionProblem<Image::ImageDimension>& problem,  typename Image::Pointer heat);

  //Pre: The output heat already contains the partial solution. In particular,
  //     for any pixels p such that problem.IsBoundary(p)==true, heat[p]==boundary value
  static void SolveIteratively(const HeatDiffusionProblem<Image::ImageDimension>& problem,  typename Image::Pointer heat, int numIterations);

private:
  class Neighborhood
  {
  public:
    Neighborhood()
      {
      for(unsigned int i=0; i<Image::ImageDimension; ++i)
        {
        int lo = 2*i;
        int hi = 2*i+1;
        for(unsigned int j=0; j<Image::ImageDimension; ++j)
          {
          this->Offsets[lo][j] = j==i? -1 : 0;
          this->Offsets[hi][j] = j==i?  1 : 0;
          }
        }
      }
    PixelOffset Offsets[2*Image::ImageDimension];
  };
};

#include "SolveHeatDiffusionProblem.txx"

#endif
