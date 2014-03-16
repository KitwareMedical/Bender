#include <iostream>
#include <string>

#include "itkDifferenceImageFilter.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "VotingResample.h"

/** Main function */
int main(int argc, char* argv[])
{
  // Check arguments
  if(argc<2)
    {
    std::cout << "Usage = VotingResampleTest1 <FileName> <FileName>" 
              << std::endl;
    return EXIT_FAILURE;
    }


  // Test Specific Constants
  unsigned int failedPixelTolerance = 0;

  // Typedefs and constants
  const   unsigned int                                Dimension = 3;
  typedef unsigned short                              PixelType;
  typedef itk::Image< PixelType, Dimension >          ImageType;
  typedef itk::ImageFileReader< ImageType >           ReaderType;
  typedef itk::ImageFileWriter< ImageType >           WriterType;
  typedef itk::DifferenceImageFilter< ImageType, ImageType > 
                                                      DifferenceFilterType;

  // Read data testing data
  ReaderType::Pointer reader = ReaderType::New();
  reader->SetFileName(argv[1]);
  reader->Update();
  ImageType::Pointer inputImage = reader->GetOutput();
  
  // Read valid data
  ReaderType::Pointer goldReader = ReaderType::New();
  goldReader->SetFileName(argv[2]);
  goldReader->Update();
  ImageType::Pointer goldImage = goldReader->GetOutput();
  
  // Resample the test image
  ImageType::Pointer output = VotingResample(inputImage, true);

  // Compare the resampling with the gold standard
  DifferenceFilterType::Pointer diffFilter = DifferenceFilterType::New();
  diffFilter->SetValidInput(goldImage);
  diffFilter->SetTestInput(output);
  diffFilter->UpdateLargestPossibleRegion();
  unsigned int failedPixels = diffFilter->GetNumberOfPixelsWithDifferences();
  std::cerr << "Number of failed Pixels: " << failedPixels << std::endl;

  if(failedPixels > failedPixelTolerance)
    {
    return EXIT_FAILURE;
    }
  else
    {
    return EXIT_SUCCESS;
    }
}
