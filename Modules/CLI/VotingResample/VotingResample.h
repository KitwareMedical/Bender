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
/*=========================================================================

Program:   Maverick
Module:    $RCSfile: VotingResample.h, v $

Copyright (c) Kitware Inc. 28 Corporate Drive,
Clifton Park, NY, 12065, USA.

All rights reserved. No part of this software may be reproduced, distributed,
or modified, in any form or by any means, without permission in writing from
Kitware Inc.

IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY FOR
DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY DERIVATIVES THEREOF,
EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE IS PROVIDED ON AN
"AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE NO OBLIGATION TO PROVIDE
MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.

=========================================================================*/
#ifndef _VotingResample_h_
#define _VotingResample_h_

#include "itkImage.h"
#include "itkImageFileReader.h"
#include "itkVotingResampleImageFunction.h"
#include "itkResampleImageFilter.h"
#include <string>

#include "itkPluginFilterWatcher.h"

//----------------------------------------------------------------------------
template <class ImageType>
typename ImageType::Pointer
VotingResample(typename ImageType::Pointer input,
               std::vector<float>& spacing,
               std::vector<int>& highPrecedenceLabels,
               std::vector<int>& lowPrecedenceLabels,
               int radius,
               ModuleProcessInformation * processInformation = NULL,
               double progressFraction = 1,
               double progressStart = 0)
{
  // Get the input properties
  const typename ImageType::RegionType& inputRegion = input->GetLargestPossibleRegion();
  const typename ImageType::PointType& inputOrigin = input->GetOrigin();
  const typename ImageType::SpacingType& inputSpacing = input->GetSpacing();
  const typename ImageType::DirectionType& inputDirection = input->GetDirection();
  const typename ImageType::SizeType& inputSize = inputRegion.GetSize();
  typedef typename ImageType::SizeType::SizeValueType SizeValueType;

  // Setup the output
  typename ImageType::Pointer output = ImageType::New();
  typename ImageType::SizeType outputSize = inputRegion.GetSize();
  typename ImageType::PointType outputOrigin = input->GetOrigin();
  typename ImageType::IndexType outputIndex = inputRegion.GetIndex();
  typename ImageType::SpacingType outputSpacing = input->GetSpacing();
  typename ImageType::DirectionType outputDirection = input->GetDirection();

  for(size_t i = 0; i < spacing.size(); i++)
    {
    if (spacing[i] > 1e-6)
      {
      outputSpacing[i] = spacing[i];

      // Update size
      double scale = static_cast<double>(outputSpacing[i]) / inputSpacing[i];
      outputSize[i] =
        static_cast<SizeValueType>(static_cast<double>(inputSize[i]) / scale);

      // Update origin
      double sign = input->GetDirection()[i][i];
      outputOrigin[i] =
        inputOrigin[i] + sign * (outputSpacing[i] - inputSpacing[i]) /2.;
      }
    }


  typename ImageType::RegionType outputRegion;
  outputRegion.SetSize(outputSize);
  outputRegion.SetIndex(outputIndex);
  output->SetRegions(outputRegion);
  output->Allocate();
  output->FillBuffer(0);
  output->SetSpacing(outputSpacing);
  output->SetOrigin(outputOrigin);
  output->SetDirection(outputDirection);
  
  // Conduct the filter  
  typedef itk::ResampleImageFilter<ImageType, ImageType>
          ResampleImageFilterType;
  typedef itk::VotingResampleImageFunction<ImageType, double>
          VotingFunctionType;
  typename VotingFunctionType::Pointer interpolator = VotingFunctionType::New();
  interpolator->SetInputImage(input);
  interpolator->SetHighPrecedenceLabels(highPrecedenceLabels);
  interpolator->SetLowPrecedenceLabels(lowPrecedenceLabels);
  interpolator->SetOutputSpacing(output->GetSpacing());
  interpolator->SetRadius(radius);
  typename ResampleImageFilterType::Pointer resample =
    ResampleImageFilterType::New();
  itk::PluginFilterWatcher resampleWatcher( resample, "Voting Resample", processInformation, progressFraction, progressStart );
  resample->SetInput(input);
  resample->SetInterpolator(interpolator);
  resample->SetSize(output->GetLargestPossibleRegion().GetSize());
  resample->SetOutputSpacing(output->GetSpacing());
  resample->SetOutputOrigin(output->GetOrigin());
  resample->SetOutputDirection(output->GetDirection());
  resample->Update(); 

  // return the result
  return resample->GetOutput();
}

#endif
