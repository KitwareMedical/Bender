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

#include "LabelMapField.h"


namespace Cleaver {

LabelMapField::LabelMapField(ImageType::Pointer LabelImage)
{
  this->LabelMap = LabelImage;

  this->Interpolant = InterpolationType::New();
  this->Interpolant->SetInputImage(LabelImage);

  ImageType::SizeType size =
    LabelImage->GetLargestPossibleRegion().GetSize();
  ImageType::PointType origin = LabelImage->GetOrigin();
  // TODO: Investigate the setup of these parameters in Cleaver default to origin (0,0,0)
//     ImageType::SpacingType  spacing = LabelImage->GetSpacing();
//     vec3 o(origin[0],origin[1],origin[2]);
  vec3 o(0,0,0);
  vec3 s(size[0],size[1],size[2]);
  this->Bounds = BoundingBox(o,s);

  this->Scale[0] = 1.0;
  this->Scale[1] = 1.0;
  this->Scale[2] = 1.0;
  this->data     = NULL;
}

LabelMapField::~LabelMapField()
{
  if (this->data)
    {
    delete [] this->data;
    this->data = NULL;
    }
}

float LabelMapField::valueAt(float x, float y, float z) const
{
  if(this->GenerateDataFromLabels)
    {
    typename ImageType::SizeType size =
      this->LabelMap->GetLargestPossibleRegion().GetSize();
    x -= 0.5f;
    y -= 0.5f;
    z -= 0.5f;
    float t = fmod(x,1.0f);
    float u = fmod(y,1.0f);
    float v = fmod(z,1.0f);

    int i0 = floor(x);
    int i1 = i0+1;
    int j0 = floor(y);
    int j1 = j0+1;
    int k0 = floor(z);
    int k1 = k0+1;

    i0 = clamp(i0, 0, size[0]-1);
    j0 = clamp(j0, 0, size[1]-1);
    k0 = clamp(k0, 0, size[2]-1);

    i1 = clamp(i1, 0, size[0]-1);
    j1 = clamp(j1, 0, size[1]-1);
    k1 = clamp(k1, 0, size[2]-1);

    float C000 = this->data[i0 + j0*size[0] + k0*size[0]*size[1]];
    float C001 = this->data[i0 + j0*size[0] + k1*size[0]*size[1]];
    float C010 = this->data[i0 + j1*size[0] + k0*size[0]*size[1]];
    float C011 = this->data[i0 + j1*size[0] + k1*size[0]*size[1]];
    float C100 = this->data[i1 + j0*size[0] + k0*size[0]*size[1]];
    float C101 = this->data[i1 + j0*size[0] + k1*size[0]*size[1]];
    float C110 = this->data[i1 + j1*size[0] + k0*size[0]*size[1]];
    float C111 = this->data[i1 + j1*size[0] + k1*size[0]*size[1]];

    return float((1-t)*(1-u)*(1-v)*C000 + (1-t)*(1-u)*(v)*C001 +
                 (1-t)*  (u)*(1-v)*C010 + (1-t)*  (u)*(v)*C011 +
                 (t)*(1-u)*(1-v)*C100 +   (t)*(1-u)*(v)*C101 +
                 (t)*  (u)*(1-v)*C110 +   (t)*  (u)*(v)*C111);
    }

  double               p[3] = {x,y,z};
  ImageType::PointType point(p);
  return this->Interpolant->Evaluate(point);
}

void LabelMapField::SetGenerateDataFromLabels(bool _generateData)
{
  this->GenerateDataFromLabels = _generateData;
  if(this->GenerateDataFromLabels && this->LabelMap.IsNotNull() && this->data ==
     NULL)
    {
    typename ImageType::SizeType size =
      this->LabelMap->GetLargestPossibleRegion().GetSize();
    typename ImageType::SpacingType spacing = this->LabelMap->GetSpacing();
    int w = size[0];
    int h = size[1];
    int d = size[2];

    this->data = new ImageType::PixelType[w*h*d];
    for(int k = 0; k < d; ++k)
      for(int j = 0; j < h; ++j)
        for(int i = 0; i < w; ++i)
          {
          typename ImageType::IndexType index;
          index[0]                    = i; index[1] = j; index[2] = k;
          this->data[i + j*w + k*w*h] = this->LabelMap->GetPixel(index);
          }

    }
}

float LabelMapField::valueAt(const vec3& x) const
{
  return this->valueAt(x[0],x[1],x[2]);
}

BoundingBox LabelMapField::bounds() const
{
  return this->Bounds;
}

} // namespace

