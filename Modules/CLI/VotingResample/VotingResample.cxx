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

Program:   Maverick
Module:    $RCSfile: VotingResample.cxx, v $

Copyright (c) Kitware Inc. 28 Corporate Drive,
Clifton Park, NY, 12065, USA.

All rights reserved. No part of this software may be reproduced, distributed,
or modified, in any form or by any means, without permission in writing from
Kitware Inc.

IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY FOR
DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY DERIVATIVES THEREOF,
EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE IS PROVIDED ON AN
"AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE NO OBLIGATION TO PROVIDE
MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.

=========================================================================*/

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
    VotingResample<ImageType>(input,
                              outputSpacing,
                              highPrecedenceLabels,
                              lowPrecedenceLabels,
                              radius,
                              autoadjustSpacing,
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
