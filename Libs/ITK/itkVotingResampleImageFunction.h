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
  Module:    $RCSfile: itkVotingResampleImageFunction.h,v $
  Language:  C++
  Date:      $Date: 2007/01/30 20:56:09 $
  Version:   $Revision: 1.31 $

  Copyright (c) Insight Software Consortium. All rights reserved.
  See ITKCopyright.txt or http://www.itk.org/HTML/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef __itkVotingResampleImageFunction_h
#define __itkVotingResampleImageFunction_h

#include "itkInterpolateImageFunction.h"
#include "itkConstNeighborhoodIterator.h"

namespace itk
{

/** \class VotingResampleImageFunction
 * \brief Linearly interpolate an image at specified positions.
 *
 * VotingResampleImageFunction linearly interpolates image intensity at
 * a non-integer pixel position. This class is templated
 * over the input image type and the coordinate representation type 
 * (e.g. float or double).
 *
 * This function works for N-dimensional images.
 *
 * \warning This function work only for images with scalar pixel
 * types. For vector images use VectorVotingResampleImageFunction.
 *
 * \sa VectorVotingResampleImageFunction
 *
 * \ingroup ImageFunctions ImageInterpolators 
 */
template <class TInputImage, class TCoordRep = float>
class VotingResampleImageFunction : 
  public InterpolateImageFunction<TInputImage,TCoordRep> 
{
public:
  /** Standard class typedefs. */
  typedef VotingResampleImageFunction                     Self;
  typedef InterpolateImageFunction<TInputImage,TCoordRep> Superclass;
  typedef SmartPointer<Self>                              Pointer;
  typedef SmartPointer<const Self>                        ConstPointer;
  
  /** Run-time type information (and related methods). */
  itkTypeMacro(VotingResampleImageFunction, InterpolateImageFunction);

  /** Method for creation through the object factory. */
  itkNewMacro(Self);  

  /** OutputType typedef support. */
  typedef typename Superclass::OutputType OutputType;

  /** InputImageType typedef support. */
  typedef typename Superclass::InputImageType InputImageType;

  /** RealType typedef support. */
  typedef typename Superclass::RealType RealType;

  /** Dimension underlying input image. */
  itkStaticConstMacro(ImageDimension, unsigned int,Superclass::ImageDimension);

  /** Index typedef support. */
  typedef typename Superclass::IndexType IndexType;

  /** ContinuousIndex typedef support. */
  typedef typename Superclass::ContinuousIndexType ContinuousIndexType;

  /** SpacingType typedef support. */
  typedef typename InputImageType::SpacingType SpacingType;

  /** NeighborhoodIteratorType typedef support. */
  typedef itk::ConstNeighborhoodIterator< TInputImage > NeighborhoodIteratorType;

  /** Radius typedef support. */
  typedef typename NeighborhoodIteratorType::RadiusType RadiusType;
  typedef typename RadiusType::SizeValueType RadiusValueType;

  /** Evaluate the function at a ContinuousIndex position
   *
   * Returns the linearly interpolated image intensity at a 
   * specified point position. No bounds checking is done.
   * The point is assume to lie within the image buffer.
   *
   * ImageFunction::IsInsideBuffer() can be used to check bounds before
   * calling the method. */
  virtual OutputType EvaluateAtContinuousIndex( 
    const ContinuousIndexType & index ) const;

  // Set the precedence labels. No check is done, the previous labels are
  // simply replaced by the new ones.
  // Precedence labels influence what label is picked over another label:
  //  - High precedence labels are always picked over normal labels.
  //  - Low precedence labels are only picked if the aren't any other labels
  // around.
  // The order of the precedence labels in the vector matters:
  //  - Highest precedence labels are a the beginning of the HighPrecedenceLabels
  // vector. For example, [209, 253, 111] would make the label 209 always
  // overwrite the label 111 if both are present.
  //  - Likewise for low precedence labels, the lowest precedence labels are at
  // the beginning of the vector. For example, [143, 5, 17] would make the
  // label 17 always overwrite the label 143 if they are competing for in the
  // same voxel.
  void SetHighPrecedenceLabels(std::vector<int>& labels);
  std::vector<int> GetHighPrecedenceLabels() const;
  void SetLowPrecedenceLabels(std::vector<int>& labels);
  std::vector<int> GetLowPrecedenceLabels() const;

  void SetOutputSpacing(const SpacingType& spacing);
  itkGetConstReferenceMacro(OutputSpacing, SpacingType);

  itkSetMacro(Radius, RadiusValueType);
  itkGetConstMacro(Radius, RadiusValueType);

protected:
  VotingResampleImageFunction();
  ~VotingResampleImageFunction(){};
  void PrintSelf(std::ostream& os, Indent indent) const;

private:
  VotingResampleImageFunction( const Self& ); //purposely not implemented
  void operator=( const Self& ); //purposely not implemented

  /** Number of neighbors used in the interpolation */
  static const unsigned long  m_Neighbors;
  std::vector<int> m_HighPrecedenceLabels;
  std::vector<int> m_LowPrecedenceLabels;
  SpacingType m_OutputSpacing;
  RadiusValueType m_Radius;

};

} // end namespace itk

# include "itkVotingResampleImageFunction.txx"

#endif
