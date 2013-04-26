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

#ifndef __benderWeightMapIO_h
#define __benderWeightMapIO_h

// Bender includes
#include "benderWeightMap.h"

// STD includes
#include <string>
#include <vector>

namespace bender
{
// Get the weight files from a directory
void BENDER_COMMON_EXPORT GetWeightFileNames(const std::string& dirName, std::vector<std::string>& fnames);

// Create a weight map from a list of voxels
int BENDER_COMMON_EXPORT ReadWeights(const std::vector<std::string>& fnames,
                                     const std::vector<WeightMap::Voxel>& bodyVoxels,
                                     WeightMap& weightMap, const unsigned char* abort = 0);

// Create a weight map from an image (labelmap).
template <class T>
int BENDER_COMMON_EXPORT ReadWeightsFromImage(const std::vector<std::string>& fnames,
                                              const typename itk::Image<T, 3>::Pointer image,
                                              bender::WeightMap& weightMap, const unsigned char* abort = 0);
};

#include "benderWeightMapIO.txx"

#endif
