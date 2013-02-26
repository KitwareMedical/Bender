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

  This file was originally developed by Johan Andruejol, Kitware Inc.

=========================================================================*/

#include "ArmatureWeightThreader.h"

// ITK includes
#include <itkMultiThreader.h>
#include <itkSimpleFastMutexLock.h>
#include <itksys/SystemTools.hxx>

// STD includes
#include <sstream>

//-----------------------------------------------------------------------------
ArmatureWeightThreader::ArmatureWeightThreader()
{
  this->Threader = itk::MultiThreader::New();
}

//-----------------------------------------------------------------------------
bool ArmatureWeightThreader::CanAddThread()
{
  return this->GetNumberOfRunningThreads()
    < this->Threader->GetNumberOfThreads();
}

//-----------------------------------------------------------------------------
void ArmatureWeightThreader::AddThread(itk::ThreadFunctionType f, void* data)
{
  while (! this->CanAddThread())
    {
    itksys::SystemTools::Delay(100);
    }

  if (this->HasError())
    {
    return;
    }

  this->Mutex.Lock();
  TheadStatusType status;
  status.ErrorMessage = "Started";
  status.ReturnCode = ArmatureWeightThreader::Started;
  this->Mutex.Unlock();

  status.Id = this->Threader->SpawnThread(f, data);

  this->Mutex.Lock();
  this->Status.push_back(status);
  this->Mutex.Unlock();

}

//-----------------------------------------------------------------------------
void ArmatureWeightThreader::Success(int id)
{
  for (std::vector<TheadStatusType>::iterator it = this->Status.begin();
    it != this->Status.end(); ++it)
    {
    if ((*it).Id == id)
      {
      this->Mutex.Lock();
      it->ReturnCode = ArmatureWeightThreader::Successed;
      it->ErrorMessage = "Success";
      this->Mutex.Unlock();
      }
    }
}

//-----------------------------------------------------------------------------
void ArmatureWeightThreader::Fail(int id, std::string msg)
{
  for (std::vector<TheadStatusType>::iterator it = this->Status.begin();
    it != this->Status.end(); ++it)
    {
    if ((*it).Id == id)
      {
      this->Mutex.Lock();
      it->ReturnCode = ArmatureWeightThreader::Failed;
      it->ErrorMessage = msg;
      this->Mutex.Unlock();
      }
    }

  this->KillAll();
}

//-----------------------------------------------------------------------------
int ArmatureWeightThreader::GetNumberOfRunningThreads()
{
  int runningThread = 0;
  for (std::vector<TheadStatusType>::iterator it = this->Status.begin();
    it != this->Status.end(); ++it)
    {
    if ((*it).ReturnCode == ArmatureWeightThreader::Started)
      {
      ++runningThread;
      }
    }
  return runningThread;
}

//-----------------------------------------------------------------------------
bool ArmatureWeightThreader::HasError()
{
  for (std::vector<TheadStatusType>::iterator it = this->Status.begin();
    it != this->Status.end(); ++it)
    {
    if ((*it).ReturnCode == ArmatureWeightThreader::Failed)
      {
      return true;
      }
    }
  return false;
}

//-----------------------------------------------------------------------------
void ArmatureWeightThreader::PrintErrors()
{
  for (std::vector<TheadStatusType>::iterator it = this->Status.begin();
    it != this->Status.end(); ++it)
    {
    if ((*it).ReturnCode == ArmatureWeightThreader::Failed)
      {
      std::cerr<<"Thread #"<<(*it).Id<<" failed with the error message: "
        <<std::endl<<(*it).ErrorMessage<<std::endl;
      }
    }
}

//-----------------------------------------------------------------------------
void ArmatureWeightThreader::KillAll()
{
  this->KillAllThreads();
  while (this->GetNumberOfRunningThreads() > 0)
    {
    itksys::SystemTools::Delay(100);
    }
}

//-----------------------------------------------------------------------------
void ArmatureWeightThreader::KillAllThreads()
{
  for (std::vector<TheadStatusType>::iterator it = this->Status.begin();
    it != this->Status.end(); ++it)
    {
    if ((*it).ReturnCode == ArmatureWeightThreader::Started)
      {
      this->Threader->TerminateThread((*it).Id);

      this->Mutex.Lock();
      (*it).ReturnCode = ArmatureWeightThreader::Failed;
      (*it).ErrorMessage = "Killed";
      this->Mutex.Unlock();
      }
    }
}

//-----------------------------------------------------------------------------
void ArmatureWeightThreader::ClearThreads()
{
  this->KillAll();

  this->Mutex.Lock();
  this->Status.clear();
  this->Mutex.Unlock();
}
