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

// Logic includes
#include "vtkSlicerArmaturesLogic.h"
#include "vtkSlicerModelsLogic.h"

// MRML includes
#include "vtkMRMLModelNode.h"

// VTK includes
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
    << "Armature (*.arm)";
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
  //qDebug() << d->ArmaturesLogic->GetMRMLScene() << d->ArmaturesLogic->GetModelsLogic() << d->ArmaturesLogic->GetModelsLogic()->GetMRMLScene();
  vtkMRMLModelNode* node = d->ArmaturesLogic->AddArmatureFile(
    fileName.toLatin1());
  if (!node)
    {
    return false;
    }
  this->setLoadedNodes( QStringList(QString(node->GetID())) );
  if (properties.contains("name"))
    {
    std::string uname = this->mrmlScene()->GetUniqueNameByString(
      properties["name"].toString().toLatin1());
    node->SetName(uname.c_str());
    }
  return true;
}
