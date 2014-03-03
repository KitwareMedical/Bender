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
  Module:    $RCSfile: itkVotingResampleImageFunction.txx,v $
  Language:  C++
  Date:      $Date: 2006/08/22 22:25:37 $
  Version:   $Revision: 1.35 $

  Copyright (c) Insight Software Consortium. All rights reserved.
  See ITKCopyright.txt or http://www.itk.org/HTML/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef __itkVotingResampleImageFunction_txx
#define __itkVotingResampleImageFunction_txx

#include "itkVotingResampleImageFunction.h"

#include "itkConstNeighborhoodIterator.h"
#include "itkNeighborhood.h"
#include <map>

#include "vnl/vnl_math.h"

namespace itk
{

/**
 * Define the number of neighbors
 */
template<class TInputImage, class TCoordRep>
const unsigned long
VotingResampleImageFunction< TInputImage, TCoordRep >
::m_Neighbors = 1 << TInputImage::ImageDimension;


/**
 * Constructor
 */
template<class TInputImage, class TCoordRep>
VotingResampleImageFunction< TInputImage, TCoordRep >
::VotingResampleImageFunction()
{

}

/**
 * SetHighPrecedenceLabels
 */
template<class TInputImage, class TCoordRep>
void VotingResampleImageFunction<TInputImage, TCoordRep>::
SetHighPrecedenceLabels(std::vector<int>& labels)
{
  this->m_HighPrecedenceLabels = labels;
}

/**
 * GetHighPrecedenceLabels
 */
template<class TInputImage, class TCoordRep>
std::vector<int> VotingResampleImageFunction<TInputImage, TCoordRep>
::GetHighPrecedenceLabels() const
{
  return this->m_HighPrecedenceLabels;
}

/**
 * SetLowPrecedenceLabels
 */
template<class TInputImage, class TCoordRep>
void VotingResampleImageFunction<TInputImage, TCoordRep>
::SetLowPrecedenceLabels(std::vector<int>& labels)
{
  this->m_LowPrecedenceLabels = labels;
}

/**
 * GetLowPrecedenceLabels
 */
template<class TInputImage, class TCoordRep>
std::vector<int> VotingResampleImageFunction<TInputImage, TCoordRep>
::GetLowPrecedenceLabels() const
{
  return this->m_LowPrecedenceLabels;
}

/**
 * PrintSelf
 */
template<class TInputImage, class TCoordRep>
void
VotingResampleImageFunction< TInputImage, TCoordRep >
::PrintSelf(std::ostream& os, Indent indent) const
{
  this->Superclass::PrintSelf(os,indent);
}

/**
 * Evaluate at image index position
 */
template<class TInputImage, class TCoordRep>
typename VotingResampleImageFunction< TInputImage, TCoordRep >
::OutputType
VotingResampleImageFunction< TInputImage, TCoordRep >
::EvaluateAtContinuousIndex(
  const ContinuousIndexType& index) const
{
  typedef typename TInputImage::PixelType PixelType;
  typedef itk::ConstNeighborhoodIterator< TInputImage > 
    NeighborhoodIteratorType;


  typename NeighborhoodIteratorType::RadiusType radius;
  radius.Fill(1);
  NeighborhoodIteratorType it( radius, this->GetInputImage(), 
    this->GetInputImage()->GetRequestedRegion() );
  
  IndexType newIndex;
  for(int i = 0; i < 3; i++)
    {
    newIndex[i] = (int)index[i];
    }
  
  it.SetLocation(newIndex);
  itk::Neighborhood<PixelType,3> n = it.GetNeighborhood();

  // Initialize the tally.
  // If at least one high priority label is present, the tally can be ignored
  // as it's one of the high precedence label that will be used. In that case
  // the loop is simply used to find which one has the highest precedence.
  std::map<PixelType, int> tally;
  bool highPrecedenceLabelFound = false;
  typedef std::vector<int>::const_iterator IteratorType;
  IteratorType highestPrecedenceLabelIterator = this->m_HighPrecedenceLabels.end();
  PixelType ret;
  for (unsigned int i = 0; i < n.Size(); i++)
    {
    for (IteratorType HPIterator = this->m_HighPrecedenceLabels.begin();
      HPIterator != this->m_HighPrecedenceLabels.end(); ++HPIterator)
      {
      if (static_cast<int>(n[i]) == *HPIterator)
        {
        if (!highPrecedenceLabelFound)
          {
          // First time a high precedence label is found, initialize the
          // highestPrecedenceLabel value for future comparisons
          highPrecedenceLabelFound = true;
          highestPrecedenceLabelIterator = HPIterator;
          ret = n[i];
          }
        else if (HPIterator < highestPrecedenceLabelIterator)
          {
          highestPrecedenceLabelIterator = HPIterator;
          ret = n[i];
          }
        }
      }

    if (!highPrecedenceLabelFound)
      {
      tally[n[i]] = 0;
      }
    }

  if (highPrecedenceLabelFound)
    {
    return ret;
    }

  // No high precedence labels were present, so do the normal counting.
  for (unsigned int i = 0; i < n.Size(); i++)
    {
    tally[n[i]] += 1;

    for (IteratorType LPIterator = this->m_LowPrecedenceLabels.begin();
      LPIterator != this->m_LowPrecedenceLabels.end(); ++LPIterator)
      {
      // If any low precedence label is present, do NOT count them as they
      // shouldn't be selected over normal labels.
      if (static_cast<int>(n[i]) == *LPIterator)
        {
        tally[n[i]] = 0;
        }
      }
    }

  // Find the election winner
  int max = 0;
  ret = 0;
  typename std::map<PixelType, int>::const_iterator itr;
  for(itr = tally.begin(); itr != tally.end(); ++itr)
    {
    if(itr->second > max)
      {
      max = itr->second;
      ret = itr->first;
      }
    }

  if (max > 0)
    {
    return ret;
    }

  // There were no election winner. This means that there were only low
  // precedence labels. Find and return the label with smallest low precedence.
  ret = n[0];
  IteratorType smallestLowPrecedenceLabelIterator =
    this->m_LowPrecedenceLabels.begin();
  for (unsigned int i = 0; i < n.Size(); i++)
    {
    for (IteratorType LPIterator = this->m_LowPrecedenceLabels.begin();
      LPIterator != this->m_LowPrecedenceLabels.end(); ++LPIterator)
      {
      if (static_cast<int>(n[i]) == *LPIterator
        && LPIterator >= smallestLowPrecedenceLabelIterator)
        {
        ret = n[i];
        smallestLowPrecedenceLabelIterator = LPIterator;
        }
      }
    }
  return ret;
}

} // end namespace itk

#endif
