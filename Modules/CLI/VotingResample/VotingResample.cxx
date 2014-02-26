#if defined(_MSC_VER)
#pragma warning ( disable : 4786 )
#endif

#ifdef __BORLANDC__
#define ITK_LEAN_AND_MEAN
#endif

#include "VotingResampleCLP.h"

#include "itkTimeProbesCollectorBase.h"

#include "itkObjectFactoryBase.h"
#include "itkMRMLIDImageIOFactory.h"

#include "itkImage.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "itkResampleImageFilter.h"
#include "itkVotingResampleImageFunction.h"
#include "itkNearestNeighborInterpolateImageFunction.h"
#include "itkLinearInterpolateImageFunction.h"
#include "itkBSplineInterpolateImageFunction.h"

#include "itkPluginFilterWatcher.h"

#include "VotingResample.h"

int main( int argc, char *argv[] )
{
  PARSE_ARGS;

  itk::MRMLIDImageIOFactory::Pointer mrmlFactory = 
    itk::MRMLIDImageIOFactory::New();
  itk::ObjectFactoryBase::RegisterFactory( mrmlFactory );

  itk::TimeProbesCollectorBase timeCollector;

  typedef   unsigned short  PixelType;
  const     unsigned int    Dimension = 3;
  typedef itk::Image< PixelType, Dimension >  ImageType;

  typedef  itk::ImageFileReader< ImageType >    ReaderType;
  typedef  itk::ImageFileWriter< ImageType >   WriterType;

    // Read the file 
  ReaderType::Pointer reader = ReaderType::New();
  reader->SetFileName( inputVolume.c_str() );
  try
    {
    reader->Update();
    }
  catch( itk::ExceptionObject &err )
    {
    std::cout << "ExceptionObject caught !" << std::endl;
    std::cout << err << std::endl;
    return EXIT_FAILURE;
    }
  
  // Hold our input
  ImageType::Pointer input = reader->GetOutput();

  // Read the file 
  timeCollector.Start("VotingResample");
  ImageType::Pointer output = VotingResample( input, ResampleZ,
                                              CLPProcessInformation,
                                              1, 0 );
  timeCollector.Stop("VotingResample");

  itk::ObjectFactoryBase::UnRegisterFactory( mrmlFactory );

  // Write the file
  timeCollector.Start("Write");
  WriterType::Pointer writer = WriterType::New();
  writer->SetFileName( outputVolume.c_str() );
  writer->SetInput(output);
  try
    {
    writer->Update();
    }
  catch ( itk::ExceptionObject &err )
    {
    std::cout << "Exception Caught!" << err << std::endl;
    return EXIT_FAILURE;
    }
  timeCollector.Stop("Write");

  timeCollector.Report();

  return EXIT_SUCCESS;
}




