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

// .NAME vtkBVHReader - Reads armature files.
// .SECTION Description
// Given a path to an armature files, reads the polydata and outputs an
// armature widget.
// .SECTION See Also
// vtkBVHReader, vtkArmatureWidget,

#ifndef __vtkArmatureReader_h
#define __vtkArmatureReader_h

#include <vtkPolyDataAlgorithm.h>

#include "vtkBenderIOExport.h"

class vtkArmatureWidget;
class vtkPolyData;

class VTK_BENDER_IO_EXPORT vtkArmatureReader : public vtkPolyDataAlgorithm
{
public:
  vtkTypeMacro(vtkArmatureReader, vtkAlgorithm);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  static vtkArmatureReader *New();

  // Description:
  // Set the motion capture file's filename to read.
  // Setting a new filename invalids the current armature (if any).
  void SetFileName(const char* filename);
  const char* GetFileName() const;

  // Description:
  // A simple, non-exhaustive check to see if a file is a valid file.
  static int CanReadFile(const char *filename);

  // Description:
  // Get the armature.
  vtkArmatureWidget* GetArmature();

protected:
  vtkArmatureReader();
  ~vtkArmatureReader();

  std::string FileName;

  vtkArmatureWidget* Armature;
  int ArmatureIsValid;

  virtual int RequestData(
    vtkInformation*, vtkInformationVector**, vtkInformationVector*);
  void InvalidReader();
  int Parse(vtkPolyData*);

private:
  vtkArmatureReader(const vtkArmatureReader&);  // Not implemented.
  void operator=(const vtkArmatureReader&);  // Not implemented.
};

#endif
