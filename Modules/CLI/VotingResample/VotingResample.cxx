#if defined(_MSC_VER)
#pragma warning ( disable : 4786 )
#endif

#ifdef __BORLANDC__
#define ITK_LEAN_AND_MEAN
#endif

// ITK includes
#include "itkTimeProbesCollectorBase.h"
#include "itkImage.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "itkResampleImageFilter.h"
#include "itkVotingResampleImageFunction.h"
#include "itkNearestNeighborInterpolateImageFunction.h"
#include "itkLinearInterpolateImageFunction.h"
#include "itkBSplineInterpolateImageFunction.h"

// Bender includes
#include "itkPluginFilterWatcher.h"
#include "itkPluginUtilities.h"

// Volting Resample includes
#include "VotingResample.h"
#include "VotingResampleCLP.h"

// Use an anonymous namespace to keep class types and function names
// from colliding when module is used as shared object module.  Every
// thing should be in an anonymous namespace except for the module
// entry point, e.g. main()
//
namespace
{
template <class T> int DoIt( int argc, char * argv[] );
} // end of anonymous namespace


//----------------------------------------------------------------------------
int main( int argc, char * argv[] )
{
  PARSE_ARGS;

  itk::ImageIOBase::IOPixelType     pixelType;
  itk::ImageIOBase::IOComponentType componentType;

  try
    {
    itk::GetImageType(inputVolume, pixelType, componentType);

    switch( componentType )
      {
      case itk::ImageIOBase::UCHAR:
        return DoIt<unsigned char>( argc, argv );
        break;
      case itk::ImageIOBase::CHAR:
        return DoIt<char>( argc, argv );
        break;
      case itk::ImageIOBase::USHORT:
        return DoIt<unsigned short>( argc, argv );
        break;
      case itk::ImageIOBase::SHORT:
        return DoIt<short>( argc, argv );
        break;
      case itk::ImageIOBase::UINT:
        return DoIt<unsigned int>( argc, argv );
        break;
      case itk::ImageIOBase::INT:
        return DoIt<int>( argc, argv );
        break;
      case itk::ImageIOBase::ULONG:
        return DoIt<unsigned long>( argc, argv );
        break;
      case itk::ImageIOBase::LONG:
        return DoIt<long>( argc, argv );
        break;
      case itk::ImageIOBase::FLOAT:
        return DoIt<float>( argc, argv );
        break;
      case itk::ImageIOBase::DOUBLE:
        return DoIt<double>( argc, argv );
        break;
      case itk::ImageIOBase::UNKNOWNCOMPONENTTYPE:
      default:
        std::cout << "unknown component type" << std::endl;
        break;
      }
    }

  catch( itk::ExceptionObject & excep )
    {
    std::cerr << argv[0] << ": exception caught !" << std::endl;
    std::cerr << excep << std::endl;
    return EXIT_FAILURE;
    }
  return EXIT_SUCCESS;
}

namespace
{

//----------------------------------------------------------------------------
template <class T>
int DoIt( int argc, char *argv[] )
{
  PARSE_ARGS;

  itk::TimeProbesCollectorBase timeCollector;

  typedef T PixelType;
  const unsigned int Dimension = 3;
  typedef itk::Image< PixelType, Dimension > ImageType;

  typedef  itk::ImageFileReader< ImageType > ReaderType;
  typedef  itk::ImageFileWriter< ImageType > WriterType;

  // Read the file
  typename ReaderType::Pointer reader = ReaderType::New();
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
  typename ImageType::Pointer input = reader->GetOutput();

  if (outputSpacing.size() != input->GetImageDimension())
    {
    std::cerr<<"Given spacing doesn't have the same dimension as the input"
      <<"image."<<std::endl;
    return EXIT_FAILURE;
    }

  // Read the file 
  timeCollector.Start("VotingResample");
  typename ImageType::Pointer output =
    VotingResample<ImageType>(input, outputSpacing,
                              CLPProcessInformation, 1, 0 );
  timeCollector.Stop("VotingResample");

  // Write the file
  timeCollector.Start("Write");
  typename WriterType::Pointer writer = WriterType::New();
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

} // end of anonymous namespace
