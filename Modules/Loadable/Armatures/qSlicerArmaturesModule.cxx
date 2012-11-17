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

// Qt includes
#include <QDebug>
#include <QtPlugin>

// Armatures Logic includes
#include <vtkSlicerArmaturesLogic.h>

// Armatures includes
#include "ArmaturesModuleInstantiator.h"
#include "qSlicerArmaturesModule.h"
#include "qSlicerArmaturesModuleWidget.h"
#include "qSlicerArmaturesIO.h"

// Slicer includes
#include "qSlicerApplication.h"
#include "qSlicerIOManager.h"
#include "qSlicerModuleManager.h"
#include <vtkMRMLSliceViewDisplayableManagerFactory.h>
#include <vtkMRMLThreeDViewDisplayableManagerFactory.h>
#include "vtkSlicerAnnotationModuleLogic.h"
#include "vtkSlicerModelsLogic.h"

//-----------------------------------------------------------------------------
Q_EXPORT_PLUGIN2(qSlicerArmaturesModule, qSlicerArmaturesModule);

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_Armatures
class qSlicerArmaturesModulePrivate
{
public:
  qSlicerArmaturesModulePrivate();
};

//-----------------------------------------------------------------------------
// qSlicerArmaturesModulePrivate methods

//-----------------------------------------------------------------------------
qSlicerArmaturesModulePrivate::qSlicerArmaturesModulePrivate()
{
}

//-----------------------------------------------------------------------------
// qSlicerArmaturesModule methods

//-----------------------------------------------------------------------------
qSlicerArmaturesModule::qSlicerArmaturesModule(QObject* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerArmaturesModulePrivate)
{
}

//-----------------------------------------------------------------------------
QStringList qSlicerArmaturesModule::categories()const
{
  return QStringList() << "" << "Segmentation";
}

//-----------------------------------------------------------------------------
int qSlicerArmaturesModule::index()const
{
  return 0;
}

//-----------------------------------------------------------------------------
qSlicerArmaturesModule::~qSlicerArmaturesModule()
{
}

//-----------------------------------------------------------------------------
QString qSlicerArmaturesModule::helpText()const
{
  QString help =
    "The module is for armature and bone animation.";
  return help;
}

//-----------------------------------------------------------------------------
QString qSlicerArmaturesModule::acknowledgementText()const
{
  return "This work was supported by AFRL";
}

//-----------------------------------------------------------------------------
QStringList qSlicerArmaturesModule::contributors()const
{
  QStringList moduleContributors;
  moduleContributors << QString("Julien Finet (Kitware)")
    << QString("Johan Andruejol (Kitware)");
  return moduleContributors;
}

//-----------------------------------------------------------------------------
QStringList qSlicerArmaturesModule::dependencies()const
{
  return QStringList() << "Models" << "Annotations";
}

//-----------------------------------------------------------------------------
QIcon qSlicerArmaturesModule::icon()const
{
  return QIcon(":/Icons/Armatures.png");
}

//-----------------------------------------------------------------------------
void qSlicerArmaturesModule::setup()
{
  this->Superclass::setup();

  // Configure Armatures logic
  vtkSlicerArmaturesLogic* armaturesLogic =
    vtkSlicerArmaturesLogic::SafeDownCast(this->logic());
  qSlicerAbstractCoreModule* modelsModule =
    qSlicerCoreApplication::application()->moduleManager()->module("Models");
  if (modelsModule)
    {
    vtkSlicerModelsLogic* modelsLogic =
      vtkSlicerModelsLogic::SafeDownCast(modelsModule->logic());
    armaturesLogic->SetModelsLogic(modelsLogic);
    }

  qSlicerAbstractCoreModule* annotationsModule =
    qSlicerCoreApplication::application()->moduleManager()->module("Annotations");
  if (annotationsModule)
    {
    vtkSlicerAnnotationModuleLogic* annotationsLogic =
      vtkSlicerAnnotationModuleLogic::SafeDownCast(annotationsModule->logic());
    armaturesLogic->SetAnnotationsLogic(annotationsLogic);
    }

  // Register displayable manager
  vtkMRMLThreeDViewDisplayableManagerFactory::GetInstance()
    ->RegisterDisplayableManager("vtkMRMLArmatureDisplayableManager");
  //vtkMRMLSliceViewDisplayableManagerFactory::GetInstance()
  //  ->RegisterDisplayableManager("vtkMRMLArmatureDisplayableManager");

  // Register IOs
  qSlicerIOManager* ioManager = qSlicerApplication::application()->ioManager();
  ioManager->registerIO(new qSlicerArmaturesIO(armaturesLogic, this));
}

//-----------------------------------------------------------------------------
qSlicerAbstractModuleRepresentation * qSlicerArmaturesModule::createWidgetRepresentation()
{
  return new qSlicerArmaturesModuleWidget;
}

//-----------------------------------------------------------------------------
vtkMRMLAbstractLogic* qSlicerArmaturesModule::createLogic()
{
  return vtkSlicerArmaturesLogic::New();
}
