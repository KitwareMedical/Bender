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
#include <itkImage.h>
#include <itkImageFileWriter.h>
#include <itkImageRegionIteratorWithIndex.h>
#include <itksys/SystemTools.hxx>

// VTK includes
#include <vtkNew.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataWriter.h>

namespace bender
{
//-------------------------------------------------------------------------------
template <class ImageType>
void IOUtils::WriteImage(typename ImageType::Pointer image,const char* fname)
{
  std::cout << "Write image to " << fname << std::endl;
  typedef typename itk::ImageFileWriter<ImageType> WriterType;
  typename WriterType::Pointer writer = WriterType::New();
  writer->SetFileName(fname);
  writer->SetInput(image);
  writer->SetUseCompression(1);
  writer->Update();
}

//-------------------------------------------------------------------------------
template <class ImageType>
void IOUtils::WriteDebugImage(typename ImageType::Pointer image,
                              const char* name, const char* debugDirectory)
{
  if (!debugDirectory)
    {
    debugDirectory = itksys::SystemTools::GetEnv("TMPDIR");
    if (!debugDirectory)
      {
      std::cout<<"Could not find tmp directory. Image named "
        << name << "not written."<<std::endl;
      return;
      }
    }

  if (!itksys::SystemTools::MakeDirectory(debugDirectory))
    {
    std::cout<<"Could not create the directory: "<< debugDirectory
      << std::endl << " Image named " << name << "not written."<<std::endl;
    return;
    }

  std::string fname = debugDirectory;
  fname += "/";
  fname += name;
  WriteImage<ImageType>(image, fname.c_str());
}

};
