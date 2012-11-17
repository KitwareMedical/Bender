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

#ifndef __qSlicerAppMainWindow_h
#define __qSlicerAppMainWindow_h

// Qt includes
#include <QMainWindow>
#include <QVariantMap>

// CTK includes
#include <ctkPimpl.h>
#include <ctkVTKObject.h>

// Slicer includes
#include "qBenderAppExport.h"
#include "qSlicerIO.h"

class qSlicerAbstractCoreModule;
class qSlicerModulePanel;
class qSlicerModuleSelectorToolBar;
// [Ninja]
class qMRMLSliceWidget;
// [/Ninja]
class qSlicerAppMainWindowCore;
class qSlicerAppMainWindowPrivate;

class Q_BENDER_APP_EXPORT qSlicerAppMainWindow : public QMainWindow
{
  Q_OBJECT
  QVTK_OBJECT
public:

  typedef QMainWindow Superclass;
  qSlicerAppMainWindow(QWidget *parent=0);
  virtual ~qSlicerAppMainWindow();

  /// Return the main window core.
  qSlicerAppMainWindowCore* core()const;

  /// Return the module selector
  Q_INVOKABLE qSlicerModuleSelectorToolBar* moduleSelector()const;

public slots:
  void setHomeModuleCurrent();
  void restoreToolbars();

protected slots:
  void onModuleLoaded(const QString& moduleName);
  void onModuleAboutToBeUnloaded(const QString& moduleName);
  void onEditApplicationSettingsActionTriggered();
  void onViewExtensionManagerActionTriggered();

  void onFileRecentLoadedActionTriggered();
  void onNewFileLoaded(const qSlicerIO::IOProperties &fileProperties);

  void onMRMLSceneModified(vtkObject*);
  void onLayoutActionTriggered(QAction* action);
  void onLayoutCompareActionTriggered(QAction* action);
  void onLayoutCompareWidescreenActionTriggered(QAction* action);
  void onLayoutCompareGridActionTriggered(QAction* action);
  void onLayoutChanged(int);
  void loadDICOMActionTriggered();

  // [Ninja]
  /// Allows to configure the SliceWidgets at its creation
  void onLayoutCreateSliceWidget(qMRMLSliceWidget* sliceWidget);
  // [/Ninja]

protected:

  /// Connect MainWindow action with slots defined in MainWindowCore
  void setupMenuActions();

  /// Open a popup to warn the user Slicer is not for clinical use.
  void disclaimer();

  /// Forward the dragEnterEvent to the IOManager which will
  /// decide if it could accept a drag/drop or not.
  void dragEnterEvent(QDragEnterEvent *event);

  /// Forward the dropEvent to the IOManager.
  void dropEvent(QDropEvent *event);

  /// Reimplemented to catch show/hide events
  bool eventFilter(QObject* object, QEvent* event);

  virtual void closeEvent(QCloseEvent *event);
  virtual void showEvent(QShowEvent *event);

protected:
  QScopedPointer<qSlicerAppMainWindowPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerAppMainWindow);
  Q_DISABLE_COPY(qSlicerAppMainWindow);
};

#endif
