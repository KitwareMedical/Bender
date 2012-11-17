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

#ifndef __qSlicerAppMainWindowCorePrivate_p_h
#define __qSlicerAppMainWindowCorePrivate_p_h

// Qt includes
#include <QObject>
#include <QPointer>

// CTK includes
class ctkErrorLogWidget;
class ctkPythonConsole;

// SlicerApp includes
#include "qSlicerAppMainWindowCore.h"
#include "qSlicerAppMainWindow.h"

class qSlicerAbstractModule;

//-----------------------------------------------------------------------------
class qSlicerAppMainWindowCorePrivate: public QObject
{
  Q_OBJECT

public:
  explicit qSlicerAppMainWindowCorePrivate();
  virtual ~qSlicerAppMainWindowCorePrivate();

public:
  QPointer<qSlicerAppMainWindow> ParentWidget;
#ifdef Slicer_USE_PYTHONQT
  ctkPythonConsole*           PythonConsole;
#endif
  ctkErrorLogWidget*          ErrorLogWidget;
};

#endif
