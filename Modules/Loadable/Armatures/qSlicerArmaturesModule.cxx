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
  "<p>The Armature Module creates, edit and animate armature and bone widget"
  " annotations that support forward kinematic.</p>"
  "<p>A bone is defined by its head (start point) and its tail (end point)."
  " Multiples bones can be gathered in an armature structure that handles the"
  " interaction between the bones. </p>"
  "<p>An armature has two modes:"
  "<li>In <b>Rest</b> mode, the bones can be added, removed and edited.</li>"
  "<li>In <b>Pose</b> mode, the bones can animated. In this mode,"
  " the user can only rotate the bones in the camera plane.</p>"
  "<p>The module has three panels that allow the user to interact with the"
  " armature or the bones.<br>The panel ''Armature'' allows to modify"
  " properties on all the bones of the armature.<br> The panel ''Bones''"
  " contains the structure of the selected armatures with all its bones.<br>"
  " Finally the last panel allows to modify the properties of the currently"
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
