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

#ifndef __vtkBenderWidgetHelper_h
#define __vtkBenderWidgetHelper_h

#include <vtkCommand.h>
#include <vtkMath.h>
#include <vtkObject.h>
#include <vtkObjectFactory.h>
#include <vtkSmartPointer.h>
#include <vtkTimeStamp.h>

#include <vector>

// Helper file for testing

static bool CompareVector3(const double* v1, const double* v2)
{
  double diff[3];
  vtkMath::Subtract(v1, v2, diff);
  if (vtkMath::Dot(diff, diff) < 1e-6)
    {
    return true;
    }

  return false;
}

class vtkSpy : public vtkCommand
{
public:
  vtkTypeMacro(vtkSpy, vtkCommand);
  static vtkSpy *New(){return new vtkSpy;}
  virtual void Execute(vtkObject *caller, unsigned long eventId, void *callData);
  // All the event catched are pushed in the CalledEvents vector
  std::vector<unsigned long> CalledEvents;
  void ClearEvents() {this->CalledEvents.clear();};
  bool Verbose;
protected:
  vtkSpy():Verbose(false){}
  virtual ~vtkSpy(){}
};

//---------------------------------------------------------------------------
inline void vtkSpy::Execute(
  vtkObject *vtkcaller, unsigned long eid, void *vtkNotUsed(calldata))
{
  this->CalledEvents.push_back(eid);
  if (this->Verbose)
    {
    std::cout << "vtkSpy: event:" << eid
              << " (" << vtkCommand::GetStringFromEventId(eid) << ")"
              << " time: " << vtkTimeStamp()
              << std::endl;
    }
}

#endif
