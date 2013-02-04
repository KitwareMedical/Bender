from __main__ import vtk, qt, ctk, slicer

#
# Workflow
#

class Workflow:
  def __init__(self, parent):
    import string
    parent.title = "Bender Workflow"
    parent.categories = ["", "Segmentation.Bender"]
    parent.contributors = ["Julien Finet (Kitware), Johan Andruejol (Kitware)"]
    parent.helpText = string.Template("""
Use this module to repose a humanoid labelmap. See <a href=\"$a/Documentation/$b.$c/Modules/Workflow\">$a/Documentation/$b.$c/Modules/Workflow</a> for more information.
    """).substitute({ 'a':'http://public.kitware.com/Wiki/Bender', 'b':0, 'c':1 })
    parent.acknowledgementText = """
    This work is supported by Air Force Research Laboratory (AFRL)
    """
    self.parent = parent

#
# Workflow widget
#

class WorkflowWidget:
  def __init__(self, parent = None):
    if not parent:
      self.setup()
      self.parent.show()
    else:
      self.parent = parent
    self.logic = None
    self.labelmapNode = None
    self.parent.show()

  def setup(self):
    import imp, sys, os, slicer
    loader = qt.QUiLoader()
    moduleName = 'Workflow'
    scriptedModulesPath = eval('slicer.modules.%s.path' % moduleName.lower())
    scriptedModulesPath = os.path.dirname(scriptedModulesPath)
    path = os.path.join(scriptedModulesPath, 'Resources', 'UI', 'Workflow.ui')

    qfile = qt.QFile(path)
    qfile.open(qt.QFile.ReadOnly)
    widget = loader.load( qfile, self.parent )
    self.layout = self.parent.layout()
    self.widget = widget;
    self.layout.addWidget(widget)

    self.reloadButton = qt.QPushButton("Reload")
    self.reloadButton.toolTip = "Reload this module."
    self.reloadButton.name = "Workflow Reload"
    self.layout.addWidget(self.reloadButton)
    self.reloadButton.connect('clicked()', self.reloadModule)

    self.WorkflowWidget = self.get('WorkflowWidget')

    self.volumeNodeMergeLabelsTag = 0
    self.labelmapDisplayNodeMergeLabelsTag = 0

    # --------------------------------------------------------------------------
    # Connections
    # Workflow
    self.get('FirstPageToolButton').connect('clicked()', self.goToFirst)
    self.get('PreviousPageToolButton').connect('clicked()', self.goToPrevious)
    self.get('NextPageToolButton').connect('clicked()', self.goToNext)
    self.get('LastPageToolButton').connect('clicked()', self.goToLast)
    # 0) Bone Segmentations
    # a) Labelmap
    self.get('LabelmapVolumeNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.setupLabelmap)
    self.get('LabelmapColorNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.setVolumeColorNode)
    self.get('LabelmapGoToModulePushButton').connect('clicked()', self.openLabelmapModule)
    # b) Merge Labels
    self.get('MergeLabelsInputNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.setupMergeLabels)
    self.get('MergeLabelsApplyPushButton').connect('clicked()', self.runMergeLabels)
    self.get('MergeLabelsGoToModulePushButton').connect('clicked()', self.openMergeLabelsModule)

    # 1) Model Maker
    # a) Model Maker
    self.get('ModelMakerInputNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.setupModelMaker)
    self.get('BoneLabelComboBox').connect('currentColorChanged(int)', self.setupModelMakerLabels)
    self.get('SkinLabelComboBox').connect('currentColorChanged(int)', self.setupModelMakerLabels)
    self.get('ModelMakerApplyPushButton').connect('clicked()', self.runModelMaker)
    self.get('ModelMakerGoToModulePushButton').connect('clicked()', self.openModelMakerModule)
    # b) Volume Render
    self.get('BoneLabelComboBox').connect('currentColorChanged(int)', self.setupVolumeRenderLabels)
    self.get('SkinLabelComboBox').connect('currentColorChanged(int)', self.setupVolumeRenderLabels)
    self.get('VolumeRenderInputNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.setupVolumeRender)
    self.get('VolumeRenderLabelsLineEdit').connect('editingFinished()', self.updateVolumeRenderLabels)
    self.get('VolumeRenderCheckBox').connect('toggled(bool)',self.runVolumeRender)
    self.get('VolumeRenderGoToModulePushButton').connect('clicked()', self.openVolumeRenderModule)
    # 2) Armatures
    self.get('ArmaturesGoToPushButton').connect('clicked()', self.openArmaturesModules)
    # 3) Armature Bones
    self.get('SegmentBonesApplyPushButton').connect('clicked()',self.runSegmentBones)
    self.get('SegmentBonesGoToPushButton').connect('clicked()', self.openSegmentBonesModules)
    # 4) Resample
    self.get('ModelMakerInputNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.get('ResampleLabelMapNodeComboBox').setCurrentNode)
    self.get('ResampleApplyPushButton').connect('clicked()', self.runResample)

    # --------------------------------------------------------------------------
    # Initialize
    self.widget.setMRMLScene(slicer.mrmlScene)

  def goToFirst(self):
    self.WorkflowWidget.setCurrentIndex(0)
  def goToPrevious(self):
    self.WorkflowWidget.setCurrentIndex(self.WorkflowWidget.currentIndex - 1)
  def goToNext(self):
    self.WorkflowWidget.setCurrentIndex(self.WorkflowWidget.currentIndex + 1)
  def goToLast(self):
    self.WorkflowWidget.setCurrentIndex(self.WorkflowWidget.count() - 1)

  # 0) Bone Segmentation
  #     a) Labelmap
  def updateLabelmap(self, node, event):
    volumeNode = self.get('LabelmapVolumeNodeComboBox').currentNode()
    if node.IsA('vtkMRMLScalarVolumeNode') and node != volumeNode:
      return
    self.setupLabelmap(volumeNode)
    self.get('MergeLabelsInputNodeComboBox').setCurrentNode(volumeNode)
    self.get('SegmentBonesInputVolumeNodeComboBox').setCurrentNode(volumeNode)
    #self.setupMergeLabels(volumeNode)

  def setupLabelmap(self, volumeNode):
    if volumeNode == None:
      return
    labelmapDisplayNode = volumeNode.GetDisplayNode()
    colorNode = labelmapDisplayNode.GetColorNode()
    # set the color node first, just in case the checkbox needs it
    self.get('LabelmapColorNodeComboBox').setCurrentNode(colorNode)
    volumeNode.AddObserver('ModifiedEvent', self.updateLabelmap)
    labelmapDisplayNode.AddObserver('ModifiedEvent', self.updateLabelmap)

  def setVolumeColorNode(self, colorNode):
    volumeNode = self.get('LabelmapVolumeNodeComboBox').currentNode()
    if volumeNode == None:
      return

    volumesLogic = slicer.modules.volumes.logic()
    volumesLogic.SetVolumeAsLabelMap(volumeNode, colorNode != None)

    labelmapDisplayNode = volumeNode.GetDisplayNode()
    colorNodeID = ""
    if colorNode != None:
      colorNodeID = colorNode.GetID()
    labelmapDisplayNode.SetAndObserveColorNodeID(colorNodeID)

  def openLabelmapModule(self):
    self.openModule('Volumes')

  #    b) Merge Labels
  def updateMergeLabels(self, node, event):
    volumeNode = self.get('MergeLabelsInputNodeComboBox').currentNode()
    if node.IsA('vtkMRMLScalarVolumeNode') and node != volumeNode:
      return
    elif node.IsA('vtkMRMLVolumeDisplayNode'):
      if node != volumeNode.GetDisplayNode():
        return
    self.setupMergeLabels(volumeNode)

  def setupMergeLabels(self, volumeNode):
    if volumeNode == None:
      return
    labelmapDisplayNode = volumeNode.GetDisplayNode()
    if labelmapDisplayNode == None:
      return
    colorNode = labelmapDisplayNode.GetColorNode()
    if colorNode == None:
      self.get('BoneLabelComboBox').setMRMLColorNode(None)
      self.get('SkinLabelComboBox').setMRMLColorNode(None)
      self.get('BoneLabelsLineEdit').setText('')
      self.get('BoneLabelComboBox').setCurrentColor(None)
      self.get('SkinLabelsLineEdit').setText('')
      self.get('SkinLabelComboBox').setCurrentColor(None)

      volumeNode.RemoveObserver(self.volumeNodeMergeLabelsTag)
      labelmapDisplayNode.RemoveObserver(self.labelmapDisplayNodeMergeLabelsTag)
    else:
      self.get('BoneLabelComboBox').setMRMLColorNode(colorNode)
      self.get('SkinLabelComboBox').setMRMLColorNode(colorNode)
      boneLabels = self.searchLabels(colorNode, 'bone')
      boneLabels.update(self.searchLabels(colorNode, 'vertebr'))
      self.get('BoneLabelsLineEdit').setText(', '.join(str( val ) for val in boneLabels.keys()))
      boneLabel = self.bestLabel(boneLabels, 'bone')
      self.get('BoneLabelComboBox').setCurrentColor(boneLabel)
      skinLabels = self.searchLabels(colorNode, 'skin')
      self.get('SkinLabelsLineEdit').setText(', '.join(str(val) for val in skinLabels.keys()))
      skinLabel = self.bestLabel(skinLabels, 'skin')
      self.get('SkinLabelComboBox').setCurrentColor(skinLabel)

      self.volumeNodeMergeLabelsTag = volumeNode.AddObserver('ModifiedEvent', self.updateMergeLabels)
      self.labelmapDisplayNodeMergeLabelsTag = labelmapDisplayNode.AddObserver('ModifiedEvent', self.updateMergeLabels)

  def searchLabels(self, colorNode, label):
    """ Search the color node for all the labels that contain the word 'label'
    """
    labels = {}
    for index in range(colorNode.GetNumberOfColors()):
      if label in colorNode.GetColorName(index).lower():
        labels[index] = colorNode.GetColorName(index)
    return labels

  def bestLabel(self, labels, label):
    """ Return the label from a [index, colorName] map that fits the best the
         label name
    """
    if (len(labels) == 0):
      return -1

    for key in labels.keys():
      if labels[key].lower().startswith(label):
        return key
    return labels.keys()[0]

  def runMergeLabels(self):
    boneLabels = self.get('BoneLabelsLineEdit').text
    skinLabels = self.get('SkinLabelsLineEdit').text
    parameters = {}
    parameters["InputVolume"] = self.get('MergeLabelsInputNodeComboBox').currentNode().GetID()
    parameters["OutputVolume"] = self.get('MergeLabelsOutputNodeComboBox').currentNode().GetID()
    # That's my dream:
    #parameters["InputLabelNumber"] = len(boneLabels.split(','))
    #parameters["InputLabelNumber"] = len(skinLabels.split(','))
    #parameters["InputLabel"] = boneLabels
    #parameters["InputLabel"] = skinLabels
    #parameters["OutputLabel"] = self.get('BoneLabelComboBox').currentColor
    #parameters["OutputLabel"] = self.get('SkinLabelComboBox').currentColor
    # But that's how it is done for now
    parameters["InputLabelNumber"] = str(len(boneLabels.split(','))) + ', ' + str(len(skinLabels.split(',')))
    parameters["InputLabel"] = boneLabels + ', ' + skinLabels
    parameters["OutputLabel"] = str(self.get('BoneLabelComboBox').currentColor) + ', ' + str(self.get('SkinLabelComboBox').currentColor)
    cliNode = None
    cliNode = slicer.cli.run(slicer.modules.changelabel, cliNode, parameters, wait_for_completion = True)
    status = cliNode.GetStatusString()
    if status == 'Completed':
      print 'MergeLabels completed'

      # apply label map
      newNode = self.get('MergeLabelsOutputNodeComboBox').currentNode()
      colorNode = self.get('LabelmapColorNodeComboBox').currentNode()
      if newNode != None and colorNode != None:
        volumesLogic = slicer.modules.volumes.logic()
        volumesLogic.SetVolumeAsLabelMap(newNode, True)

        newNode.GetDisplayNode().SetAndObserveColorNodeID(colorNode.GetID())

    else:
      print 'MergeLabels failed'

  def openMergeLabelsModule(self):
    self.openModule('ChangeLabel')

  # 1) Model Maker
  #     a) Model Maker
  def updateModelMaker(self, volumeNode, event):
    if volumeNode != self.get('ModelMakerInputNodeComboBox').currentNode():
      return
    self.setupModelMaker(volumeNode)

  def setupModelMaker(self, volumeNode):
    if volumeNode == None:
      return
    volumeNode.AddObserver('ModifiedEvent', self.updateModelMaker)

  def setupModelMakerLabels(self):
    """ Update the labels of the model maker
    """
    labels = []
    labels.append(self.get('BoneLabelComboBox').currentColor)
    #labels.append(self.get('SkinLabelComboBox').currentColor)
    self.get('ModelMakerLabelsLineEdit').setText(', '.join(str(val) for val in labels))

  def runModelMaker(self):
    parameters = {}
    parameters["InputVolume"] = self.get('ModelMakerInputNodeComboBox').currentNode().GetID()
    parameters["ModelSceneFile"] = self.get('ModelMakerOutputNodeComboBox').currentNode().GetID()
    parameters["Labels"] = self.get('ModelMakerLabelsLineEdit').text
    parameters['GenerateAll'] = False
    parameters["JointSmoothing"] = False
    parameters["SplitNormals"] = True
    parameters["PointNormals"] = True
    parameters["SkipUnNamed"] = True
    parameters["Decimate"] = 0.25
    parameters["Smooth"] = 10
    cliNode = None
    cliNode = slicer.cli.run(slicer.modules.modelmaker, cliNode, parameters, wait_for_completion = True)
    status = cliNode.GetStatusString()
    if status == 'Completed':
      print 'ModelMaker completed'
    else:
      print 'ModelMaker failed'

  def openModelMakerModule(self):
    self.openModule('ModelMaker')

  #     b) Volume Render
  def updateVolumeRender(self, volumeNode, event):
    if volumeNode != self.get('VolumeRenderInputNodeComboBox').currentNode():
      return
    self.setupVolumeRender(volumeNode)

  def setupVolumeRender(self, volumeNode):
    if volumeNode == None:
      return
    displayNode = volumeNode.GetNthDisplayNodeByClass(0, 'vtkMRMLVolumeRenderingDisplayNode')
    visible = False
    if displayNode != None:
      visible = displayNode.GetVisibility()
    self.get('VolumeRenderCheckBox').setChecked(visible)
    self.setupVolumeRenderLabels()
    volumeNode.AddObserver('ModifiedEvent', self.updateVolumeRender)

  def setupVolumeRenderLabels(self):
    """ Update the labels of the volume rendering
    """
    labels = []
    labels.append(self.get('BoneLabelComboBox').currentColor)
    labels.append(self.get('SkinLabelComboBox').currentColor)
    self.get('VolumeRenderLabelsLineEdit').setText(', '.join(str(val) for val in labels))

  def getVolumeRenderLabels(self):
    return self.get('VolumeRenderLabelsLineEdit').text.split(', ')

  def updateVolumeRenderLabels(self):
    """ Update the LUT used to volume render the labelmap
    """
    if not self.get('VolumeRenderCheckBox').isChecked():
      return
    volumeNode = self.get('VolumeRenderInputNodeComboBox').currentNode()
    displayNode = volumeNode.GetNthDisplayNodeByClass(0, 'vtkMRMLVolumeRenderingDisplayNode')
    volumePropertyNode = displayNode.GetVolumePropertyNode()
    opacities = volumePropertyNode.GetScalarOpacity()
    labels = self.getVolumeRenderLabels()
    for i in range(opacities.GetSize()):
      node = [0, 0, 0, 0]
      opacities.GetNodeValue(i, node)
      if str(i) in labels:
        node[1] = 0.5
        node[3] = 1
      else:
        node[1] = 0
        node[3] = 1
      opacities.SetNodeValue(i, node)
    opacities.Modified()

  def runVolumeRender(self, show):
    volumeNode = self.get('VolumeRenderInputNodeComboBox').currentNode()
    displayNode = volumeNode.GetNthDisplayNodeByClass(0, 'vtkMRMLVolumeRenderingDisplayNode')
    if not show:
      if displayNode == None:
        return
      displayNode.SetVisibility(0)
    else:
      volumeRenderingLogic = slicer.modules.volumerendering.logic()
      if displayNode == None:
        displayNode = volumeRenderingLogic.CreateVolumeRenderingDisplayNode()
        slicer.mrmlScene.AddNode(displayNode)
        displayNode.UnRegister(volumeRenderingLogic)
        volumeRenderingLogic.UpdateDisplayNodeFromVolumeNode(displayNode, volumeNode)
        volumeNode.AddAndObserveDisplayNodeID(displayNode.GetID())
      else:
        volumeRenderingLogic.UpdateDisplayNodeFromVolumeNode(displayNode, volumeNode)
      self.updateVolumeRenderLabels()
      volumePropertyNode = displayNode.GetVolumePropertyNode()
      volumeProperty = volumePropertyNode.GetVolumeProperty()
      volumeProperty.SetShade(0)
      displayNode.SetVisibility(1)

  def openVolumeRenderModule(self):
    self.openModule('VolumeRendering')

  # 2) Armatures first part
  def openArmaturesModules(self):
    self.openModule('Armatures')

  # 3) Armatures first part
  def runSegmentBones(self):
    parameters = {}
    parameters["RestLabelmap"] = self.get('SegmentBonesInputVolumeNodeComboBox').currentNode().GetID()
    parameters["ArmaturePoly"] = self.get('SegmentBonesAmartureNodeComboBox').currentNode().GetID()
    parameters["BodyPartition"] = self.get('SegmentBonesOutputVolumeNodeComboBox').currentNode().GetID()
    parameters["Padding"] = 1
    parameters["Debug"] = False
    parameters["ArmatureInRAS"] = False
    cliNode = None
    cliNode = slicer.cli.run(slicer.modules.armaturebones, cliNode, parameters, wait_for_completion = True)
    status = cliNode.GetStatusString()
    if status == 'Completed':
      print 'Armature Bones completed'
    else:
      print 'Armature Bones failed'

  def openSegmentBonesModules(self):
    self.openModule('ArmatureBones')

  # 4) Resample NOTE: SHOULD BE LAST STEP
  def runResample(self):
    print('Resample')

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

  def openModule(self, moduleName):
    slicer.util.selectModule(moduleName)

  def reloadModule(self,moduleName="Workflow"):
    """Generic reload method for any scripted module.
    ModuleWizard will subsitute correct default moduleName.
    """
    import imp, sys, os, slicer

    widgetName = moduleName + "Widget"

    # reload the source code
    # - set source file path
    # - load the module to the global space
    filePath = eval('slicer.modules.%s.path' % moduleName.lower())
    p = os.path.dirname(filePath)
    if not sys.path.__contains__(p):
      sys.path.insert(0,p)
    fp = open(filePath, "r")
    globals()[moduleName] = imp.load_module(
        moduleName, fp, filePath, ('.py', 'r', imp.PY_SOURCE))
    fp.close()

    # rebuild the widget
    # - find and hide the existing widget
    # - create a new widget in the existing parent
    parent = slicer.util.findChildren(name='%s Reload' % moduleName)[0].parent()
    for child in parent.children():
      try:
        child.hide()
      except AttributeError:
        pass

    self.layout.removeWidget(self.widget)
    self.widget.deleteLater()
    self.widget = None

    # Remove spacer items
    item = parent.layout().itemAt(0)
    while item:
      parent.layout().removeItem(item)
      item = parent.layout().itemAt(0)
    # create new widget inside existing parent
    globals()[widgetName.lower()] = eval(
        'globals()["%s"].%s(parent)' % (moduleName, widgetName))
    globals()[widgetName.lower()].setup()

  # =================== END ==============

class WorkflowLogic:
  """Implement the logic to calculate label statistics.
  Nodes are passed in as arguments.
  Results are stored as 'statistics' instance variable.
  """
  def __init__(self):
    return

class Slicelet(object):
  """A slicer slicelet is a module widget that comes up in stand alone mode
  implemented as a python class.
  This class provides common wrapper functionality used by all slicer modlets.
  """
  # TODO: put this in a SliceletLib
  # TODO: parse command line arge


  def __init__(self, widgetClass=None):
    self.parent = qt.QFrame()
    self.parent.setLayout( qt.QVBoxLayout() )

    # TODO: should have way to pop up python interactor
    self.buttons = qt.QFrame()
    self.buttons.setLayout( qt.QHBoxLayout() )
    self.parent.layout().addWidget(self.buttons)
    self.addDataButton = qt.QPushButton("Add Data")
    self.buttons.layout().addWidget(self.addDataButton)
    self.addDataButton.connect("clicked()",slicer.app.ioManager().openAddDataDialog)
    self.loadSceneButton = qt.QPushButton("Load Scene")
    self.buttons.layout().addWidget(self.loadSceneButton)
    self.loadSceneButton.connect("clicked()",slicer.app.ioManager().openLoadSceneDialog)

    if widgetClass:
      self.widget = widgetClass(self.parent)
      self.widget.setup()
    self.parent.show()

class WorkflowSlicelet(Slicelet):
  """ Creates the interface when module is run as a stand alone gui app.
  """

  def __init__(self):
    super(WorkflowSlicelet,self).__init__(WorkflowWidget)


if __name__ == "__main__":
  # TODO: need a way to access and parse command line arguments
  # TODO: ideally command line args should handle --xml

  import sys
  print( sys.argv )

  slicelet = WorkflowSlicelet()
