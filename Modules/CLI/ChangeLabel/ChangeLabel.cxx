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

// ITK includes
#include "itkChangeLabelImageFilter.h"
#include "itkImageFileWriter.h"

#include "itkPluginUtilities.h"
#include "ChangeLabelCLP.h"

// STD includes
#include <numeric>

// Use an anonymous namespace to keep class types and function names
// from colliding when module is used as shared object module.  Every
// thing should be in an anonymous namespace except for the module
// entry point, e.g. main()
//
namespace
{
template <class T> int DoIt( int argc, char * argv[] );
} // end of anonymous namespace

int main( int argc, char * argv[] )
{

  PARSE_ARGS;

  itk::ImageIOBase::IOPixelType     pixelType;
  itk::ImageIOBase::IOComponentType componentType;

  try
    {
    itk::GetImageType(InputVolume, pixelType, componentType);

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

// Use an anonymous namespace to keep class types and function names
// from colliding when module is used as shared object module.  Every
// thing should be in an anonymous namespace except for the module
// entry point, e.g. main()
//
namespace
{

template <class T>
int DoIt( int argc, char * argv[] )
{

  PARSE_ARGS;

  typedef T InputPixelType;
  typedef T OutputPixelType;

  typedef itk::Image<InputPixelType,  3> InputImageType;
  typedef itk::Image<OutputPixelType, 3> OutputImageType;

  typedef itk::ImageFileReader<InputImageType>  ReaderType;
  typedef itk::ImageFileWriter<OutputImageType> WriterType;

  typedef itk::ChangeLabelImageFilter<InputImageType, InputImageType>
    FilterType;

  typename ReaderType::Pointer reader1 = ReaderType::New();
  itk::PluginFilterWatcher watchReader1(reader1, "Read Volume",
                                        CLPProcessInformation);

  reader1->SetFileName( InputVolume.c_str() );

  typename FilterType::Pointer filter = FilterType::New();
  itk::PluginFilterWatcher watchFilter(filter,
                                       "Change label",
                                       CLPProcessInformation);

  filter->SetInput( 0, reader1->GetOutput() );

  int totalNumberOfInputLabels =
    std::accumulate(InputLabelNumber.begin(), InputLabelNumber.end(), 0);
  if (OutputLabel.size() != InputLabelNumber.size()
      || InputLabel.size() <= 0
      || InputLabelNumber.size() <= 0
      || OutputLabel.size() <= 0
      || InputLabel.size() != totalNumberOfInputLabels
      )
    {
    std::cerr << "Error, bad input sizes:" << std::endl
      << "InputLabel size: " << InputLabel.size() << "\t"
      << "InputLabelNumber size: " << InputLabelNumber.size() << "\t"
      << "OutputLabel size: " << OutputLabel.size() << std::endl
      << "The sum of all the InputLabelNumber values shoud be equal to the"
      << " size of InputLabel."<<std::endl
      << "The size of OutputLabel should be the same as the size of"
      << " InputLabelNumber." << std::endl;
    return EXIT_FAILURE;
    }
  size_t k = 0;
  for (size_t i = 0; i < InputLabelNumber.size(); ++i)
    {
    const int outputLabel = OutputLabel[i];
    const size_t numberOfLabels = InputLabelNumber[i];
    for (size_t j = 0; j < numberOfLabels; ++j)
      {
      //const int inputLabel = InputLabel[i][j];
      const int inputLabel = InputLabel[k + j];
      filter->SetChange(inputLabel, outputLabel);
      }
    k += numberOfLabels;
    }

  typename WriterType::Pointer writer = WriterType::New();
  itk::PluginFilterWatcher watchWriter(writer,
                                       "Write Volume",
                                       CLPProcessInformation);
  writer->SetFileName( OutputVolume.c_str() );
  writer->SetInput( filter->GetOutput() );
  writer->SetUseCompression(1);
  writer->Update();

  return EXIT_SUCCESS;
}

} // end of anonymous namespace
