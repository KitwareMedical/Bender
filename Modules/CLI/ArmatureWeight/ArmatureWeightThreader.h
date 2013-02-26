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

#ifndef __ArmatureWeightThreader_h
#define __ArmatureWeightThreader_h

// .NAME ArmatureWeightThreader - Helps handling threads
// .SECTION General Description
// Helps handling threads.
// 
// Disclaimer:
// This class is very preliminary and would necessit much improvement to be
// more than an helper.

// ITK includes
#include <itkMultiThreader.h>
#include <itkSimpleFastMutexLock.h>

// STD includes
#include <vector>

//-------------------------------------------------------------------------------
struct TheadStatusType
{
  int Id;
  int ReturnCode;
  std::string ErrorMessage;
};

//-------------------------------------------------------------------------------
class ArmatureWeightThreader
{
public:
  ArmatureWeightThreader();
  ~ArmatureWeightThreader() {};

  bool CanAddThread();

  // Create a thread that will execute f with the given data.
  void AddThread(itk::ThreadFunctionType f, void* data);

  // Finish successfully the thread with ID id.
  // No check on given ID.
  void Success(int id);

  // Finish UNsuccessfully the thread with ID id.
  // No Check on ID
  void Fail(int id, std::string msg);

  enum ExitStatusType
    {
    Started = 0,
    Failed,
    Successed
    };

  // Get The number of thread currently running
  int GetNumberOfRunningThreads();

  // return if there are any errors.
  bool HasError();

  // Print errors to std::cerr
  void PrintErrors();

  // Kill all threads
  void KillAll();

  // Clear the thread history
  void ClearThreads();

private:
  std::vector<TheadStatusType> Status;
  itk::SimpleFastMutexLock Mutex;
  itk::MultiThreader::Pointer Threader;

  void KillAllThreads();
};

#endif
