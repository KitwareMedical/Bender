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

#ifndef __WeightMapMath_h
#define __WeightMapMath_h

// Bender includes
#include "benderWeightMap.h"

namespace bender
{
  template<class MaskImageType>
  inline bool Lerp(const bender::WeightMap& weightMap, //weight input
                   const itk::ContinuousIndex<double,3>& coord, //the point to evaluate at
                   const typename MaskImageType::Pointer& mask, //mask that defines the function domain, only the voxels in domain will be used
                   const typename MaskImageType::PixelType& foreground_minimum, //pixels > this value will be considered in the domain
                   bender::WeightMap::WeightVector& w_pi) //output, assumed to be initialized to the vector dimension of the weight map
  {
    typedef bender::WeightMap WeightMap;
    typedef WeightMap::Voxel Voxel;
    typename MaskImageType::RegionType region = mask->GetLargestPossibleRegion();
    w_pi.Fill(0);
    WeightMap::WeightVector w_corner=w_pi;
    w_corner.Fill(0);

    Voxel m; //min index of the cell containing the point
    m.CopyWithCast(coord);

    //interpoalte the weight for vertex over the cube corner
    double cornerWSum(0);
    for(unsigned int corner=0; corner<8; ++corner)
      {
      //for each bit of index
      unsigned int bit = corner;
      double cornerW=1.0;
      Voxel q;
      for(int dim=0; dim<3; ++dim)
        {
        bool upper = bit & 1;
        bit>>=1;
        float t = coord[dim] - static_cast<float>(m[dim]);
        cornerW*= upper? t : 1-t;
        q[dim] = m[dim]+ static_cast<int>(upper);
        }
      if(cornerW>=0 &&
         cornerW<=1
         && region.IsInside(q)
         && mask->GetPixel(q)>=foreground_minimum)
        {
        w_corner.Fill(0);
        cornerWSum+=cornerW;
        weightMap.Get(q, w_corner);
        w_pi += cornerW*w_corner;
        }
      }
    if(cornerWSum!=0.0)
      {
      w_pi*= 1.0/cornerWSum;
      return true;
      }
    else
      {
      return false;
      }
    }
};

#endif
