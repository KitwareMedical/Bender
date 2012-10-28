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
#include "qSlicerArmaturesModuleWidget.h"
#include "ui_qSlicerArmaturesModule.h"
#include "vtkMRMLArmatureNode.h"
#include "vtkMRMLBoneNode.h"
#include "vtkSlicerArmaturesLogic.h"

// Annotations includes
#include <qMRMLSceneAnnotationModel.h>
#include <vtkSlicerAnnotationModuleLogic.h>

// MRML includes
#include <vtkMRMLInteractionNode.h>
#include <vtkMRMLSelectionNode.h>

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_Armatures
class qSlicerArmaturesModuleWidgetPrivate
  : public Ui_qSlicerArmaturesModule
{
  Q_DECLARE_PUBLIC(qSlicerArmaturesModuleWidget);
protected:
  qSlicerArmaturesModuleWidget* const q_ptr;

public:
  typedef Ui_qSlicerArmaturesModule Superclass;
  qSlicerArmaturesModuleWidgetPrivate(qSlicerArmaturesModuleWidget& object);

  vtkSlicerArmaturesLogic* logic() const;
  virtual void setupUi(qSlicerWidget* armatureModuleWidget);
};

//-----------------------------------------------------------------------------
// qSlicerArmaturesModuleWidgetPrivate methods

//-----------------------------------------------------------------------------
qSlicerArmaturesModuleWidgetPrivate
::qSlicerArmaturesModuleWidgetPrivate(qSlicerArmaturesModuleWidget& object)
  : q_ptr(&object)
{
}

//-----------------------------------------------------------------------------
vtkSlicerArmaturesLogic*
qSlicerArmaturesModuleWidgetPrivate::logic() const
{
  Q_Q(const qSlicerArmaturesModuleWidget);
  return vtkSlicerArmaturesLogic::SafeDownCast(q->logic());
}

//-----------------------------------------------------------------------------
void qSlicerArmaturesModuleWidgetPrivate::setupUi(qSlicerWidget* armatureModuleWidget)
{
  Q_Q(qSlicerArmaturesModuleWidget);
  this->Superclass::setupUi(armatureModuleWidget);

  // Armatures
  QObject::connect(this->ArmatureNodeComboBox,
                   SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(setMRMLArmatureNode(vtkMRMLNode*)));
  QObject::connect(this->ArmatureVisibilityCheckBox, SIGNAL(toggled(bool)),
                   q, SLOT(setArmatureVisibility(bool)));

  // Bones
  this->BonesTreeView->annotationModel()->setAnnotationsAreParent(true);
  this->BonesTreeView->setLogic(this->logic()->GetAnnotationsLogic());
  this->BonesTreeView->annotationModel()->setNameColumn(0);
  this->BonesTreeView->annotationModel()->setVisibilityColumn(0);

  this->BonesTreeView->annotationModel()->setCheckableColumn(-1);
  this->BonesTreeView->annotationModel()->setLockColumn(-1);
  this->BonesTreeView->annotationModel()->setEditColumn(-1);
  this->BonesTreeView->annotationModel()->setValueColumn(-1);
  this->BonesTreeView->annotationModel()->setTextColumn(-1);

  this->BonesTreeView->setHeaderHidden(true);

  QObject::connect(this->BonesTreeView,
                   SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(setMRMLBoneNode(vtkMRMLNode*)));

  QAction* addBoneAction = new QAction("Add bone", this->BonesTreeView);
  this->BonesTreeView->prependNodeMenuAction(addBoneAction);
  this->BonesTreeView->prependSceneMenuAction(addBoneAction);
  QObject::connect(addBoneAction, SIGNAL(triggered()),
                   q, SLOT(addAndPlaceBone()));

  // Logic
  q->qvtkConnect(this->logic(), vtkCommand::ModifiedEvent,
                 q, SLOT(updateWidgetFromLogic()));
}

//-----------------------------------------------------------------------------
// qSlicerArmaturesModuleWidget methods

//-----------------------------------------------------------------------------
qSlicerArmaturesModuleWidget::qSlicerArmaturesModuleWidget(QWidget* _parent)
  : Superclass( _parent )
  , d_ptr( new qSlicerArmaturesModuleWidgetPrivate(*this) )
{
}

//-----------------------------------------------------------------------------
qSlicerArmaturesModuleWidget::~qSlicerArmaturesModuleWidget()
{
}

//-----------------------------------------------------------------------------
void qSlicerArmaturesModuleWidget::setup()
{
  Q_D(qSlicerArmaturesModuleWidget);
  d->setupUi(this);
  this->Superclass::setup();
}

//-----------------------------------------------------------------------------
void qSlicerArmaturesModuleWidget
::setMRMLArmatureNode(vtkMRMLArmatureNode* armatureNode)
{
  Q_D(qSlicerArmaturesModuleWidget);
  d->logic()->SetActiveArmature(armatureNode);
}

//-----------------------------------------------------------------------------
void qSlicerArmaturesModuleWidget
::setMRMLArmatureNode(vtkMRMLNode* armatureNode)
{
  this->setMRMLArmatureNode(vtkMRMLArmatureNode::SafeDownCast(armatureNode));
}

//-----------------------------------------------------------------------------
void qSlicerArmaturesModuleWidget::setArmatureVisibility(bool visible)
{
  Q_UNUSED(visible);
  //vtkMRMLArmatureDisplayNode* armatureDisplayNode =
  //  this->mrmlArmatureDisplayNode();
  //this->mrmlArmatureDisplayNode()->SetVisibility(visible);
}

//-----------------------------------------------------------------------------
vtkMRMLArmatureNode* qSlicerArmaturesModuleWidget::mrmlArmatureNode()const
{
  Q_D(const qSlicerArmaturesModuleWidget);
  return vtkMRMLArmatureNode::SafeDownCast(
    d->ArmatureNodeComboBox->currentNode());
}

/*
//-----------------------------------------------------------------------------
vtkMRMLArmatureDisplayNode* qSlicerArmaturesModuleWidget
::mrmlArmatureDisplayNode()
{
  vtkMRMLArmatureNode* armatureNode = this->mrmlArmatureNode();
  return armatureNode ? armatureNode->GetArmatureDisplayNode() : 0;
}
*/

//-----------------------------------------------------------------------------
void qSlicerArmaturesModuleWidget
::setMRMLBoneNode(vtkMRMLBoneNode* boneNode)
{
  Q_D(qSlicerArmaturesModuleWidget);
  //d->logic()->SetActiveBone(boneNode);
  //if (boneNode == 0)
  //  {
  //  d->logic()->SetActiveArmature(this->mrmlArmatureNode());
  //  }
}

//-----------------------------------------------------------------------------
void qSlicerArmaturesModuleWidget
::setMRMLBoneNode(vtkMRMLNode* boneNode)
{
  Q_D(qSlicerArmaturesModuleWidget);
  this->setMRMLBoneNode(vtkMRMLBoneNode::SafeDownCast(boneNode));
}

//-----------------------------------------------------------------------------
void qSlicerArmaturesModuleWidget::addAndPlaceBone()
{
  Q_D(qSlicerArmaturesModuleWidget);
  vtkMRMLSelectionNode* selectionNode = vtkMRMLSelectionNode::SafeDownCast(
    this->mrmlScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
  vtkMRMLInteractionNode* interactionNode =
    vtkMRMLInteractionNode::SafeDownCast(
      this->mrmlScene()->GetNodeByID("vtkMRMLInteractionNodeSingleton"));
  if (!selectionNode || !interactionNode)
    {
    qCritical() << "Invalid scene, no interaction or selection node";
    return;
    }
  selectionNode->SetReferenceActiveAnnotationID("vtkMRMLBoneNode");
  interactionNode->SwitchToSinglePlaceMode();
}

//-----------------------------------------------------------------------------
void qSlicerArmaturesModuleWidget::updateWidgetFromLogic()
{
  Q_D(qSlicerArmaturesModuleWidget);
  vtkMRMLNode* activeNode = d->logic()->GetActiveBone();
  if (activeNode == 0)
    {
    activeNode = d->logic()->GetActiveArmature();
    }
  d->BonesTreeView->setCurrentNode(activeNode);
}
