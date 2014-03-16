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
#include <itkImageRegionIterator.h>

int TestLabelMapField(int argc, char * argv[]);
bool TestLabelMapField();

typedef Cleaver::LabelMapField<float>::ImageType LabelImageType;

int TestLabelMapField(int argc, char * argv[])
{
  bool res = TestLabelMapField();
  return res ? EXIT_SUCCESS : EXIT_FAILURE;
}

LabelImageType::Pointer CreateImage()
{
  LabelImageType::Pointer image = LabelImageType::New();
  LabelImageType::SizeType size;
  size[0] = 3; // size along X
  size[1] = 3; // size along Y
  size[2] = 3; // size along Z
  LabelImageType::IndexType start;
  start[0] = 0; // first index on X
  start[1] = 0; // first index on Y
  start[2] = 0; // first index on Z
  LabelImageType::RegionType region;
  region.SetSize( size );
  region.SetIndex( start );
  image->SetRegions( region );
  image->Allocate();

  LabelImageType::SpacingType spacing;
  spacing[0] = 1.0; // spacing in mm along X
  spacing[1] = 1.0; // spacing in mm along Y
  spacing[2] = 1.0; // spacing in mm along Z
  image->SetSpacing( spacing );

  LabelImageType::PointType origin;
  origin[0] = 0.0; // coordinates of the
  origin[1] = 0.0; // first pixel in N-D
  origin[2] = 0.0;
  image->SetOrigin( origin );

  float data[27] = {0., 1., 2.,
                    4., 8., 16.,
                    32., 64., 128.,
                    0., 1., 2.,
                    4., 8., 16.,
                    32., 64., 128.,
                    0., 1., 2.,
                    4., 8., 16.,
                    32., 64., 128.};
  size_t i = 0;
  typedef itk::ImageRegionIterator<LabelImageType> ItType;
  ItType it( image, region );
  for(it.GoToBegin(); !it.IsAtEnd();++it)
    {
    it.Set(data[i++]);
    }
  return image;
}

bool TestLabelMapField()
{
  LabelImageType::Pointer image = CreateImage();
  Cleaver::LabelMapField<float> labelMapField(image);
  //labelMapField.SetGenerateDataFromLabels(true);
  for (float z = 0.; z <= 0.; z+=0.5)
    {
    for (float y = -1.; y < 10.; y+=0.5)
      {
      for (float x = -1.; x < 10.; x+=0.5)
        {
        std::cout << "("<< x<< "," << y << "," << z << "):"
                  << labelMapField.valueAt(x,y,z) << std::endl;
        }
      }
    }
  return true;
}
