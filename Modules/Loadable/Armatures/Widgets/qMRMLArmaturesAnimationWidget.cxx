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

// Armatures includes
#include "qMRMLArmaturesAnimationWidget.h"
#include "ui_qMRMLArmaturesAnimationWidget.h"

// Bender includes
#include "vtkMRMLArmatureNode.h"
#include <vtkMRMLArmatureStorageNode.h>

// Slicer includes
#include <qSlicerFileDialog.h>
#include <qSlicerIOManager.h>

// VTK includes
#include <vtkCollection.h>
#include <vtkNew.h>

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_Annotation
class qMRMLArmaturesAnimationWidgetPrivate :
  public Ui_qMRMLArmaturesAnimationWidget
{
  Q_DECLARE_PUBLIC(qMRMLArmaturesAnimationWidget);
protected:
  qMRMLArmaturesAnimationWidget* const q_ptr;
public:
  qMRMLArmaturesAnimationWidgetPrivate(qMRMLArmaturesAnimationWidget& object);

  void setup();

  vtkMRMLArmatureNode* ArmatureNode;
};


//-----------------------------------------------------------------------------
// qMRMLArmaturesAnimationWidgetPrivate methods

//-----------------------------------------------------------------------------
qMRMLArmaturesAnimationWidgetPrivate
::qMRMLArmaturesAnimationWidgetPrivate(qMRMLArmaturesAnimationWidget& object)
  : q_ptr(&object)
{
  this->ArmatureNode = 0;
  this->setup();
}

//-----------------------------------------------------------------------------
void qMRMLArmaturesAnimationWidgetPrivate::setup()
{
  Q_Q(qMRMLArmaturesAnimationWidget);

  this->setupUi(q);

  // -- Armature Pose --
  QObject::connect(this->FrameSliderWidget, SIGNAL(valueChanged(double)),
    q, SLOT(onFrameChanged(double)));

  QObject::connect(this->ImportAnimationPushButton, SIGNAL(clicked()),
    q, SLOT(onImportAnimationClicked()));
}

//-----------------------------------------------------------------------------
// qSlicerArmaturesModuleWidget methods

//-----------------------------------------------------------------------------
qMRMLArmaturesAnimationWidget::qMRMLArmaturesAnimationWidget(QWidget* _parent)
  : Superclass( _parent )
  , d_ptr( new qMRMLArmaturesAnimationWidgetPrivate(*this) )
{
}

//-----------------------------------------------------------------------------
qMRMLArmaturesAnimationWidget::~qMRMLArmaturesAnimationWidget()
{
}

//-----------------------------------------------------------------------------
void qMRMLArmaturesAnimationWidget
::setMRMLArmatureNode(vtkMRMLArmatureNode* armatureNode)
{
  Q_D(qMRMLArmaturesAnimationWidget);
  if (armatureNode == d->ArmatureNode)
    {
    return;
    }

  this->qvtkReconnect(d->ArmatureNode, armatureNode,
    vtkCommand::ModifiedEvent, this, SLOT(updateWidgetFromArmatureNode()));
  d->ArmatureNode = armatureNode;

  this->updateWidgetFromArmatureNode();
}

//-----------------------------------------------------------------------------
void qMRMLArmaturesAnimationWidget
::setMRMLArmatureNode(vtkMRMLNode* armatureNode)
{
  this->setMRMLArmatureNode(vtkMRMLArmatureNode::SafeDownCast(armatureNode));
}

//-----------------------------------------------------------------------------
void qMRMLArmaturesAnimationWidget::onFrameChanged(double newFrame)
{
  Q_D(qMRMLArmaturesAnimationWidget);
  if (!d->ArmatureNode)
    {
    return;
    }

  d->ArmatureNode->SetFrame(static_cast<unsigned int>(newFrame));
}

//-----------------------------------------------------------------------------
void qMRMLArmaturesAnimationWidget::onImportAnimationClicked()
{
  Q_D(qMRMLArmaturesAnimationWidget);
  if (!d->ArmatureNode)
    {
    return;
    }

  // open dialog with bvh file
  qSlicerIO::IOProperties ioProperties;
  ioProperties["targetArmature"] = d->ArmatureNode->GetID();
  vtkNew<vtkCollection> nodes;
  qSlicerApplication::application()->ioManager()->openDialog(
    QString("ArmatureFile"),
    qSlicerFileDialog::Read,
    ioProperties,
    nodes.GetPointer());
}

//-----------------------------------------------------------------------------
void qMRMLArmaturesAnimationWidget::updateWidgetFromArmatureNode()
{
  Q_D(qMRMLArmaturesAnimationWidget);

  vtkMRMLArmatureStorageNode* armatureStorageNode = 0;
  if (d->ArmatureNode)
    {
    armatureStorageNode = d->ArmatureNode->GetArmatureStorageNode();
    }
  d->FrameSliderWidget->setEnabled(armatureStorageNode != 0);
  d->ImportAnimationPushButton->setEnabled(d->ArmatureNode != 0);

  if (!d->ArmatureNode)
    {
    return;
    }

  if (armatureStorageNode)
    {
    d->FrameSliderWidget->setMaximum(
      armatureStorageNode->GetNumberOfFrames() -1);
    }
  d->FrameSliderWidget->setValue(d->ArmatureNode->GetFrame());
}

