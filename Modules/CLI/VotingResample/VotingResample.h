#ifndef _VotingResample_h_
#define _VotingResample_h_

#include "itkImage.h"
#include "itkImageFileReader.h"
#include "itkVotingResampleImageFunction.h"
#include "itkResampleImageFilter.h"
#include <string>

#include "itkPluginFilterWatcher.h"

typedef   unsigned short  PixelType;
typedef itk::Image< PixelType, 3 >  ImageType;
typedef  itk::ImageFileReader< ImageType >    ReaderType;


ImageType::Pointer VotingResample( ImageType::Pointer input,
                                   bool ResampleZ,
                                   ModuleProcessInformation * processInformation = NULL,
                                   double progressFraction = 1, 
                                   double progressStart = 0)
  { 


  // Setup the output
  ImageType::Pointer output = ImageType::New();
  ImageType::RegionType inputRegion = input->GetLargestPossibleRegion();
  ImageType::SizeType outputSize = inputRegion.GetSize();
  ImageType::PointType outputOrigin = input->GetOrigin();
  ImageType::IndexType outputIndex = inputRegion.GetIndex();
  ImageType::SpacingType outputSpacing = input->GetSpacing();
  ImageType::DirectionType outputDirection = input->GetDirection();
  int tmpMax = 0;
  if(ResampleZ)
    {
    tmpMax = 3;
    }
  else
    {
    tmpMax = 2;
    }
  for(int i = 0; i < tmpMax; i++)
    {
    outputSize[i] = outputSize[i] / 2;
    outputSpacing[i] = outputSpacing[i] * 2;
    }
  ImageType::RegionType outputRegion;
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
  VotingFunctionType::Pointer interpolator = VotingFunctionType::New();
  interpolator->SetInputImage(input);
  ResampleImageFilterType::Pointer resample = ResampleImageFilterType::New();
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
