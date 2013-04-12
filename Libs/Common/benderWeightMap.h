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

#ifndef __benderWeightMap_h
#define __benderWeightMap_h

// .NAME WeightMap - sparse representation over weight vectors over a volume
// .SECTION General Description
// WeightMap represents a field of weight vectors over a volume. Compared
// with a dense representation, e.g. storing a set of weight images., this is
// sparse in two ways:
// -  Not all voxel weights are stored, only a set of chosen voxels.
// -  Define the "support" of a voxel by the number of non-zero weights at the voxel.
//    Then, for each voxel, the storage we spend is at most the max support over the volume.

// Bender includes
#include "BenderCommonExport.h"

// ITK includes
#include <itkImage.h>
#include <itkVariableLengthVector.h>

// STD includes
#include <vector>
#include <limits>

namespace bender
{
class BENDER_COMMON_EXPORT WeightMap
{
public:
  typedef unsigned char SiteIndex;
  struct WeightEntry
  {
    SiteIndex Index;
    float Value;

    WeightEntry(): Index(std::numeric_limits<SiteIndex>::max()), Value(0.){}
  };

  typedef itk::Index<3> Voxel;
  typedef std::vector<SiteIndex> RowSizes;
  typedef std::vector<WeightEntry> WeightEntries;
  typedef itk::ImageRegion<3> Region;
  typedef itk::VariableLengthVector<float> WeightVector;

  // For any j, WeightTable[...][j] correspond to the weights at a voxel.
  typedef std::vector<WeightEntries> WeightLUT;

  // For each voxel v, WeightLUTIndex[v] index into the "column" of WeightLUT.
  typedef itk::Image<size_t,3> WeightLUTIndex;

  WeightMap();
  /// Init from a list of points
  void Init(const std::vector<Voxel>& voxels, const Region& region);
  /// Init from an image
  template <class T>
  void Init(const typename itk::Image<T, 3>::Pointer image,
            const itk::ImageRegion<3>& region);
  bool Insert(const Voxel& v, SiteIndex index, float value);
  void Get(const Voxel& v, WeightVector& values) const;
  void AddRow();
  void Print() const;

  /// Mask that defines the function domain, only the voxels in domain will be used.
  /// Pixels >= minForegroundValue will be considered in the domain.
  /// \sa Lerp(), SetWeightsFiliation()
  void SetMaskImage(const itk::Image<float, 3>::Pointer maskImage,
                    float minForegroundValue);
  /// Interpolate the weights at a given point.
  /// \a coord: the point to evaluate at.
  /// \a w_pi: out, assumed to be initialized to the vector dimension of the
  /// weight map.
  /// \sa SetMaskImage(), SetWeightsFiliation()
  bool Lerp(const itk::ContinuousIndex<double,3>& coord,
            WeightMap::WeightVector& w_pi)const;

private:
  /// Set the mask region to the smallest region between the weight map region
  /// and the mask image region.
  /// \sa SetMaskImage()
  void UpdateMaskRegion();

  /// Return true if the voxel should be discarded/masked.
  /// Voxels are masked if
  ///  * they are outside the weight map region or mask region if any.
  ///  * or their value in the mask image is < MinForegroundValue.
  /// \sa IsUnfiliated(), SetMaskImage()
  bool IsMasked(const WeightMap::Voxel& voxel)const;

  WeightLUT LUT;
  WeightLUTIndex::Pointer LUTIndex;
  RowSizes RowSize;
  size_t Cols;

  itk::Image<float,3>::Pointer MaskImage;
  float MinForegroundValue;
  itk::ImageRegion<3> MaskRegion;

};

};

#include "benderWeightMap.txx"

#endif
