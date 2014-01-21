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
#include <QString>
#include <QVector3D>

// Armatures includes
#include "qSlicerArmaturesModuleWidget.h"
#include "ui_qSlicerArmaturesModule.h"

class QAction;
class qSlicerWidget;
class vtkMRMLArmatureNode;
class vtkMRMLBoneNode;
class vtkObject;
class vtkSlicerArmaturesLogic;

//------------------------------------------------------------------------------
// qSlicerArmaturesModuleWidgetPrivate
//------------------------------------------------------------------------------
class qSlicerArmaturesModuleWidgetPrivate
  : public QObject
  , public Ui_qSlicerArmaturesModule
{
  Q_OBJECT

  Q_DECLARE_PUBLIC(qSlicerArmaturesModuleWidget);

protected:
  qSlicerArmaturesModuleWidget* const q_ptr;

public:
  typedef Ui_qSlicerArmaturesModule Superclass;
  qSlicerArmaturesModuleWidgetPrivate(qSlicerArmaturesModuleWidget& object);

  vtkSlicerArmaturesLogic* logic() const;
  virtual void setupUi(qSlicerWidget* armatureModuleWidget);

  QVector3D direction();

  void blockPositionsSignals(bool block);
  void blockArmatureDisplaySignals(bool block);

  void deleteBoneChildren(vtkMRMLBoneNode* boneNode);

  // Set the interaction node of the current scene back to ViewTransform mode
  void stopPlaceBone();

  void populateLoadFromArmature();

public slots:
  // Calls all the update functions
  void updateArmatureWidget(vtkMRMLBoneNode* boneNode);
  void updateArmatureWidget(vtkMRMLArmatureNode* armatureNode);

  // Display positions functions
  void updateArmatureDisplay(vtkMRMLArmatureNode* armatureNode);
  void updateArmatureAdvancedDisplay(vtkMRMLArmatureNode* armatureNode);

  // Hierarchy update functions
  void updateHierarchy(vtkMRMLBoneNode* boneNode);

  // Positions update functions
  void updatePositions(vtkMRMLBoneNode* boneNode);
  void updateAdvancedPositions(vtkMRMLBoneNode* boneNode);

  void setCoordinatesFromBoneNode(vtkMRMLBoneNode* boneNode);
  void setCoordinatesToBoneNode(vtkMRMLBoneNode* boneNode);

  void selectBoneNode(vtkObject*, void*);

protected slots:

  void onPositionTypeChanged();
  void onDistanceChanged(double newDistance);
  void onDirectionChanged(double* direction);
  void onResetPoseClicked();

  void onParentNodeChanged(vtkMRMLNode* node);
  void onLinkedWithParentChanged(int linked);

  void onFrameChanged(double frame);
  void onImportAnimationClicked();

protected:
  // Select/Unselect the current bone node.
  void selectCurrentBoneDisplayNode(int select);

private:
  vtkMRMLArmatureNode* ArmatureNode;
  vtkMRMLBoneNode* BoneNode;
  QAction* AddBoneAction;
  QAction* DeleteBonesAction;
};

