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
#include <QFileInfo>

// SlicerQt includes
#include "qSlicerArmaturesIO.h"
#include "qSlicerArmaturesIOOptionsWidget.h"

// Logic includes
#include "vtkSlicerArmaturesLogic.h"
#include "vtkSlicerModelsLogic.h"

// Bender includes
#include "vtkBVHReader.h"

// MRML includes
#include "vtkMRMLArmatureNode.h"
#include "vtkMRMLArmatureNodeHelper.h"
#include "vtkMRMLArmatureStorageNode.h"
#include "vtkMRMLBoneNode.h"
#include "vtkMRMLModelNode.h"

// VTK includes
#include <vtkNew.h>
#include <vtkSmartPointer.h>

//-----------------------------------------------------------------------------
class qSlicerArmaturesIOPrivate
{
public:
  vtkSmartPointer<vtkSlicerArmaturesLogic> ArmaturesLogic;
};

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_Armatures
qSlicerArmaturesIO::qSlicerArmaturesIO(vtkSlicerArmaturesLogic* armaturesLogic, QObject* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerArmaturesIOPrivate)
{
  this->setArmaturesLogic(armaturesLogic);
}

//-----------------------------------------------------------------------------
qSlicerArmaturesIO::~qSlicerArmaturesIO()
{
}

//-----------------------------------------------------------------------------
void qSlicerArmaturesIO::setArmaturesLogic(vtkSlicerArmaturesLogic* newArmaturesLogic)
{
  Q_D(qSlicerArmaturesIO);
  d->ArmaturesLogic = newArmaturesLogic;
}

//-----------------------------------------------------------------------------
vtkSlicerArmaturesLogic* qSlicerArmaturesIO::armaturesLogic()const
{
  Q_D(const qSlicerArmaturesIO);
  return d->ArmaturesLogic;
}

//-----------------------------------------------------------------------------
QString qSlicerArmaturesIO::description()const
{
  return "Armature";
}

//-----------------------------------------------------------------------------
qSlicerIO::IOFileType qSlicerArmaturesIO::fileType()const
{
  return "ArmatureFile";
}

//-----------------------------------------------------------------------------
QStringList qSlicerArmaturesIO::extensions()const
{
  return QStringList()
    << "Armature (*.arm *.vtk *.bvh)";
}

//-----------------------------------------------------------------------------
bool qSlicerArmaturesIO::load(const IOProperties& properties)
{
  Q_D(qSlicerArmaturesIO);
  Q_ASSERT(properties.contains("fileName"));
  QString fileName = properties["fileName"].toString();

  this->setLoadedNodes(QStringList());
  if (d->ArmaturesLogic.GetPointer() == 0)
    {
    return false;
    }

  if (properties.contains("targetArmature") && fileName.endsWith(".bvh"))
    {
    return this->importAnimationFromFile(properties);
    }
  else if (fileName.endsWith(".vtk")
    || fileName.endsWith(".arm")
    || fileName.endsWith(".bvh"))
    {
    vtkMRMLArmatureNode* armatureNode =
      d->ArmaturesLogic->AddArmatureFile(fileName.toLatin1());
    if (!armatureNode)
      {
      return false;
      }

    if (properties.contains("name"))
      {
      std::string uname = this->mrmlScene()->GetUniqueNameByString(
        properties["name"].toString().toLatin1());
      armatureNode->SetName(uname.c_str());
      }

    if (properties.contains("frame"))
      {
      armatureNode->SetFrame(properties["frame"].toUInt());
      }

    this->setLoadedNodes(QStringList(armatureNode->GetID()));
    return true;
    }

  return false;
}

//-----------------------------------------------------------------------------
bool qSlicerArmaturesIO::importAnimationFromFile(const IOProperties& properties)
{
  Q_D(qSlicerArmaturesIO);
  QString fileName = properties["fileName"].toString();
  QString currentArmatureID = properties["targetArmature"].toString();

  vtkMRMLArmatureNode* targetArmature =
    vtkMRMLArmatureNode::SafeDownCast(
      d->ArmaturesLogic->GetMRMLScene()->GetNodeByID(
        currentArmatureID.toLatin1()));
  if (!targetArmature)
    {
    qCritical()<<"Could not find target node. Animation import failed.";
    return false;
    }

  vtkNew<vtkBVHReader> reader;
  reader->SetFileName(fileName.toLatin1());
  reader->Update();
  if (!reader->GetArmature())
    {
    qCritical()<<"Could not read in animation file: "<<fileName
      <<". Make sure the file is valid. Animation import failed.";
    return false;
    };
  reader->SetFrame(properties["frame"].toUInt());
  reader->GetArmature()->SetWidgetState(vtkArmatureWidget::Pose);

  if (!vtkMRMLArmatureNodeHelper::AnimateArmature(
    targetArmature, reader->GetArmature()))
    {
    qCritical()<<"Animation import failed.";
    return false;
    }

  // Kill any potential animation if the armature was a bvh
  vtkMRMLArmatureStorageNode* storageNode =
    targetArmature->GetArmatureStorageNode();
  if (storageNode)
    {
    targetArmature->SetArmatureStorageNode(0);
    d->ArmaturesLogic->GetMRMLScene()->RemoveNode(storageNode);
    }

  return true;
}

//-----------------------------------------------------------------------------
qSlicerIOOptions* qSlicerArmaturesIO::options()const
{
  return new qSlicerArmaturesIOOptionsWidget;
}
