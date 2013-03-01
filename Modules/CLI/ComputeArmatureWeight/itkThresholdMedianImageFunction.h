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
#ifndef __itkThresholdMedianImageFunction_h
#define __itkThresholdMedianImageFunction_h

#include <itkInterpolateImageFunction.h>
#include <itkNumericTraits.h>

namespace itk
{

/**
 * \class ThresholdMedianImageFunction
 * \brief Calculate the median value in the neighborhood of a pixel
 *
 * Calculate the median pixel value over the standard 8, 26, etc. connected
 * neighborhood.  This calculation uses a ZeroFluxNeumannBoundaryCondition.
 *
 * If called with a ContinuousIndex or Point, the calculation is performed
 * at the nearest neighbor.
 *
 * This class is templated over the input image type and the
 * coordinate representation type (e.g. float or double ).
 *
 * \ingroup ImageFunctions
 */
template <class TInputImage, class TCoordRep = double>
class ThresholdMedianImageFunction :
  public InterpolateImageFunction< TInputImage, TCoordRep >
{
public:
  /** Standard class typedefs. */
  typedef ThresholdMedianImageFunction       Self;
  typedef InterpolateImageFunction<TInputImage, TCoordRep > Superclass;
  typedef SmartPointer<Self>        Pointer;
  typedef SmartPointer<const Self>  ConstPointer;

  /** Run-time type information (and related methods). */
  itkTypeMacro(ThresholdMedianImageFunction, InterpolateImageFunction);

  /** Method for creation through the object factory. */
  itkNewMacro(Self);

  /** InputImageType typedef support. */
  typedef TInputImage                         InputImageType;
  typedef typename Superclass::InputPixelType InputPixelType;

  /** OutputType typedef support. */
  typedef typename Superclass::OutputType OutputType;

  /** Index typedef support. */
  typedef typename Superclass::IndexType IndexType;

  /** ContinuousIndex typedef support. */
  typedef typename Superclass::ContinuousIndexType ContinuousIndexType;

  /** Point typedef support. */
  typedef typename Superclass::PointType PointType;

  /** Dimension of the underlying image. */
  itkStaticConstMacro(ImageDimension, unsigned int,
                      InputImageType::ImageDimension);

  itkGetMacro(NeighborhoodRadius, unsigned int);
  itkSetMacro(NeighborhoodRadius, unsigned int);
  itkGetMacro(RejectPixel, InputPixelType);
  itkSetMacro(RejectPixel, InputPixelType);

  /** Evalulate the function at specified index */
  virtual OutputType EvaluateAtIndex( const IndexType& index ) const;

  /** Evaluate the function at non-integer positions */
  virtual OutputType Evaluate( const PointType& point ) const
    {
    IndexType index;
    this->ConvertPointToNearestIndex( point, index );
    return this->EvaluateAtIndex( index );
    }
  virtual OutputType EvaluateAtContinuousIndex(
    const ContinuousIndexType& cindex ) const
    {
    IndexType index;
    this->ConvertContinuousIndexToNearestIndex( cindex, index );
    return this->EvaluateAtIndex( index );
    }

protected:
  ThresholdMedianImageFunction();
  ~ThresholdMedianImageFunction(){};
  void PrintSelf(std::ostream& os, Indent indent) const;

  unsigned int m_NeighborhoodRadius;
  InputPixelType m_RejectPixel;
private:
  ThresholdMedianImageFunction( const Self& ); //purposely not implemented
  void operator=( const Self& ); //purposely not implemented

};

} // end namespace itk

#include "itkThresholdMedianImageFunction.txx"

#endif
