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

// VTK includes
#include <vtkIdTypeArray.h>

// STD includes
#include <iostream>
#include <numeric>

namespace bender
{
//-------------------------------------------------------------------------------
WeightMap::WeightMap()
 : Cols(0)
 , MinForegroundValue(0.)
 , MaxWeightDegree(-1)
 , MinWeightValue(std::numeric_limits<float>::min())
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
  if (value < this->MinWeightValue)
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
WeightMap::WeightEntry WeightMap::Get(const Voxel& v, WeightVector& values) const
{
  WeightEntry maxEntry;
  values.Fill(0);

  if (!this->LUTIndex->GetLargestPossibleRegion().IsInside(v))
    {
    return maxEntry;
    }

  size_t j = this->LUTIndex->GetPixel(v);
  assert(j<Cols);

  const size_t rows = this->RowSize[j];
  for(size_t i=0; i<rows; ++i)
    {
    const WeightEntry& entry = this->LUT[i][j];
    values[entry.Index] = entry.Value;
    if (entry.Value >= maxEntry.Value)
      {
      maxEntry.Value = entry.Value;
      maxEntry.Index = entry.Index;
      }
    }
  return maxEntry;
}

//-------------------------------------------------------------------------------
void WeightMap::SetMinWeightValue(float minWeight)
{
  this->MinWeightValue = minWeight;
}

//-------------------------------------------------------------------------------
float WeightMap::GetMinWeightValue()const
{
  return this->MinWeightValue;
}

//-------------------------------------------------------------------------------
void WeightMap::AddRow()
{
  this->LUT.push_back(WeightEntries(0));
  this->LUT.back().resize(this->Cols);
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
void WeightMap::SetWeightsFiliation(vtkIdTypeArray* weightsFiliation,
                                    int maxDegree)
{
  this->WeightsDegrees.resize(weightsFiliation->GetNumberOfTuples());
  for (SiteIndex index = 0; index < weightsFiliation->GetNumberOfTuples(); ++index)
    {
    //
    // Use Djikstras to compute distance map
    //

    // Init edges and distance map
    RowSizes& degrees = this->WeightsDegrees[index];
    degrees.resize(weightsFiliation->GetNumberOfTuples(),
                   std::numeric_limits<SiteIndex>::max());
    degrees[index] = 0; // 0th degree for itself
    std::vector<bool> edges (weightsFiliation->GetNumberOfTuples(), false);

    // for each weight
    for (vtkIdType count = 0; count < weightsFiliation->GetNumberOfTuples(); ++count)
      {
      // search the next closest weight (in degree).
      SiteIndex degree = std::numeric_limits<SiteIndex>::max();
      vtkIdType currentID = -1;
      for (size_t i = 0; i < degrees.size(); ++i)
        {
        if (degrees[i] < degree && edges[i] == false)
          {
          degree = degrees[i];
          currentID = i;
          }
        }
      if (degree == std::numeric_limits<SiteIndex>::max())
        {
        std::cerr << "ERROR:" << std::endl
                  << "While computing edge distance map, "
                  << "every edge should be accessible !"<<std::endl;
        break;
        }
      edges[currentID] = true; // visited

      for (vtkIdType id = 0; id < weightsFiliation->GetNumberOfTuples(); ++id)
        {
        SiteIndex newDegree = std::numeric_limits<SiteIndex>::max();
        if (weightsFiliation->GetValue(currentID) == id // ID is currentID parent
            || weightsFiliation->GetValue(id) == currentID) // ID is currentID child
          {
          newDegree = degree + 1;
          }

        if (newDegree < degrees[id])
          {
          degrees[id] = newDegree;
          }
        }
      }
    }
  this->MaxWeightDegree = maxDegree;
}

//-------------------------------------------------------------------------------
bool WeightMap::IsUnfiliated(SiteIndex index, SiteIndex cornerIndex)const
{
  if (this->MaxWeightDegree < 0)
    {
    return false;
    }
  if (index == std::numeric_limits<SiteIndex>::max() ||
      cornerIndex == std::numeric_limits<SiteIndex>::max())
    {
    return true;
    }
  return this->WeightsDegrees[index][cornerIndex] > this->MaxWeightDegree;
}

//-------------------------------------------------------------------------------
bool WeightMap::Lerp(const itk::ContinuousIndex<double,3>& coord,
                     WeightMap::WeightVector& w_pi)const
{
  WeightMap::Voxel minVoxel; // min index of the cell containing the point
  minVoxel.CopyWithCast(coord);

  // Closest cell valid index containing the point.
  WeightEntry closestEntry;
  double highestW = 0.;

  WeightMap::Voxel q[8];
  double cornerW[8];

  for (unsigned int corner=0; corner<8; ++corner)
    {
    // for each bit of index
    cornerW[corner] = 1.;
    unsigned int bit = corner;
    for (int dim=0; dim<3; ++dim)
      {
      bool upper = bit & 1;
      bit >>= 1;
      float t = coord[dim] - static_cast<float>(minVoxel[dim]);
      cornerW[corner] *= upper? t : 1-t;
      q[corner][dim] = minVoxel[dim]+ static_cast<int>(upper);
      }
    if (cornerW[corner] > highestW)
      {
      WeightEntry entry = this->Get(q[corner], w_pi);
      if (entry.Index != std::numeric_limits<SiteIndex>::max())
        {
        highestW = cornerW[corner];
        closestEntry = entry;
        }
      }
    }

  w_pi.Fill(0.);

  // interpolate the weight for vertex over the cube corner
  double cornerWSum(0);
  for (unsigned int corner = 0; corner < 8; ++corner)
    {
    if (cornerW[corner] > 0. &&
        cornerW[corner] <= 1. &&
        !this->IsMasked(q[corner]))
      {
      WeightMap::WeightVector w_corner(w_pi.GetSize());
      WeightEntry entry = this->Get(q[corner], w_corner);
      if (!this->IsUnfiliated(closestEntry.Index, entry.Index))
        {
        w_pi += w_corner * cornerW[corner];
        cornerWSum += cornerW[corner];
        }
      }
    }
  if (cornerWSum != 0.0)
    {
    w_pi *= 1.0/cornerWSum;
    return true;
    }
  else
    {
    return false;
    }
}

};
