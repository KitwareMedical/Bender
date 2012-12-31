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

#ifndef __qSlicerArmaturesModuleWidget_h
#define __qSlicerArmaturesModuleWidget_h

// Armatures includes
#include "qSlicerArmaturesModuleExport.h"
class qSlicerArmaturesModuleWidgetPrivate;
class vtkMRMLArmatureNode;
class vtkMRMLBoneNode;
class vtkMRMLNode;

// SlicerQt includes
#include "qSlicerAbstractModuleWidget.h"

// CTK includes
#include <ctkVTKObject.h>

/// \ingroup Slicer_QtModules_Armatures
class Q_SLICER_QTMODULES_ARMATURES_EXPORT qSlicerArmaturesModuleWidget
  : public qSlicerAbstractModuleWidget
{
  Q_OBJECT
  QVTK_OBJECT

public:

  typedef qSlicerAbstractModuleWidget Superclass;
  qSlicerArmaturesModuleWidget(QWidget *parent=0);
  virtual ~qSlicerArmaturesModuleWidget();

  /// Return the current armature node if any, 0 otherwise.
  /// \sa mrmlArmatureDisplayNode()
  vtkMRMLArmatureNode* mrmlArmatureNode()const;

  /// Return the current bone node if any, 0 otherwise.
  /// \sa mrmlArmatureDisplayNode()
  vtkMRMLBoneNode* mrmlBoneNode()const;

  /// Return the display node of the current armature node if any, 0 otherwise.
  /// \sa mrmlArmatureNode()
  //vtkMRMLArmatureDisplayNode* mrmlArmatureDisplayNode()const;

  /// Reimplemented to set the bone node the default bone annotation.
  virtual void enter();

public slots:
   /// Set the mrml scene.
   /// Listens to add added node to select the newly added armature node
   /// as selected.
    virtual void setMRMLScene(vtkMRMLScene* scene);

  /// Set \a armatureNode as current.
  /// \sa setMRMLArmatureNode(vtkMRMLNode*) setMRMLNode(vtkMRMLNode* node)
  void setMRMLArmatureNode(vtkMRMLArmatureNode* armatureNode);
  /// Utility function to conveniently connect signals/slots.
  /// \sa setMRMLArmatureNode(vtkMRMLArmatureNode*)
  /// \sa setMRMLNode(vtkMRMLNode* node)
  void setMRMLArmatureNode(vtkMRMLNode* armatureNode);
  /// Set \a boneNode as current.
  /// \sa setMRMLBoneNode(vtkMRMLNode*) setMRMLNode(vtkMRMLNode* node)
  void setMRMLBoneNode(vtkMRMLBoneNode* boneNode);

  /// Utility function called when a bone or an armature is selected.
  /// \sa setMRMLBoneNode(vtkMRMLBoneNode*)
  /// \sa setMRMLArmatureNode(vtkMRMLArmatureNode*)
  /// \sa setMRMLArmatureNode(vtkMRMLNode*)
  void setMRMLNode(vtkMRMLNode* node);

  /// Set the visibility of the current armature node.
  /// \sa vtkMRMLArmatureDisplayNode::Visibility
  void setArmatureVisibility(bool visible);

  /// Create a bone and start the mouse mode to place it.
  void addAndPlaceBone();

  /// Delete the currently selected bone and all its children (if any).
  void deleteBones();

protected slots:
  /// Update the GUI from the armatures logic.
  void updateWidgetFromLogic();

  /// Update the GUI from the \a the current armature node.
  /// \sa updateWidgetFromBoneNode()
  void updateWidgetFromArmatureNode();

  /// Update the GUI from the \a the current bone node.
  /// \sa updateWidgetFromArmatureNode()
  void updateWidgetFromBoneNode();

  /// Update the GUI for the \a selected node.
  /// \sa updateWidgetFromArmatureNode(), updateWidgetFromBoneNode()
  void onTreeNodeSelected(vtkMRMLNode* node);

  void updateCurrentMRMLArmatureNode();
  void updateCurrentMRMLBoneNode();

  /// Update the selected armature if the new bone added is an armature
  void onMRMLNodeAdded(vtkObject*, void*);

protected:
  QScopedPointer<qSlicerArmaturesModuleWidgetPrivate> d_ptr;

  virtual void setup();

private:
  Q_DECLARE_PRIVATE(qSlicerArmaturesModuleWidget);
  Q_DISABLE_COPY(qSlicerArmaturesModuleWidget);
};

#endif
