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
#include <itkImageRegionIterator.h>

// STD includes
#include <iostream>
#include <numeric>

namespace bender
{
//-------------------------------------------------------------------------------
WeightMap::WeightMap()
 : Cols(0)
 , MinForegroundValue(0.)
{
}

//-------------------------------------------------------------------------------
void WeightMap::Init(const std::vector<Voxel>& voxels, const itk::ImageRegion<3>& region)
{
  this->Cols = voxels.size();
  this->RowSize.resize(this->Cols,0);

  this->LUTIndex = WeightLUTIndex::New();
  this->LUTIndex->SetRegions(region);
  this->LUTIndex->Allocate();
  this->LUTIndex->FillBuffer(std::numeric_limits<std::size_t>::max());

  //itk::ImageRegionIterator<WeightLUTIndex> it(this->LUTIndex,region);
  //for(it.GoToBegin(); !it.IsAtEnd(); ++it)
  //  {
  //  it.Set(std::numeric_limits<std::size_t>::max());
  //  }

  for(size_t j=0; j<voxels.size(); ++j)
    {
    if (region.IsInside(voxels[j]))
      {
      this->LUTIndex->SetPixel(voxels[j], j);
      }
    }

  this->UpdateMaskRegion();
}

//-------------------------------------------------------------------------------
bool WeightMap::Insert(const Voxel& v, SiteIndex index, float value)
{
  if(value<=0)
    {
    return false;
    }
  if (!this->LUTIndex->GetLargestPossibleRegion().IsInside(v))
    {
    return false;
    }
  size_t j = this->LUTIndex->GetPixel(v);
  assert(j<Cols);
  size_t i = this->RowSize[j];
  if (i>=LUT.size())
    {
    this->AddRow();
    }
  WeightEntry& weight = this->LUT[i][j];
  weight.Index = index;
  weight.Value = value;

  this->RowSize[j]++;
  return true;
}

//-------------------------------------------------------------------------------
void WeightMap::Get(const Voxel& v, WeightVector& values) const
{
  values.Fill(0);

  if (!this->LUTIndex->GetLargestPossibleRegion().IsInside(v))
    {
    return;
    }

  size_t j = this->LUTIndex->GetPixel(v);
  assert(j<Cols);

  const size_t rows = this->RowSize[j];
  for(size_t i=0; i<rows; ++i)
    {
    const WeightEntry& entry = this->LUT[i][j];
    values[entry.Index] = entry.Value;

    }
}

//-------------------------------------------------------------------------------
void WeightMap::AddRow()
{
  this->LUT.push_back(WeightEntries(this->Cols));
}

//-------------------------------------------------------------------------------
void WeightMap::Print() const
{
  int numEntries = std::accumulate(this->RowSize.begin(), this->RowSize.end(),0);
  std::cout<<"Weight map "<<this->LUT.size()<<"x"<<Cols<<" has "<<numEntries<<" entries"<<std::endl;
}

//-------------------------------------------------------------------------------
void WeightMap::SetMaskImage(const itk::Image<float, 3>::Pointer maskImage,
                  float minForegroundValue)
{
  this->MaskImage = maskImage;
  this->MinForegroundValue = minForegroundValue;

  this->UpdateMaskRegion();
}

//-------------------------------------------------------------------------------
void WeightMap::UpdateMaskRegion()
{
  if (this->LUTIndex.IsNotNull());
    {
    this->MaskRegion = this->LUTIndex->GetLargestPossibleRegion();
    }
  if (this->MaskImage.IsNotNull())
    {
    this->MaskRegion.Crop(this->MaskImage->GetLargestPossibleRegion());
    }
}

//-------------------------------------------------------------------------------
bool WeightMap::IsMasked(const WeightMap::Voxel& voxel)const
{
  if (!this->MaskRegion.IsInside(voxel))
    {
    return true;
    }
  if (this->MaskImage.IsNotNull())
    {
    if (this->MaskImage->GetPixel(voxel) < this->MinForegroundValue)
      {
      return true;
      }
    }
  return false;
}


//-------------------------------------------------------------------------------
bool WeightMap::Lerp(const itk::ContinuousIndex<double,3>& coord,
                     WeightMap::WeightVector& w_pi)const
{
  WeightMap::Voxel minVoxel; // min index of the cell containing the point
  minVoxel.CopyWithCast(coord);

  WeightMap::Voxel closestVoxel; // closest index of the cell containing the point
  closestVoxel.CopyWithRound(coord);
  WeightEntry closestEntry = this->Get(closestVoxel, w_pi);
  w_pi.Fill(0.);

  WeightMap::Voxel q;

  // interpolate the weight for vertex over the cube corner
  double cornerWSum(0);
  for (unsigned int corner=0; corner<8; ++corner)
    {
    // for each bit of index
    unsigned int bit = corner;
    double cornerW=1.0;
    for (int dim=0; dim<3; ++dim)
      {
      bool upper = bit & 1;
      bit >>= 1;
      float t = coord[dim] - static_cast<float>(minVoxel[dim]);
      cornerW *= upper? t : 1-t;
      q[dim] = minVoxel[dim]+ static_cast<int>(upper);
      }
    if (cornerW > 0. &&
        cornerW <= 1. &&
        !this->IsMasked(q))
      {
      WeightMap::WeightVector w_corner(w_pi.GetSize());
      WeightEntry entry = this->Get(q, w_corner);
        w_pi += w_corner * cornerW;
        cornerWSum+=cornerW;
      }
    }
  if (cornerWSum != 0.0)
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
