/*==============================================================================

  Program: 3D Slicer

  Copyright (c) Kitware Inc.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Julien Finet, Kitware Inc.
  and was partially funded by NIH grant 3P41RR013218-12S1

==============================================================================*/

#ifndef __qSlicerArmaturesIOOptionsWidget_h
#define __qSlicerArmaturesIOOptionsWidget_h

// CTK includes
#include <ctkPimpl.h>

// SlicerQt includes
#include <qSlicerIOOptionsWidget.h>

// Armatures include
#include "qSlicerArmaturesModuleExport.h"

class qSlicerArmaturesIOOptionsWidgetPrivate;

class Q_SLICER_QTMODULES_ARMATURES_EXPORT qSlicerArmaturesIOOptionsWidget
  : public qSlicerIOOptionsWidget
{
  Q_OBJECT
public:
  typedef qSlicerIOOptionsWidget Superclass;
  qSlicerArmaturesIOOptionsWidget(QWidget *parent=0);
  virtual ~qSlicerArmaturesIOOptionsWidget();

public slots:
  virtual void setFileName(const QString& fileName);
  virtual void setFileNames(const QStringList& fileNames);

protected slots:
  void updateProperties();
  void enableFrameChange(int enable);
  void updateToolTip();

private:
  Q_DECLARE_PRIVATE_D(qGetPtrHelper(qSlicerIOOptions::d_ptr), qSlicerArmaturesIOOptionsWidget);
  Q_DISABLE_COPY(qSlicerArmaturesIOOptionsWidget);
};

#endif
