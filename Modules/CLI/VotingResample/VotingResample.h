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
        static_cast<SizeValueType>(0.5 + static_cast<double>(inputSize[i]) / scale);

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
  typename ResampleImageFilterType::Pointer resample =
    ResampleImageFilterType::New();
  itk::PluginFilterWatcher resampleWatcher( resample, "Voting Resample", processInformation, progressFraction, progressStart );
  resample->SetInput(input);
  resample->SetInterpolator(interpolator);
  resample->SetSize(output->GetLargestPossibleRegion().GetSize());
  resample->SetOutputSpacing(output->GetSpacing());
  resample->SetOutputOrigin(outputOrigin);
  resample->SetOutputDirection(outputDirection);
  resample->Update(); 

  // return the result
  return resample->GetOutput();
}

#endif
