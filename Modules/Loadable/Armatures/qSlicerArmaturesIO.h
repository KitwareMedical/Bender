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

#ifndef __qSlicerArmaturesIO_h
#define __qSlicerArmaturesIO_h

//Qt includes
#include <QHash>

// SlicerQt includes
#include "qSlicerFileReader.h"
class qSlicerArmaturesIOPrivate;

// Slicer includes
class qSlicerIOOptions;

// Bender includes
class vtkArmatureWidget;
class vtkBoneWidget;

// Armatures includes
class vtkMRMLArmatureNode;
class vtkMRMLBoneNode;
class vtkSlicerArmaturesLogic;

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_Armatures
class qSlicerArmaturesIO
  : public qSlicerFileReader
{
  Q_OBJECT
public:
  typedef qSlicerFileReader Superclass;
  qSlicerArmaturesIO(vtkSlicerArmaturesLogic* armaturesLogic = 0, QObject* parent = 0);
  virtual ~qSlicerArmaturesIO();

  void setArmaturesLogic(vtkSlicerArmaturesLogic* armaturesLogic);
  vtkSlicerArmaturesLogic* armaturesLogic()const;

  virtual QString description()const;
  virtual IOFileType fileType()const;
  virtual QStringList extensions()const;

  virtual bool load(const IOProperties& properties);

  /// Returns a list of options for the reader. qSlicerIOOptions can be
  /// derived and have a UI associated to it (i.e. qSlicerIOOptionsWidget).
  /// Warning: you are responsible for freeing the memory of the returned
  /// options
  virtual qSlicerIOOptions* options() const;

protected:
  QScopedPointer<qSlicerArmaturesIOPrivate> d_ptr;

  bool importAnimationFromFile(const IOProperties& properties);

private:
  Q_DECLARE_PRIVATE(qSlicerArmaturesIO);
  Q_DISABLE_COPY(qSlicerArmaturesIO);
};

#endif
