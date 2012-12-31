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

#ifndef __vtkMRMLNodeHelper_h
#define __vtkMRMLNodeHelper_h

// VTK Includes
#include <vtkObject.h>

// STD includes
#include <string.h>

// Armatures includes
#include "vtkBenderArmaturesModuleMRMLCoreExport.h"

/// \ingroup Bender_MRML
/// \brief Groups helpers functions
///

class VTK_BENDER_ARMATURES_MRML_CORE_EXPORT vtkMRMLNodeHelper
  : public vtkObject
{
public:
  static vtkMRMLNodeHelper *New();
  vtkTypeMacro(vtkMRMLNodeHelper,vtkObject);
  virtual void PrintSelf(ostream& os, vtkIndent indent);

  static void PrintVector(ostream& of, int* vec, int n);
  static void PrintVector3(ostream& of, int* vec);
  static void PrintVector(ostream& of, double* vec, int n);
  static void PrintVector3(ostream& of, double* vec);

  static void PrintQuotedVector(ostream& of, int* vec, int n);
  static void PrintQuotedVector3(ostream& of, int* vec);
  static void PrintQuotedVector(ostream& of, double* vec, int n);
  static void PrintQuotedVector3(ostream& of, double* vec);

  static int StringToInt(std::string num);
  static double StringToDouble(std::string num);

  static void StringToVector(std::string value, int* vec, int n);
  static void StringToVector3(std::string value, int* vec);
  static void StringToVector(std::string value, double* vec, int n);
  static void StringToVector3(std::string value, double* vec);

protected:
  vtkMRMLNodeHelper() {};
  ~vtkMRMLNodeHelper() {};

private:
  vtkMRMLNodeHelper(const vtkMRMLNodeHelper&);  // Not implemented.
  void operator=(const vtkMRMLNodeHelper&);  // Not implemented.

};
#endif
