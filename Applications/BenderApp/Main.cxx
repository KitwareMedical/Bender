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
#include <QList>
#include <QSettings>
#include <QSplashScreen>
#include <QString>
#include <QTimer>
#include <QTranslator>

// Slicer includes
#include "vtkSlicerConfigure.h" // For Slicer_USE_PYTHONQT Slicer_QM_OUTPUT_DIRS, Slicer_INSTALL_QM_DIR

// CTK includes
#include <ctkAbstractLibraryFactory.h>
#ifdef Slicer_USE_PYTHONQT
# include <ctkPythonConsole.h>
#endif

// MRMLWidgets includes
#include <qMRMLEventLoggerWidget.h>

// SlicerApp includes
#include "qSlicerApplication.h"
#include "qSlicerApplicationHelper.h"
#ifdef Slicer_BUILD_CLI_SUPPORT
# include "qSlicerCLIExecutableModuleFactory.h"
# include "qSlicerCLILoadableModuleFactory.h"
#endif
#include "qSlicerCommandOptions.h"
#include "qSlicerCoreModuleFactory.h"
#include "qSlicerLoadableModuleFactory.h"
#include "qSlicerAppMainWindow.h"
#include "qSlicerModuleFactoryManager.h"
#include "qSlicerModuleManager.h"
#include "qSlicerStyle.h"

// ITK includes
#include "itkFactoryRegistration.h"

#ifdef Slicer_USE_PYTHONQT
# include "qSlicerScriptedLoadableModuleFactory.h"
#endif

#ifdef Slicer_USE_PYTHONQT
# include <PythonQtObjectPtr.h>
# include <PythonQtPythonInclude.h>
# include "qSlicerPythonManager.h"
# include "qSlicerSettingsPythonPanel.h"
#endif

// Bender includes
#include "benderVersionConfigure.h" // For Bender_VERSION_FULL

#if defined (_WIN32) && !defined (Slicer_BUILD_WIN32_CONSOLE)
# include <windows.h>
# include <vtksys/SystemTools.hxx>
#endif

namespace
{

#ifdef Slicer_USE_QtTesting
//-----------------------------------------------------------------------------
void setEnableQtTesting()
{
  if (qSlicerApplication::application()->commandOptions()->enableQtTesting() ||
      qSlicerApplication::application()->settings()->value("QtTesting/Enabled").toBool())
    {
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeMenuBar);
    }
}
#endif

#ifdef Slicer_USE_PYTHONQT

//----------------------------------------------------------------------------
void initializePythonConsole(ctkPythonConsole& pythonConsole)
{
  // Create python console
  Q_ASSERT(qSlicerApplication::application()->pythonManager());
  pythonConsole.initialize(qSlicerApplication::application()->pythonManager());

  QStringList autocompletePreferenceList;
  autocompletePreferenceList
      << "slicer" << "slicer.mrmlScene"
      << "qt.QPushButton";
  pythonConsole.completer()->setAutocompletePreferenceList(autocompletePreferenceList);

  //pythonConsole.setAttribute(Qt::WA_QuitOnClose, false);
  pythonConsole.resize(600, 280);

  qSlicerApplication::application()->settingsDialog()->addPanel(
    "Python", new qSlicerSettingsPythonPanel);

  // Show pythonConsole if required
  qSlicerCommandOptions * options = qSlicerApplication::application()->commandOptions();
  if(options->showPythonInteractor() && !options->runPythonAndExit())
    {
    pythonConsole.show();
    pythonConsole.activateWindow();
    pythonConsole.raise();
    }
}
#endif

//----------------------------------------------------------------------------
void showMRMLEventLoggerWidget()
{
  qMRMLEventLoggerWidget* logger = new qMRMLEventLoggerWidget(0);
  logger->setAttribute(Qt::WA_DeleteOnClose);
  logger->setConsoleOutputEnabled(false);
  logger->setMRMLScene(qSlicerApplication::application()->mrmlScene());

  QObject::connect(qSlicerApplication::application(),
                   SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   logger,
                   SLOT(setMRMLScene(vtkMRMLScene*)));

  logger->show();
}

//----------------------------------------------------------------------------
void splashMessage(QScopedPointer<QSplashScreen>& splashScreen, const QString& message)
{
  if (splashScreen.isNull())
    {
    return;
    }
  splashScreen->showMessage(message, Qt::AlignBottom | Qt::AlignHCenter);
  //splashScreen->repaint();
}

//----------------------------------------------------------------------------
void loadTranslations(const QString& dir)
{
  qSlicerApplication * app = qSlicerApplication::application();
  Q_ASSERT(app);

  QString localeFilter =
      QString( QString("*") + app->settings()->value("language").toString());
  localeFilter.resize(3);
  localeFilter += QString(".qm");

  QDir directory(dir);
  QStringList qmFiles = directory.entryList(QStringList(localeFilter));

  foreach(QString qmFile, qmFiles)
    {
    QTranslator* translator = new QTranslator();
    QString qmFilePath = QString(dir + QString("/") + qmFile);

    if(!translator->load(qmFilePath))
      {
      qDebug() << "The File " << qmFile << " hasn't been loaded in the translator";
      return;
      }
    app->installTranslator(translator);
    }
}

//----------------------------------------------------------------------------
void loadLanguage()
{
  qSlicerApplication * app = qSlicerApplication::application();
  Q_ASSERT(app);

  // we check if the application is installed or not.
  if (app->isInstalled())
    {
    QString qmDir = QString(Slicer_QM_DIR);
    loadTranslations(qmDir);
    }
  else
    {
    QStringList qmDirs = QString(Slicer_QM_OUTPUT_DIRS).split(";");
    foreach(QString qmDir, qmDirs)
      {
      loadTranslations(qmDir);
      }
    }
}

//----------------------------------------------------------------------------
int SlicerAppMain(int argc, char* argv[])
{
  itk::itkFactoryRegistration();

  // [Bender]
  QCoreApplication::setApplicationName("Bender");
  QApplication::setStyle("plastique");
  // [/Bender]

  QCoreApplication::setApplicationVersion(Bender_VERSION_FULL);
  //vtkObject::SetGlobalWarningDisplay(false);
  QApplication::setDesktopSettingsAware(false);

  qSlicerApplication app(argc, argv);
  if (app.returnCode() != -1)
    {
    return app.returnCode();
    }

  // We load the language selected for the application
  loadLanguage();

#ifdef Slicer_USE_QtTesting
  setEnableQtTesting(); // disabled the native menu bar.
#endif

#ifdef Slicer_USE_PYTHONQT
  ctkPythonConsole pythonConsole;
  pythonConsole.setWindowTitle("Slicer Python Interactor");
  if (!qSlicerApplication::testAttribute(qSlicerApplication::AA_DisablePython))
    {
    initializePythonConsole(pythonConsole);
    }
#endif

  bool enableMainWindow = !app.commandOptions()->noMainWindow();
  enableMainWindow = enableMainWindow && !app.commandOptions()->runPythonAndExit();
  bool showSplashScreen = !app.commandOptions()->noSplash() && enableMainWindow;

  QScopedPointer<QSplashScreen> splashScreen;
  if (showSplashScreen)
    {
    QPixmap pixmap(":/SplashScreen.png");
    splashScreen.reset(new QSplashScreen(pixmap));
    splashMessage(splashScreen, "Initializing...");
    splashScreen->show();
    }

  qSlicerModuleManager * moduleManager = qSlicerApplication::application()->moduleManager();
  qSlicerModuleFactoryManager * moduleFactoryManager = moduleManager->factoryManager();
  moduleFactoryManager->addSearchPaths(app.commandOptions()->additonalModulePaths());
  qSlicerApplicationHelper::setupModuleFactoryManager(moduleFactoryManager);

  // Add modules to ignore (for Bender) here:
  //moduleFactoryManager->addModuleToIgnore("SampleData");

  // Register and instantiate modules
  splashMessage(splashScreen, "Registering modules...");
  moduleFactoryManager->registerModules();
  qDebug() << "Number of registered modules:"
           << moduleFactoryManager->registeredModuleNames().count();
  splashMessage(splashScreen, "Instantiating modules...");
  moduleFactoryManager->instantiateModules();
  qDebug() << "Number of instantiated modules:"
           << moduleFactoryManager->instantiatedModuleNames().count();
  // Create main window
  splashMessage(splashScreen, "Initializing user interface...");
  QScopedPointer<qSlicerAppMainWindow> window;
  if (enableMainWindow)
    {
    window.reset(new qSlicerAppMainWindow);
    window->setWindowTitle(window->windowTitle()+ " " + Bender_VERSION);
    }

  // Load all available modules
  foreach(const QString& name, moduleFactoryManager->instantiatedModuleNames())
    {
    Q_ASSERT(!name.isNull());
    qDebug() << "Loading module" << name;
    splashMessage(splashScreen, "Loading module \"" + name + "\"...");
    moduleFactoryManager->loadModule(name);
    }
  qDebug() << "Number of loaded modules:" << moduleManager->modulesNames().count();

  splashMessage(splashScreen, QString());

  if (window)
    {
    window->setHomeModuleCurrent();
    window->show();
    }

  if (splashScreen && window)
    {
    splashScreen->finish(window.data());
    }

  // Process command line argument after the event loop is started
  QTimer::singleShot(0, &app, SLOT(handleCommandLineArguments()));

  // showMRMLEventLoggerWidget();

  // Look at QApplication::exec() documentation, it is recommended to connect
  // clean up code to the aboutToQuit() signal
  return app.exec();
}

} // end of anonymous namespace

#if defined (_WIN32) && !defined (Slicer_BUILD_WIN32_CONSOLE)
int __stdcall WinMain(HINSTANCE hInstance,
                      HINSTANCE hPrevInstance,
                      LPSTR lpCmdLine, int nShowCmd)
{
  Q_UNUSED(hInstance);
  Q_UNUSED(hPrevInstance);
  Q_UNUSED(nShowCmd);

  int argc;
  char **argv;
  vtksys::SystemTools::ConvertWindowsCommandLineToUnixArguments(
    lpCmdLine, &argc, &argv);

  int ret = SlicerAppMain(argc, argv);

  for (int i = 0; i < argc; i++)
    {
    delete [] argv[i];
    }
  delete [] argv;

  return ret;
}
#else
int main(int argc, char *argv[])
{
  return SlicerAppMain(argc, argv);
}
#endif
