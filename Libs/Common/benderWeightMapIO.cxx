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
#include "benderWeightMapIO.h"

// ITK includes
#include <itkImageRegion.h>
#include <itkImageFileReader.h>
#include <itkDirectory.h>

using namespace bender;

typedef itk::ImageRegion<3> Region;
typedef itk::Image<float, 3>  WeightImage;

namespace bender
{
//----------------------------------------------------------------------------
  void GetWeightFileNames(const std::string& dirName, std::vector<std::string>& fnames)
{
  fnames.clear();
  itk::Directory::Pointer dir = itk::Directory::New();
  dir->Load(dirName.c_str());
  for(unsigned int i=0; i<dir->GetNumberOfFiles(); ++i)
    {
    std::string name = dir->GetFile(i);
    if(strstr(name.c_str(), ".mha"))
      {
      std::string fname = dirName;
      fname.append("/");
      fname.append(name);
      fnames.push_back(fname);
      }
    }

  std::sort(fnames.begin(), fnames.end());
}

//-------------------------------------------------------------------------------
//create a weight map from a series of files
int ReadWeights(const std::vector<std::string>& fnames,
                const std::vector<WeightMap::Voxel>& bodyVoxels,
                WeightMap& weightMap)
{
  typedef std::vector<WeightMap::Voxel> Voxels;
  Region region;
  int numSites = fnames.size();

  int numInserted(0);
  for(size_t i=0; i<fnames.size(); ++i)
    {
    std::cout << "Read " << fnames[i] << std::endl;

    typedef itk::ImageFileReader<WeightImage>  ReaderType;
    ReaderType::Pointer reader = ReaderType::New();
    reader->SetFileName(fnames[i].c_str());
    reader->Update();
    WeightImage::Pointer weight_i =  reader->GetOutput();

    if(i==0)
      {
      region = weight_i->GetLargestPossibleRegion();
      weightMap.Init(bodyVoxels,region);
      }

    if(weight_i->GetLargestPossibleRegion()!=region)
      {
      std::cerr << "WARNING: " << fnames[i] << " skipped" << std::endl;
      }
    else
      {
      for(Voxels::const_iterator v_iter = bodyVoxels.begin(); v_iter!=bodyVoxels.end(); ++v_iter)
        {
        const WeightMap::Voxel& v(*v_iter);
        WeightMap::SiteIndex index = static_cast<WeightMap::SiteIndex>(i);
        float value = weight_i->GetPixel(v);
        bool inserted = weightMap.Insert(v,index,value);
        numInserted+= inserted;
        }
      std::cout << numInserted << " inserted to weight map" << std::endl;
      weightMap.Print();
      }
    }
  return numSites;
}

};


