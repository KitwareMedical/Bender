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

  Program:   Insight Segmentation & Registration Toolkit
  Module:    itkThresholdMedianImageFunction.h
  Language:  C++
  Date:      $Date$
  Version:   $Revision$

  Copyright (c) Insight Software Consortium. All rights reserved.
  See ITKCopyright.txt or http://www.itk.org/HTML/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef __itkThresholdMedianImageFunction_txx
#define __itkThresholdMedianImageFunction_txx

#include "itkThresholdMedianImageFunction.h"
#include "itkNumericTraits.h"
#include "itkConstNeighborhoodIterator.h"

#include <vector>
#include <algorithm>

namespace itk
{

/**
 * Constructor
 */
template <class TInputImage, class TCoordRep>
ThresholdMedianImageFunction<TInputImage,TCoordRep>
::ThresholdMedianImageFunction()
{
}


/**
 *
 */
template <class TInputImage, class TCoordRep>
void
ThresholdMedianImageFunction<TInputImage,TCoordRep>
::PrintSelf(std::ostream& os, Indent indent) const
{
  this->Superclass::PrintSelf(os,indent);
}


/**
 *
 */
template <class TInputImage, class TCoordRep>
typename ThresholdMedianImageFunction<TInputImage,TCoordRep>
::OutputType
ThresholdMedianImageFunction<TInputImage,TCoordRep>
::EvaluateAtIndex(const IndexType& index) const
{
  unsigned int i;

  if( !this->GetInputImage() )
    {
    return ( NumericTraits<OutputType>::max() );
    }

  if ( !this->IsInsideBuffer( index ) )
    {
    return ( NumericTraits<OutputType>::max() );
    }

  // Create an N-d neighborhood kernel, using a zeroflux boundary condition
  typename InputImageType::SizeType kernelSize;
  kernelSize.Fill( this->m_NeighborhoodRadius );

  ConstNeighborhoodIterator<InputImageType>
    it(kernelSize, this->GetInputImage(), this->GetInputImage()->GetBufferedRegion());

  // Set the iterator at the desired location
  it.SetLocation(index);

  // We have to copy the pixels so we can run std::nth_element.
  std::vector<InputPixelType> pixels;
  typename std::vector<InputPixelType>::iterator medianIterator;

  // Walk the neighborhood
  for (i = 0; i < it.Size(); ++i)
    {
    InputPixelType pixel = it.GetPixel(i);
    if (pixel != this->m_RejectPixel)
      {
      pixels.push_back( pixel );
      }
    }

  if (pixels.size() == 0)
    {
    return this->m_RejectPixel;
    }
  // Get the median value
  unsigned int medianPosition = pixels.size() / 2;
  medianIterator = pixels.begin() + medianPosition;
  std::nth_element(pixels.begin(), medianIterator, pixels.end());
  return ( *medianIterator );
}


} // namespace itk

#endif
