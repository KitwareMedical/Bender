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

#ifndef __qSlicerArmaturesModule_h
#define __qSlicerArmaturesModule_h

// SlicerQt includes
#include "qSlicerLoadableModule.h"

#include "qSlicerArmaturesModuleExport.h"

class qSlicerArmaturesModulePrivate;

/// \ingroup Slicer_QtModules_Armatures
class Q_SLICER_QTMODULES_ARMATURES_EXPORT qSlicerArmaturesModule :
  public qSlicerLoadableModule
{
  Q_OBJECT
  Q_INTERFACES(qSlicerLoadableModule);

public:

  typedef qSlicerLoadableModule Superclass;
  explicit qSlicerArmaturesModule(QObject *parent=0);
  virtual ~qSlicerArmaturesModule();

  qSlicerGetTitleMacro(QTMODULE_TITLE);
  
  /// Help to use the module
  virtual QString helpText()const;

  /// Return acknowledgements
  virtual QString acknowledgementText()const;

  /// Return the authors of the module
  virtual QStringList  contributors()const;

  /// Return a custom icon for the module
  virtual QIcon icon()const;

  /// Return the categories for the module
  virtual QStringList categories()const;

  /// Return the index of the module in the list
  virtual int index()const;

  /// Return the dependencies for the module
  virtual QStringList dependencies()const;

protected:

  /// Initialize the module. Register the volumes reader/writer
  virtual void setup();

  /// Create and return the widget representation associated to this module
  virtual qSlicerAbstractModuleRepresentation * createWidgetRepresentation();

  /// Create and return the logic associated to this module
  virtual vtkMRMLAbstractLogic* createLogic();

protected:
  QScopedPointer<qSlicerArmaturesModulePrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerArmaturesModule);
  Q_DISABLE_COPY(qSlicerArmaturesModule);

};

#endif
