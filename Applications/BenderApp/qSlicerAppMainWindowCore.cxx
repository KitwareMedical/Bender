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
#include <QAction>
#include <QDebug>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMessageBox>

#include "vtkSlicerConfigure.h" // For Slicer_USE_PYTHONQT

// CTK includes
#include <ctkErrorLogWidget.h>
#include <ctkFileDialog.h>
#include <ctkMessageBox.h>
#ifdef Slicer_USE_PYTHONQT
# include <ctkPythonConsole.h>
#endif
#ifdef Slicer_USE_QtTesting
# include <ctkQtTestingUtility.h>
#endif


// SlicerApp includes
#include "qSlicerAbstractModule.h"
#include "qSlicerAppAboutDialog.h"
#include "qSlicerActionsDialog.h"
#include "qSlicerApplication.h"
#include "qSlicerIOManager.h"
#include "qSlicerLayoutManager.h"
#include "qSlicerAppMainWindowCore_p.h"
#include "qSlicerModuleManager.h"
#ifdef Slicer_USE_PYTHONQT
# include "qSlicerPythonManager.h"
#endif
#include "qMRMLUtils.h"

// SlicerLogic includes
#include <vtkSlicerApplicationLogic.h>

// MRML includes
#include <vtkMRMLScene.h>

// VTK includes
#include <vtkSmartPointer.h>
#include <vtkImageData.h>
#include <vtkNew.h>

// vtksys includes
#include <vtksys/SystemTools.hxx>


//---------------------------------------------------------------------------
// qSlicerAppMainWindowCorePrivate methods

//---------------------------------------------------------------------------
qSlicerAppMainWindowCorePrivate::qSlicerAppMainWindowCorePrivate()
  {
#ifdef Slicer_USE_PYTHONQT
  this->PythonConsole = 0;
#endif
  this->ErrorLogWidget = 0;
  }

//---------------------------------------------------------------------------
qSlicerAppMainWindowCorePrivate::~qSlicerAppMainWindowCorePrivate()
{
  delete this->ErrorLogWidget;
}

//-----------------------------------------------------------------------------
// qSlicerAppMainWindowCore methods

//-----------------------------------------------------------------------------
qSlicerAppMainWindowCore::qSlicerAppMainWindowCore(qSlicerAppMainWindow* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerAppMainWindowCorePrivate)
{
  Q_D(qSlicerAppMainWindowCore);
  
  d->ParentWidget = _parent;
  d->ErrorLogWidget = new ctkErrorLogWidget;
  d->ErrorLogWidget->setErrorLogModel(
    qSlicerCoreApplication::application()->errorLogModel());
}

//-----------------------------------------------------------------------------
qSlicerAppMainWindowCore::~qSlicerAppMainWindowCore()
{
}

//-----------------------------------------------------------------------------
CTK_GET_CPP(qSlicerAppMainWindowCore, qSlicerAppMainWindow*, widget, ParentWidget);

#ifdef Slicer_USE_PYTHONQT
//---------------------------------------------------------------------------
ctkPythonConsole* qSlicerAppMainWindowCore::pythonConsole()const
{
  Q_D(const qSlicerAppMainWindowCore);
  if (!d->PythonConsole)
    {
    // Lookup reference of 'PythonConsole' widget
    // and cache the value
    foreach(QWidget * widget, qApp->topLevelWidgets())
      {
      if(widget->objectName().compare(QLatin1String("pythonConsole")) == 0)
        {
        const_cast<qSlicerAppMainWindowCorePrivate*>(d)
          ->PythonConsole = qobject_cast<ctkPythonConsole*>(widget);
        break;
        }
      }
    }
  return d->PythonConsole;
}
#endif

//---------------------------------------------------------------------------
ctkErrorLogWidget* qSlicerAppMainWindowCore::errorLogWidget()const
{
  Q_D(const qSlicerAppMainWindowCore);
  return d->ErrorLogWidget;
}

//---------------------------------------------------------------------------
void qSlicerAppMainWindowCore::onFileAddDataActionTriggered()
{
  qSlicerApplication::application()->ioManager()->openAddDataDialog();
}

//---------------------------------------------------------------------------
void qSlicerAppMainWindowCore::onFileLoadDataActionTriggered()
{
  qSlicerApplication::application()->ioManager()->openAddDataDialog();
}


//---------------------------------------------------------------------------
void qSlicerAppMainWindowCore::onFileImportSceneActionTriggered()
{
  qSlicerApplication::application()->ioManager()->openAddSceneDialog();
}

//---------------------------------------------------------------------------
void qSlicerAppMainWindowCore::onFileLoadSceneActionTriggered()
{
  qSlicerApplication::application()->ioManager()->openLoadSceneDialog();
}

//---------------------------------------------------------------------------
void qSlicerAppMainWindowCore::onFileAddVolumeActionTriggered()
{
  qSlicerApplication::application()->ioManager()->openAddVolumesDialog();
}

//---------------------------------------------------------------------------
void qSlicerAppMainWindowCore::onFileAddTransformActionTriggered()
{
  qSlicerApplication::application()->ioManager()->openAddTransformDialog();
}

//---------------------------------------------------------------------------
void qSlicerAppMainWindowCore::onFileSaveSceneActionTriggered()
{
  qSlicerApplication::application()->ioManager()->openSaveDataDialog();
}

//---------------------------------------------------------------------------
void qSlicerAppMainWindowCore::onSDBSaveToDirectoryActionTriggered()
{
  // Q_D(qSlicerAppMainWindowCore);
  // open a file dialog to let the user choose where to save
  QString tempDir = qSlicerCoreApplication::application()->temporaryPath();
  QString saveDirName = QFileDialog::getExistingDirectory(this->widget(), tr("Slicer Data Bundle Directory (Select Empty Directory)"), tempDir, QFileDialog::ShowDirsOnly);
  //qDebug() << "saveDirName = " << qPrintable(saveDirName);
  if (saveDirName.isEmpty())
    {
    std::cout << "No directory name chosen!" << std::endl;
    return;
    }
  // pass in a screen shot
  QWidget* widget = qSlicerApplication::application()->layoutManager()->viewport();
  QPixmap screenShot = QPixmap::grabWidget(widget);
  // convert to vtkImageData
  vtkSmartPointer<vtkImageData> imageData = vtkSmartPointer<vtkImageData>::New();
  qMRMLUtils::qImageToVtkImageData(screenShot.toImage(), imageData);

  qSlicerIO::IOProperties properties;
  properties["fileName"] = saveDirName;
  properties["screenShot"] = screenShot;
  qSlicerCoreApplication::application()->coreIOManager()
    ->saveNodes("SceneFile", properties);
}

//---------------------------------------------------------------------------
void qSlicerAppMainWindowCore::onSDBSaveToMRBActionTriggered()
{
  //
  // open a file dialog to let the user choose where to save
  // make sure it was selected and add a .mrb to it if needed
  //
  QString fileName = QFileDialog::getSaveFileName(this->widget(), tr("Save Data Bundle File"),
                                                    "", tr("Medical Reality Bundle (*.mrb)"));

  if (fileName.isEmpty())
    {
    std::cout << "No directory name chosen!" << std::endl;
    return;
    }

  if ( !fileName.endsWith(".mrb") )
    {
    fileName += QString(".mrb");
    }
  qSlicerIO::IOProperties properties;
  properties["fileName"] = fileName;
  qSlicerCoreApplication::application()->coreIOManager()
    ->saveNodes("SceneFile", properties);
}

//---------------------------------------------------------------------------
void qSlicerAppMainWindowCore::onSDBSaveToDCMActionTriggered()
{
  // NOT IMPLEMENTED YET
}

//---------------------------------------------------------------------------
void qSlicerAppMainWindowCore::onFileCloseSceneActionTriggered()
{
  qSlicerCoreApplication::application()->mrmlScene()->Clear(false);
}

//---------------------------------------------------------------------------
void qSlicerAppMainWindowCore::onEditRecordMacroActionTriggered()
{
#ifdef Slicer_USE_QtTesting
  qSlicerApplication::application()->testingUtility()->recordTestsBySuffix(QString("xml"));
#endif
}

//---------------------------------------------------------------------------
void qSlicerAppMainWindowCore::onEditPlayMacroActionTriggered()
{
#ifdef Slicer_USE_QtTesting
  qSlicerApplication::application()->testingUtility()->openPlayerDialog();
#endif
}

//---------------------------------------------------------------------------
void qSlicerAppMainWindowCore::onEditUndoActionTriggered()
{
  qSlicerApplication::application()->mrmlScene()->Undo();
}

//---------------------------------------------------------------------------
void qSlicerAppMainWindowCore::onEditRedoActionTriggered()
{
  qSlicerApplication::application()->mrmlScene()->Redo();
}

//---------------------------------------------------------------------------
void qSlicerAppMainWindowCore::setLayout(int layout)
{
  qSlicerApplication::application()->layoutManager()->setLayout(layout);
}

//---------------------------------------------------------------------------
void qSlicerAppMainWindowCore::setLayoutNumberOfCompareViewRows(int num)
{
  qSlicerApplication::application()->layoutManager()->setLayoutNumberOfCompareViewRows(num);
}

//---------------------------------------------------------------------------
void qSlicerAppMainWindowCore::setLayoutNumberOfCompareViewColumns(int num)
{
  qSlicerApplication::application()->layoutManager()->setLayoutNumberOfCompareViewColumns(num);
}

//-----------------------------------------------------------------------------
void qSlicerAppMainWindowCore::onWindowErrorLogActionTriggered(bool show)
{
  Q_D(qSlicerAppMainWindowCore);
  if (show)
    {
    d->ErrorLogWidget->show();
    d->ErrorLogWidget->activateWindow();
    d->ErrorLogWidget->raise();
    }
  else
    {
    d->ErrorLogWidget->close();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAppMainWindowCore::onWindowPythonInteractorActionTriggered(bool show)
{
#ifdef Slicer_USE_PYTHONQT
  ctkPythonConsole* console = this->pythonConsole();
  Q_ASSERT(console);
  if (show)
    {
    console->show();
    console->activateWindow();
    console->raise();
    }
  else
    {
    console->close();
    }
#else
  Q_UNUSED(show);
#endif
}

//---------------------------------------------------------------------------
void qSlicerAppMainWindowCore::onHelpKeyboardShortcutsActionTriggered()
{
  qSlicerActionsDialog actionsDialog(this->widget());
  actionsDialog.setActionsWithNoShortcutVisible(false);
  actionsDialog.setMenuActionsVisible(false);

  // [Ninja]
  actionsDialog.addActions(this->widget()->findChildren<QAction*>(), qSlicerApplication::application()->applicationName() + "Application");
  // [/Ninja]

  // scan the modules for their actions
  QList<QAction*> moduleActions;
  qSlicerModuleManager * moduleManager = qSlicerApplication::application()->moduleManager();
  foreach(const QString& moduleName, moduleManager->modulesNames())
    {
    qSlicerAbstractModule* module =
      qobject_cast<qSlicerAbstractModule*>(moduleManager->module(moduleName));
    if (module)
      {
      moduleActions << module->action();
      }
    }
  if (moduleActions.size())
    {
    actionsDialog.addActions(moduleActions, "Modules");
    }
  // TODO add more actions
  actionsDialog.exec();
}

//---------------------------------------------------------------------------
void qSlicerAppMainWindowCore::onHelpBrowseTutorialsActionTriggered()
{
  QDesktopServices::openUrl(
    QUrl(QString("http://www.slicer.org/slicerWiki/index.php/Documentation/%1.%2/Training")
         .arg(Slicer_VERSION_MAJOR).arg(Slicer_VERSION_MINOR)));
}
//---------------------------------------------------------------------------
void qSlicerAppMainWindowCore::onHelpInterfaceDocumentationActionTriggered()
{
  QDesktopServices::openUrl(
    QUrl(QString("http://wiki.slicer.org/slicerWiki/index.php/Documentation/%1.%2")
         .arg(Slicer_VERSION_MAJOR).arg(Slicer_VERSION_MINOR)));
}
//---------------------------------------------------------------------------
void qSlicerAppMainWindowCore::onHelpSlicerPublicationsActionTriggered()
{
  QDesktopServices::openUrl(QUrl("http://www.slicer.org/publications"));
}
//---------------------------------------------------------------------------
void qSlicerAppMainWindowCore::onHelpAboutSlicerAppActionTriggered()
{
  qSlicerAppAboutDialog about(this->widget());
  about.exec();
}

//---------------------------------------------------------------------------
void qSlicerAppMainWindowCore::onHelpReportBugOrFeatureRequestActionTriggered()
{
  QDesktopServices::openUrl(QUrl("http://public.kitware.com/Bug/search.php?project_id=41"));
}

//---------------------------------------------------------------------------
void qSlicerAppMainWindowCore::onHelpVisualBlogActionTriggered()
{
  QDesktopServices::openUrl(QUrl("http://public.kitware.com/Wiki/Bender"));
}
