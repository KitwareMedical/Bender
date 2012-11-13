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
#ifndef __WeightMapIO_h
#define __WeightMapIO_h

#include "WeightMap.h"
#include <string>
#include <vector>

//Get the weight files from a directory
void GetWeightFileNames(const std::string& dirName, std::vector<std::string>& fnames);

//create a weight map from a series of files
int ReadWeights(const std::vector<std::string>& fnames,  const std::vector<WeightMap::Voxel>& bodyVoxels,  WeightMap& weightMap);

#endif
