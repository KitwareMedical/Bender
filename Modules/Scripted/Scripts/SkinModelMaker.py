from __main__ import vtk, qt, ctk, slicer

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

    if self.CreateSkinModel(volume, skin, backgroundLabel, skinLabels, spacing, decimate) == True:
      print 'Skin Model Maker Success'
    else:
      print 'Skin Model Maker Error'

  def CreateSkinModel(self, volume, skin, backgroundLabel, skinLabels, spacing, decimate = False):
    erodeSkin = len(skinLabels) > 0

    # 0 - Add intermediate volume to the scene
    newVolume = slicer.vtkMRMLScalarVolumeNode()
    newVolume.SetName('SkinModelMakerTemp')
    if erodeSkin:
      slicer.mrmlScene.AddNode(newVolume)

    # 1 - Get rid of the skin
    if erodeSkin:
      changeLabelParameters = {}
      changeLabelParameters["InputVolume"] = volume
      changeLabelParameters["OutputVolume"] = newVolume

      changeLabelParameters["InputLabelNumber"] = str(len(skinLabels.split(',')))
      changeLabelParameters["InputLabel"] = skinLabels
      changeLabelParameters["OutputLabel"] = backgroundLabel

      changeLabelCLI = None
      changeLabelCLI = slicer.cli.run(slicer.modules.changelabel, changeLabelCLI, changeLabelParameters, wait_for_completion = True)
      if changeLabelCLI.GetStatusString() == 'Completed':
        print 'MergeLabels completed'
      else:
        print 'MergeLabels failed'
        if erodeSkin:
          slicer.mrmlScene.RemoveNode(newVolume)
        return False

    # 2- Generate model
    grayscaleParameters = {}
    if erodeSkin:
      grayscaleParameters["InputVolume"] = newVolume
    else:
      grayscaleParameters["InputVolume"] = volume

    grayscaleParameters["OutputGeometry"] = skin
    grayscaleParameters["Threshold"] = backgroundLabel + 0.1
    if decimate:
      grayscaleParameters["Decimate"] = 0.0
      grayscaleParameters["Smooth"] = 0

    grayscaleCLI = None
    grayscaleCLI = slicer.cli.run(slicer.modules.grayscalemodelmaker, grayscaleCLI, grayscaleParameters, wait_for_completion = True)
    if grayscaleCLI.GetStatusString() == 'Completed':
      print 'MergeLabels completed'
    else:
      print 'MergeLabels failed'
      if erodeSkin:
        slicer.mrmlScene.RemoveNode(newVolume)
      return False

    if not decimate:
      if erodeSkin:
        slicer.mrmlScene.RemoveNode(newVolume)
      return True

    # 3 - Downsample it
    decimatorParameters = {}
    decimatorParameters["InputModel"] = skin
    decimatorParameters["DecimatedModel"] = skin
    decimatorParameters["Spacing"] = spacing
    decimatorParameters['UseInputPoints'] = True
    decimatorParameters['UseFeatureEdges'] = True
    decimatorParameters['UseFeaturePoints'] = True

    decimatorCLI = None
    decimatorCLI = slicer.cli.run(slicer.modules.modelquadricclusteringdecimation, decimatorCLI, decimatorParameters, wait_for_completion = True)
    if decimatorCLI.GetStatusString() == 'Completed':
      print 'Decimator completed'
    else:
      print 'Decimator failed'
      if erodeSkin:
        slicer.mrmlScene.RemoveNode(newVolume)
      return False

    if erodeSkin:
      slicer.mrmlScene.RemoveNode(newVolume)
    return True

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
