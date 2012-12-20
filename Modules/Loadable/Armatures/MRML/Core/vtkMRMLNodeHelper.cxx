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

// Bender includes
#include "vtkMRMLNodeHelper.h"

// VTK includes
#include <vtkObjectFactory.h>

// STD includes
//#include <algorithm>
//#include <cassert>
#include <sstream>

namespace vtkMRMLNodeHelperTemplates
{
//----------------------------------------------------------------------------
template <typename T> void PrintVector(ostream& of, const T* vec, int n)
{
  for (int i = 0; i < n; ++i)
    {
    of << vec[i];
    if ( n > 1 && i < n - 1)
      {
      of << " ";
      }
    }
}

//----------------------------------------------------------------------------
template <typename T> void PrintQuotedVector(ostream& of, const T* vec, int n)
{
  of << "\"";
  PrintVector(of, vec, n);
  of << "\"";
}

//----------------------------------------------------------------------------
template <typename T> T StringToNumber(std::string num)
{
  std::stringstream ss;
  ss << num;
  T result;
  return ss >> result ? result : 0;
}

//----------------------------------------------------------------------------
template <typename T>
void StringToVector (std::string value, T* vec, int n)
{
  size_t before = 0;
  size_t after = value.find_first_of(" ");

  for (int i = 0; i < n; ++i)
    {
    std::string ss = value.substr(before, after);
    vec[i] = StringToNumber<T>(ss);

    before = after;
    after = value.find(" ", before + 1);
    }
}

}//end namespace


//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkMRMLNodeHelper)

//----------------------------------------------------------------------------
void vtkMRMLNodeHelper::PrintVector(ostream& of, int* vec, int n)
{
  vtkMRMLNodeHelperTemplates::PrintVector<int>(of, vec, n);
}

//----------------------------------------------------------------------------
void vtkMRMLNodeHelper::PrintVector3(ostream& of, int* vec)
{
  vtkMRMLNodeHelperTemplates::PrintVector<int>(of, vec, 3);
}

//----------------------------------------------------------------------------
void vtkMRMLNodeHelper::PrintVector(ostream& of, double* vec, int n)
{
  vtkMRMLNodeHelperTemplates::PrintVector<double>(of, vec, n);
}

//----------------------------------------------------------------------------
void vtkMRMLNodeHelper::PrintVector3(ostream& of, double* vec)
{
  vtkMRMLNodeHelperTemplates::PrintVector<double>(of, vec, 3);
}

//----------------------------------------------------------------------------
void vtkMRMLNodeHelper::PrintQuotedVector(ostream& of, int* vec, int n)
{
  vtkMRMLNodeHelperTemplates::PrintQuotedVector<int>(of, vec, n);
}

//----------------------------------------------------------------------------
void vtkMRMLNodeHelper::PrintQuotedVector3(ostream& of, int* vec)
{
  vtkMRMLNodeHelperTemplates::PrintQuotedVector<int>(of, vec, 3);
}

//----------------------------------------------------------------------------
void vtkMRMLNodeHelper
::PrintQuotedVector(ostream& of, double* vec, int n)
{
  vtkMRMLNodeHelperTemplates::PrintQuotedVector<double>(of, vec, n);
}

//----------------------------------------------------------------------------
void vtkMRMLNodeHelper::PrintQuotedVector3(ostream& of, double* vec)
{
  vtkMRMLNodeHelperTemplates
    ::PrintQuotedVector<double>(of, vec, 3);
}

//----------------------------------------------------------------------------
int vtkMRMLNodeHelper::StringToInt(std::string num)
{
  return vtkMRMLNodeHelperTemplates::StringToNumber<int>(num);
}

//----------------------------------------------------------------------------
double vtkMRMLNodeHelper::StringToDouble(std::string num)
{
  return vtkMRMLNodeHelperTemplates::StringToNumber<double>(num);
}

//----------------------------------------------------------------------------
void vtkMRMLNodeHelper
::StringToVector(std::string value, int* vec, int n)
{
  vtkMRMLNodeHelperTemplates
    ::StringToVector<int>(value, vec, n);
}

//----------------------------------------------------------------------------
void vtkMRMLNodeHelper
::StringToVector3(std::string value, int* vec)
{
  vtkMRMLNodeHelperTemplates
    ::StringToVector<int>(value, vec, 3);
}
//----------------------------------------------------------------------------
void vtkMRMLNodeHelper
::StringToVector(std::string value, double* vec, int n)
{
  vtkMRMLNodeHelperTemplates
    ::StringToVector<double>(value, vec, n);
}

//----------------------------------------------------------------------------
void vtkMRMLNodeHelper
::StringToVector3(std::string value, double* vec)
{
  vtkMRMLNodeHelperTemplates
    ::StringToVector<double>(value, vec, 3);
}

//----------------------------------------------------------------------------
void vtkMRMLNodeHelper::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os, indent);
}
