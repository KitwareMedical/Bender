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

  def CreateSkinModel(self, volume, skin, backgroundLabel, skinLabels, decimate = False, spacing = "", wait_for_completion = False):
    self.CLINode.SetStatus(self.CLINode.Running, True)

    erodeSkin = len(skinLabels) > 0

    # 0 - Add intermediate volume to the scene
    newVolume = slicer.vtkMRMLScalarVolumeNode()
    newVolume.SetName('SkinModelMakerTemp')
    if erodeSkin:
      slicer.mrmlScene.AddNode(newVolume)

    # 1 - Setup change label parameters
    self.ChangeLabelParameters["RunChangeLabel"] = erodeSkin
    self.ChangeLabelParameters["InputVolume"] = volume
    self.ChangeLabelParameters["OutputVolume"] = newVolume
    self.ChangeLabelParameters["InputLabelNumber"] = str(len(skinLabels.split(',')))
    self.ChangeLabelParameters["InputLabel"] = skinLabels
    self.ChangeLabelParameters["OutputLabel"] = backgroundLabel

    # 2 Setup generate model parameters
    if erodeSkin:
      self.GenerateModelParameters["InputVolume"] = newVolume
    else:
      self.GenerateModelParameters["InputVolume"] = volume
    self.GenerateModelParameters["OutputGeometry"] = skin
    self.GenerateModelParameters["Threshold"] = backgroundLabel + 0.1
    if decimate:
      self.GenerateModelParameters["Decimate"] = 0.0
      self.GenerateModelParameters["Smooth"] = 0

    # 3 Setup decimator parameters
    self.DecimateParameters["RunDecimator"] = decimate
    self.DecimateParameters["InputModel"] = skin
    self.DecimateParameters["DecimatedModel"] = skin
    self.DecimateParameters["Spacing"] = spacing
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

      if cliNode.GetStatusString() == 'Completed':
        print ('Merge Labels completed')
        self.runGenerateModel()
      else:
        slicer.mrmlScene.RemoveNode(self.ChangeLabelParameters["OutputVolume"])
        print ('Skin Model Maker Failed: Merge Labels failed')
        self.CLINode.SetStatus(self.CLINode.CompletedWithErrors, True)

  def runGenerateModel(self):
    cliNode = self.getCLINode(slicer.modules.grayscalemodelmaker)
    self.addObserver(cliNode, self.StatusModifiedEvent, self.onGenerateModelModified)
    cliNode = slicer.cli.run(slicer.modules.grayscalemodelmaker, cliNode, self.GenerateModelParameters, self.WaitForCompletion)

  def onGenerateModelModified(self, cliNode, event):
    if not cliNode.IsBusy():
      self.removeObservers(self.onGenerateModelModified)

      if self.ChangeLabelParameters["RunChangeLabel"]:
        slicer.mrmlScene.RemoveNode(self.GenerateModelParameters["InputVolume"])

      if cliNode.GetStatusString() == 'Completed':
        print ('Grayscale Model Maker completed')
        self.runDecimator()
      else:
        print ('Skin Model Maker Failed: Grayscale Model Maker failed')
        self.CLINode.SetStatus(self.CLINode.CompletedWithErrors, True)

  def runDecimator(self):
    if self.DecimateParameters["RunDecimator"]:
      cliNode = self.getCLINode(slicer.modules.modelquadricclusteringdecimation)
      self.addObserver(cliNode, self.StatusModifiedEvent, self.onDecimatorModified)
      cliNode = slicer.cli.run(slicer.modules.modelquadricclusteringdecimation, cliNode, self.DecimateParameters, self.WaitForCompletion)
    else:
      print ('Skin Model Maker Succes !')
      self.CLINode.SetStatus(self.CLINode.Completed, True)

  def onDecimatorModified(self, cliNode, event):
    if not cliNode.IsBusy():
      self.removeObservers(self.onDecimatorModified)

      if cliNode.GetStatusString() == 'Completed':
        print ('Skin Model Maker Succes !')
        self.GetCLINode.SetStatus(self.CLINode.Completed, True)
      else:
        print ('Skin Model Maker Failed: Model Quadric Clustering Decimation failed')
        self.GetCLINode.SetStatus(self.CLINode.CompletedWithErrors, True)

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
    parent.title = "Skin Model Maker"
    parent.categories = ["Surface Models"]
    parent.dependencies = ["ChangeLabel", "GrayscaleModelMaker", "ModelQuadricClusteringDecimation"]
    parent.contributors = ["Johan Andruejol (Kitware)",]
    parent.helpText = """This module creates a skin model contained within the given volume.
                        It really is a wrapper around the ChangeLabel, GrayscaleModelMaker and the ModelQuadricClusteringDecimator.
                        The principle is to set the volume's skin label to background and then run
                        the model maker, with the option to decimate it after using the decimator."""
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

    #
    # Signals / Slots
    #

    self.get('ApplyPushButton').connect('clicked()', self.onApply)

    # --------------------------------------------------------------------------
    # Initialize all the MRML aware GUI elements.
    # Lots of setup methods are called from this line
    self.widget.setMRMLScene(slicer.mrmlScene)

  def onApply(self):
    volume = self.get('VolumeNodeComboBox').currentNode()
    skin = self.get('OutputSkinModelNodeComboBox').currentNode()
    backgroundLabel = self.get('BackgroundLabelSpinBox').value
    skinLabels =  self.get('SkinLabelsLineEdit').text
    decimate = self.get('DecimateOutputSkinLabelCheckBox').isChecked()
    spacing = self.get('SpacingLineEdit').text

    if volume == None or skin == None or backgroundLabel == None or skinLabels == None or spacing == None:
      print 'Parameters incorrect'
      return

    self.Logic = SkinModelMakerLogic()
    self.Logic.CreateSkinModel(volume, skin, backgroundLabel, skinLabels, decimate, spacing, False)

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
