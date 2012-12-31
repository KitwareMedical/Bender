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

#ifndef __vtkQuaternionSetGet_h
#define __vtkQuaternionSetGet_h

// .NAME vtkQuaternionSetGe - Quaternion Macro for Set/Get
// .SECTION General Description
// \TO DO: Set Macro

#define vtkGetQuaternionMacro(name, type) \
virtual void Get##name (type &_arg1, type &_arg2, type &_arg3, type &_arg4) \
  { \
  _arg1 = this->name.GetW(); \
  _arg2 = this->name.GetX(); \
  _arg3 = this->name.GetY(); \
  _arg4 = this->name.GetZ(); \
  vtkDebugMacro(<< this->GetClassName() << " (" << this << "): returning " << #name " = (" << _arg1 << "," << _arg2 << "," << _arg3 << "," << _arg4 << ")"); \
  }; \
virtual void Get##name (type _arg[4]) \
  { \
  this->Get##name (_arg[0], _arg[1], _arg[2], _arg[3]);\
  }\

#define vtkGetQuaterniondMacro(name, type) \
vtkGetQuaternionMacro(name, type)\
virtual vtkQuaterniond Get##name () \
  { \
  return this->name;\
  }

#define vtkGetQuaternionfMacro(name, type) \
vtkGetQuaternionMacro(name, type)\
virtual vtkQuaternionf Get##name () \
  { \
  return this->name;\
  }
#endif
