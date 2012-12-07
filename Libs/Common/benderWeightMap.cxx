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
WeightMap::WeightMap():Cols(0)
{
}

//-------------------------------------------------------------------------------
void WeightMap::Init(const std::vector<Voxel>& voxels, const Region& region)
{
  this->Cols = voxels.size();
  this->RowSize.resize(this->Cols,0);

  this->LUTIndex = WeightLUTIndex::New();
  this->LUTIndex->SetRegions(region);
  this->LUTIndex->Allocate();

  itk::ImageRegionIterator<WeightLUTIndex> it(this->LUTIndex,region);
  for(it.GoToBegin(); !it.IsAtEnd(); ++it)
    {
    it.Set(std::numeric_limits<std::size_t>::max());
    }

  for(size_t j=0; j<voxels.size(); ++j)
    {
    if (region.IsInside(voxels[j]))
      {
      this->LUTIndex->SetPixel(voxels[j], j);
      }
    }
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
};


