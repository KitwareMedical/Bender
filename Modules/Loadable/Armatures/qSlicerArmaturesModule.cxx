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
  return QStringList() << "" << "Segmentation.Bender";
}

//-----------------------------------------------------------------------------
int qSlicerArmaturesModule::index()const
{
  return 1;
}

//-----------------------------------------------------------------------------
qSlicerArmaturesModule::~qSlicerArmaturesModule()
{
}

//-----------------------------------------------------------------------------
QString qSlicerArmaturesModule::helpText()const
{
  QString help =
  "<p>The Armatures module creates, edits and animates(poses) bone armatures"
  " using forward kinematic.</p>"
  "<p>A bone is defined by its head (start point) and its tail (end point)."
  " Bones are organized in an armature structure that handles"
  " interaction between bones. </p>"
  "<p>An armature has two modes:"
  "<li><b>Rest</b> mode: bones are added, edited or removed.</li>"
  "<li><b>Pose</b> mode: bones are animated. In this mode,"
  " the user can only rotate the bones around its head.</p>"
  "<p>The module is split in three parts:<br>The panel ''Armature'' controls the"
  " properties of all the bones of the armature.<br> The panel ''Bones''"
  " lists the bones hierarchy of the current armature.<br>"
  " The last panel controls the properties of the currently"
  " selected bone.</p>";
  return help;
}

//-----------------------------------------------------------------------------
QString qSlicerArmaturesModule::acknowledgementText()const
{
  QString acknowledgement = tr(
    "<center><table border=\"0\"><tr>"
    "<td><img src=\":AFRL-100.png\" "
    "alt\"Air Force Research Laboratory\"></td>"
    "</tr></table></center>"
    "This work is supported by Air Force Research Laboratory (AFRL)");
  return acknowledgement;
}

//-----------------------------------------------------------------------------
QStringList qSlicerArmaturesModule::contributors()const
{
  QStringList moduleContributors;
  moduleContributors
    << QString("Johan Andruejol (Kitware)")
    << QString("Julien Finet (Kitware)");
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
