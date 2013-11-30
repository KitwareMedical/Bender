from __main__ import vtk, qt, ctk, slicer

class SkinModelMakerLogic:
  def __init__(self):
    # VTK Signals variables
    self.Observations = []
    self.StatusModifiedEvent = slicer.vtkMRMLCommandLineModuleNode().StatusModifiedEvent

    # Parameters dictionnaries
    self.ChangeLabelParameters = {}
    self.GenerateModelParameters = {}
    self.DecimateParameters = {}

    # Status
    self.CLINode = slicer.vtkMRMLCommandLineModuleNode()
    self.CLINode.SetStatus(self.CLINode.Idle)

  def GetCLINode(self):
    return self.CLINode

  def Cancel(self):
    self.CLINode.SetStatus(self.CLINode.Cancelling)

    # Stop CLIs
    # The CLIs will set the CLINode status to Cancel once they stop.
    changeLabelCLINode = self.getCLINode(slicer.modules.changelabel)
    changeLabelCLINode.Cancel()

    modelMakerCLINode = self.getCLINode(slicer.modules.grayscalemodelmaker)
    modelMakerCLINode.Cancel()

    decimatorCLINode = self.getCLINode(slicer.modules.modelquadricclusteringdecimation)
    decimatorCLINode.Cancel()

  def CreateSkinModel(self, parameters, wait_for_completion = False):
    self.CLINode.SetStatus(self.CLINode.Running)

    erodeSkin = len(parameters['SkinLabel']) > 0

    # 0 - Add intermediate volume to the scene
    self.NewVolume = None
    if erodeSkin:
      self.NewVolume = slicer.vtkMRMLScalarVolumeNode()
      self.NewVolume.SetName('SkinModelMakerTemp')
      slicer.mrmlScene.AddNode(self.NewVolume)

    # 1 - Setup change label parameters
    self.ChangeLabelParameters["RunChangeLabel"] = erodeSkin
    self.ChangeLabelParameters["InputVolume"] =  parameters["InputVolume"]
    self.ChangeLabelParameters["OutputVolume"] = self.NewVolume
    self.ChangeLabelParameters["InputLabelNumber"] = str(len(parameters['SkinLabel'].split(',')))
    self.ChangeLabelParameters["InputLabel"] = parameters['SkinLabel']
    self.ChangeLabelParameters["OutputLabel"] = parameters['BackgroundLabel']

    # 2 Setup generate model parameters
    if erodeSkin:
      self.GenerateModelParameters["InputVolume"] = self.NewVolume
    else:
      self.GenerateModelParameters["InputVolume"] =  parameters["InputVolume"]
    self.GenerateModelParameters["OutputGeometry"] = parameters["OutputGeometry"]
    self.GenerateModelParameters["Threshold"] = parameters['BackgroundLabel'] + 0.1
    if parameters['Decimate']:
      self.GenerateModelParameters["Decimate"] = 0.0
      self.GenerateModelParameters["Smooth"] = 0

    # 3 Setup decimator parameters
    self.DecimateParameters["RunDecimator"] = parameters['Decimate']
    self.DecimateParameters["InputModel"] = parameters["OutputGeometry"]
    self.DecimateParameters["DecimatedModel"] = parameters["OutputGeometry"]
    self.DecimateParameters["Spacing"] = parameters['Spacing']
    self.DecimateParameters['UseInputPoints'] = True
    self.DecimateParameters['UseFeatureEdges'] = True
    self.DecimateParameters['UseFeaturePoints'] = True

    # Start CLI chain
    self.WaitForCompletion = wait_for_completion
    self.runChangeLabel()

  def runChangeLabel(self):
    if self.ChangeLabelParameters["RunChangeLabel"]:
      cliNode = self.getCLINode(slicer.modules.changelabel)
      self.addObserver(cliNode, self.StatusModifiedEvent, self.onChangeLabelModified)
      cliNode = slicer.cli.run(slicer.modules.changelabel, cliNode, self.ChangeLabelParameters, self.WaitForCompletion)
    else:
      self.runGenerateModel()

  def onChangeLabelModified(self, cliNode, event):
    if not cliNode.IsBusy():
      self.removeObservers(self.onChangeLabelModified)

      print ('Merge Labels %s' % cliNode.GetStatusString())
      if cliNode.GetStatusString() == 'Completed':
        self.runGenerateModel()
      else:
        slicer.mrmlScene.RemoveNode(self.ChangeLabelParameters["OutputVolume"])
        self.CLINode.SetStatus(cliNode.GetStatus())

  def runGenerateModel(self):
    cliNode = self.getCLINode(slicer.modules.grayscalemodelmaker)
    self.addObserver(cliNode, self.StatusModifiedEvent, self.onGenerateModelModified)
    cliNode = slicer.cli.run(slicer.modules.grayscalemodelmaker, cliNode, self.GenerateModelParameters, self.WaitForCompletion)

  def onGenerateModelModified(self, cliNode, event):
    if not cliNode.IsBusy():
      self.removeObservers(self.onGenerateModelModified)

      if self.ChangeLabelParameters["RunChangeLabel"]:
        slicer.mrmlScene.RemoveNode(self.GenerateModelParameters["InputVolume"])

      print ('Grayscale Model Maker %s' % cliNode.GetStatusString())
      if cliNode.GetStatusString() == 'Completed':
        self.runDecimator()
      else:
        self.CLINode.SetStatus(cliNode.GetStatus())

  def runDecimator(self):
    if self.DecimateParameters["RunDecimator"]:
      cliNode = self.getCLINode(slicer.modules.modelquadricclusteringdecimation)
      self.addObserver(cliNode, self.StatusModifiedEvent, self.onDecimatorModified)
      cliNode = slicer.cli.run(slicer.modules.modelquadricclusteringdecimation, cliNode, self.DecimateParameters, self.WaitForCompletion)
    else:
      self.CLINode.SetStatus(self.CLINode.Completed)

  def onDecimatorModified(self, cliNode, event):
    if not cliNode.IsBusy():
      self.removeObservers(self.onDecimatorModified)

      print ('Model Quadric Clustering %s' % cliNode.GetStatusString())
      if cliNode.GetStatusString() == 'Completed':
        self.GetCLINode().SetStatus(self.CLINode.Completed)
      else:
        self.CLINode.SetStatus(cliNode.GetStatus())

  def getCLINode(self, cliModule):
    """ Return the cli node to use for a given CLI module. Create the node in
    scene if needed.
    """
    cliNode = slicer.mrmlScene.GetFirstNodeByName(cliModule.title)
    if cliNode == None:
      cliNode = slicer.cli.createNode(cliModule)
      cliNode.SetName(cliModule.title)
    return cliNode

  def removeObservers(self, method):
    for o, e, m, g, t in self.Observations:
      if method == m:
        o.RemoveObserver(t)
        self.Observations.remove([o, e, m, g, t])

  def addObserver(self, object, event, method, group = 'none'):
    if self.hasObserver(object, event, method):
      print 'already has observer'
      return
    tag = object.AddObserver(event, method)
    self.Observations.append([object, event, method, group, tag])

  def hasObserver(self, object, event, method):
    for o, e, m, g, t in self.Observations:
      if o == object and e == event and m == method:
        return True
    return False

#
# SkinModelMaker
#

class SkinModelMaker:
  def __init__(self, parent):
    import string
    parent.title = "Skin Model Maker"
    parent.categories = ["Surface Models"]
    parent.dependencies = ["ChangeLabel", "GrayscaleModelMaker", "ModelQuadricClusteringDecimation"]
    parent.contributors = ["Johan Andruejol (Kitware)",]
    parent.helpText = string.Template(
      """This module extracts the outer surface of a volume (labelmap or grayscale).
         It conveniently wraps the modules ChangeLabel, GrayscaleModelMaker and ModelQuadricClusteringDecimator.
         In order to ensure the surface lies within the volume, the skin can be peeled off (chaned into backgroun)
         before extracting the surface.
         In order to reduce the number of points and cells of the extracted surface model,
         an additional decimation is possible using Quadric Clustering.
         See <a href=\"$a/Documentation/$b.$c/Modules/SkinModelMaker\">$a/Documentation/$b.$c/Modules/SkinModelMaker</a> for more information.
      """
      ).substitute({ 'a':'http://public.kitware.com/Wiki/Bender', 'b':1, 'c':1 })
    parent.acknowledgementText = """This work is supported by Air Force Research Laboratory (AFRL)"""
    self.parent = parent

#
# SkinModelMakerWidget
#

class SkinModelMakerWidget:
  def __init__(self, parent = None):
    if not parent:
      self.parent = slicer.qMRMLWidget()
      self.parent.setLayout(qt.QVBoxLayout())
      self.parent.setMRMLScene(slicer.mrmlScene)
    else:
      self.parent = parent
    self.layout = self.parent.layout()
    if not parent:
      self.setup()
      self.parent.show()

  def setup(self):
    import imp, sys, os, slicer
    loader = qt.QUiLoader()
    moduleName = 'SkinModelMaker'
    scriptedModulesPath = eval('slicer.modules.%s.path' % moduleName.lower())
    scriptedModulesPath = os.path.dirname(scriptedModulesPath)
    path = os.path.join(scriptedModulesPath, 'Resources', 'UI', 'SkinModelMaker.ui')

    qfile = qt.QFile(path)
    qfile.open(qt.QFile.ReadOnly)
    widget = loader.load( qfile, self.parent )
    self.layout = self.parent.layout()
    self.widget = widget;
    self.layout.addWidget(widget)

    # Uncomment this vvvv and reloadModule function for faster development
    # self.reloadButton = qt.QPushButton("Reload")
    # self.reloadButton.toolTip = "Reload this module."
    # self.reloadButton.name = "SkinModelMaker Reload"
    # self.layout.addWidget(self.reloadButton)
    # self.reloadButton.connect('clicked()', self.reloadModule)

    # Logic
    self.Logic = SkinModelMakerLogic()

    #
    # Signals / Slots
    #

    self.get('ApplyPushButton').connect('clicked(bool)', self.onApply)

    # --------------------------------------------------------------------------
    # Initialize all the MRML aware GUI elements.
    # Lots of setup methods are called from this line
    self.widget.setMRMLScene(slicer.mrmlScene)

  def onApply(self, run):
    if run:
      self.get('ApplyPushButton').setChecked(True)

      parameters = {}
      parameters["InputVolume"] = self.get('VolumeNodeComboBox').currentNode()
      parameters["OutputGeometry"] = self.get('OutputSkinModelNodeComboBox').currentNode()
      parameters['BackgroundLabel'] = self.get('BackgroundLabelSpinBox').value
      parameters['SkinLabel'] = self.get('SkinLabelsLineEdit').text
      parameters['Decimate'] = self.get('DecimateOutputSkinLabelCheckBox').isChecked()
      parameters['Spacing'] = self.get('SpacingLineEdit').text

      for i in parameters.keys():
        if parameters[i] == None:
          print 'Skin model maker not ran: Parameters incorrect'
          self.get('ApplyPushButton').setChecked(False)
          return

      self.Logic.addObserver(self.Logic.GetCLINode(), slicer.vtkMRMLCommandLineModuleNode().StatusModifiedEvent, self.onLogicModified)
      self.Logic.CreateSkinModel(parameters, False)
    else:
      self.get('ApplyPushButton').setChecked(True) # Keep checked until actually cancelled
      self.get('ApplyPushButton').setEnabled(False)
      self.Logic.Cancel()

  def onLogicModified(self, cliNode, event):
    if not cliNode.IsBusy():
      self.Logic.removeObservers(self.onLogicModified)
      self.get('ApplyPushButton').setEnabled(True)
      self.get('ApplyPushButton').setChecked(False)
      print 'Skin Model Maker %s' % cliNode.GetStatusString()

  # =================== END ==============
  def get(self, objectName):
    return self.findWidget(self.widget, objectName)

  def findWidget(self, widget, objectName):
    if widget.objectName == objectName:
        return widget
    else:
        children = []
        for w in widget.children():
            resulting_widget = self.findWidget(w, objectName)
            if resulting_widget:
                return resulting_widget
        return None

  # def reloadModule(self,moduleName="SkinModelMaker"):
    # """Generic reload method for any scripted module.
    # ModuleWizard will subsitute correct default moduleName.
    # """
    # import imp, sys, os, slicer

    # widgetName = moduleName + "Widget"

    # # reload the source code
    # # - set source file path
    # # - load the module to the global space
    # filePath = eval('slicer.modules.%s.path' % moduleName.lower())
    # p = os.path.dirname(filePath)
    # if not sys.path.__contains__(p):
      # sys.path.insert(0,p)
    # fp = open(filePath, "r")
    # globals()[moduleName] = imp.load_module(
        # moduleName, fp, filePath, ('.py', 'r', imp.PY_SOURCE))
    # fp.close()

    # # rebuild the widget
    # # - find and hide the existing widget
    # # - create a new widget in the existing parent
    # parent = slicer.util.findChildren(name='%s Reload' % moduleName)[0].parent()
    # for child in parent.children():
      # try:
        # child.hide()
      # except AttributeError:
        # pass

    # self.layout.removeWidget(self.widget)
    # self.widget.deleteLater()
    # self.widget = None

    # # Remove spacer items
    # item = parent.layout().itemAt(0)
    # while item:
      # parent.layout().removeItem(item)
      # item = parent.layout().itemAt(0)
    # # create new widget inside existing parent
    # globals()[widgetName.lower()] = eval(
        # 'globals()["%s"].%s(parent)' % (moduleName, widgetName))
    # globals()[widgetName.lower()].setup()
