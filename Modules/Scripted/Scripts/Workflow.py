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
    parent.index = 0
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

    self.Observations = []

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
    self.TitleLabel = self.get('TitleLabel')

    # Labelmap variables

    # Transform variables
    self.TransformNode = None

    # --------------------------------------------------------------------------
    # Connections
    # Workflow
    self.get('NextPageToolButton').connect('clicked()', self.goToNext)
    self.get('PreviousPageToolButton').connect('clicked()', self.goToPrevious)
    # 0) Welcome
    self.get('WelcomeSimpleWorkflowCheckBox').connect('stateChanged(int)', self.setupSimpleWorkflow)
    # 1) Adjust Labelmap
    # a) Labelmap
    self.get('LabelmapVolumeNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.setupLabelmap)
    self.get('LabelMapApplyColorNodePushButton').connect('clicked()', self.applyColorNode)
    self.get('LabelmapGoToModulePushButton').connect('clicked()', self.openLabelmapModule)
    self.get('TransformApplyPushButton').connect('clicked()', self.runTransform)
    # c) Merge Labels
    self.get('MergeLabelsInputNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.setupMergeLabels)
    self.get('MergeLabelsApplyPushButton').connect('clicked()', self.runMergeLabels)
    self.get('MergeLabelsGoToModulePushButton').connect('clicked()', self.openMergeLabelsModule)
    # 2) Model Maker
    # a) Bone Model Maker
    self.get('BoneLabelComboBox').connect('currentColorChanged(int)', self.setupBoneModelMakerLabels)
    self.get('BoneModelMakerApplyPushButton').connect('clicked()', self.runBoneModelMaker)
    self.get('BoneModelMakerGoToModelsModulePushButton').connect('clicked()', self.openModelsModule)
    self.get('BoneModelMakerGoToModulePushButton').connect('clicked()', self.openBoneModelMakerModule)
    # b) Skin Model Maker
    self.get('SkinModelMakerInputNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.setupSkinModelMakerLabels)
    self.get('SkinModelMakerToggleVisiblePushButtton').connect('clicked()', self.updateSkinNodeVisibility)
    self.get('SkinModelMakerApplyPushButton').connect('clicked()', self.runSkinModelMaker)
    self.get('SkinModelMakerGoToModelsModulePushButton').connect('clicked()', self.openModelsModule)
    self.get('SkinModelMakerGoToModulePushButton').connect('clicked()', self.openSkinModelMakerModule)
    # c) Volume Render
    self.get('BoneLabelComboBox').connect('currentColorChanged(int)', self.setupVolumeRenderLabels)
    self.get('SkinLabelComboBox').connect('currentColorChanged(int)', self.setupVolumeRenderLabels)
    self.get('VolumeRenderInputNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.setupVolumeRender)
    self.get('VolumeRenderLabelsLineEdit').connect('editingFinished()', self.updateVolumeRenderLabels)
    self.get('VolumeRenderCheckBox').connect('toggled(bool)',self.runVolumeRender)
    self.get('VolumeRenderGoToModulePushButton').connect('clicked()', self.openVolumeRenderModule)
    # 3) Armatures
    self.get('ArmaturesToggleVisiblePushButtton').connect('clicked()', self.updateSkinNodeVisibility)
    self.get('ArmaturesLoadArmaturePushButton').connect('clicked()', self.loadArmatureFile)
    self.get('ArmaturesGoToPushButton').connect('clicked()', self.openArmaturesModule)
    # 4) Skinning
    # a) Volume Skinning
    self.get('VolumeSkinningApplyPushButton').connect('clicked()',self.runVolumeSkinning)
    self.get('VolumeSkinningGoToPushButton').connect('clicked()', self.openVolumeSkinningModule)
    self.get('VolumeSkinningInputVolumeNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.createOutputSkinnedVolume)
    # b) Armatures Weight
    self.get('ComputeArmatureWeightApplyPushButton').connect('clicked()',self.runComputeArmatureWeight)
    self.get('ComputeArmatureWeightGoToPushButton').connect('clicked()', self.openComputeArmatureWeightModule)
    # 5) (Pose) Armature And Pose Body
    # a) Eval Weight
    self.get('EvalSurfaceWeightApplyPushButton').connect('clicked()', self.runEvalSurfaceWeight)
    self.get('EvalSurfaceWeightGoToPushButton').connect('clicked()', self.openEvalSurfaceWeight)
    self.get('EvalSurfaceWeightWeightDirectoryButton').connect('directoryChanged(QString)', self.setWeightDirectory)
    # b) (Pose) Armatures
    self.get('PoseArmaturesGoToPushButton').connect('clicked()', self.openPosedArmatureModule)
    # c) Pose Body
    self.get('PoseSurfaceOutputNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.poseSurfaceParameterChanged)
    self.get('PoseSurfaceWeightInputDirectoryButton').connect('directoryChanged(QString)', self.poseSurfaceParameterChanged)
    self.get('PoseSurfaceInputComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.poseSurfaceParameterChanged)
    self.get('PoseSurfaceArmatureInputNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.poseSurfaceParameterChanged)
    self.get('PoseSurfaceApplyPushButton').connect('clicked()', self.runPoseSurface)

    self.get('PoseSurfaceGoToPushButton').connect('clicked()', self.openPoseSurfaceModule)
    self.get('ComputeArmatureWeightOutputDirectoryButton').connect('directoryChanged(QString)', self.setWeightDirectory)

    self.get('PoseSurfaceInputComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.createOutputSurface)
    # 6) Resample
    self.get('PoseLabelmapApplyPushButton').connect('clicked()', self.runPoseLabelmap)
    self.get('PoseLabelmapGoToPushButton').connect('clicked()', self.openPoseLabelmap)

    self.get('PoseLabelmapInputNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.createOutputPoseLabelmap)

    self.openPage = { 0 : self.openAdjustPage,
                      1 : self.openExtractPage,
                      2 : self.openCreateArmaturePage,
                      3 : self.openSkinningPage,
                      4 : self.openPoseArmaturePage,
                      5 : self.openPoseLabelmapPage
                      }
    # --------------------------------------------------------------------------
    # Initialize all the MRML aware GUI elements.
    # Lots of setup methods are called from this line
    self.setupComboboxes()
    self.widget.setMRMLScene(slicer.mrmlScene)

    # Init title
    self.updateHeader()

    # Init transform node
    self.TransformNode = slicer.mrmlScene.GetFirstNodeByName('WorflowTransformNode')
    if self.TransformNode == None:
      self.TransformNode = slicer.vtkMRMLLinearTransformNode()
      self.TransformNode.SetName('WorflowTransformNode')
      self.TransformNode.HideFromEditorsOn()

      transform = vtk.vtkMatrix4x4()
      transform.DeepCopy((-1.0, 0.0, 0.0, 0.0,
                           0.0, -1.0, 0.0, 0.0,
                           0.0, 0.0, 1.0, 0.0,
                           0.0, 0.0, 0.0, 1.0))
      self.TransformNode.ApplyTransformMatrix(transform)
      slicer.mrmlScene.AddNode(self.TransformNode)

    # Workflow page
    self.setupSimpleWorkflow(self.get('WelcomeSimpleWorkflowCheckBox').isChecked())
    self.get('AdvancedPropertiesWidget').setVisible(self.get('ExpandAdvancedPropertiesButton').isChecked())

  # Worflow
  def updateHeader(self):
    # title
    title = self.WorkflowWidget.currentWidget().accessibleName
    self.TitleLabel.setText('<h2>%i) %s</h2>' % (self.WorkflowWidget.currentIndex + 1, title))

    # help
    self.get('HelpCollapsibleButton').setText('Help')
    self.get('HelpLabel').setText(self.WorkflowWidget.currentWidget().accessibleDescription)

    # previous
    if self.WorkflowWidget.currentIndex > 0:
      self.get('PreviousPageToolButton').setVisible(True)
      previousIndex = self.WorkflowWidget.currentIndex - 1
      previousWidget = self.WorkflowWidget.widget(previousIndex)

      previous = previousWidget.accessibleName
      self.get('PreviousPageToolButton').setText('< %i) %s' %(previousIndex + 1, previous))
    else:
      self.get('PreviousPageToolButton').setVisible(False)

    # next
    if self.WorkflowWidget.currentIndex < self.WorkflowWidget.count - 1:
      self.get('NextPageToolButton').setVisible(True)
      nextIndex = self.WorkflowWidget.currentIndex + 1
      nextWidget = self.WorkflowWidget.widget(nextIndex)

      next = nextWidget.accessibleName
      self.get('NextPageToolButton').setText('%i) %s >' %(nextIndex + 1, next))
    else:
      self.get('NextPageToolButton').setVisible(False)
    self.openPage[self.WorkflowWidget.currentIndex]()

  def goToPrevious(self):
    self.WorkflowWidget.setCurrentIndex(self.WorkflowWidget.currentIndex - 1)
    self.updateHeader()

  def goToNext(self):
    self.WorkflowWidget.setCurrentIndex(self.WorkflowWidget.currentIndex + 1)
    self.updateHeader()

  # 0) Welcome
  def openWelcomePage(self):
    print('welcome')

  # Helper function for setting the visibility of a list of widgets
  def setWidgetsVisibility(self, widgets, visible):
    for widget in widgets:
      self.get(widget).setVisible(visible)

  def setupSimpleWorkflow(self, advanced):
    # 1) LabelMap
    # a)
    self.get('LabelmapGoToModulePushButton').setVisible(advanced)

    # b) Merge Labels
    # Hide all but the output
    advancedMergeWidgets = ['MergeLabelsInputLabel', 'MergeLabelsInputNodeComboBox',
                            'BoneLabelsLabel', 'BoneLabelsLineEdit',
                            'BoneLabelLabel', 'BoneLabelComboBox',
                            'SkinLabelsLabel', 'SkinLabelsLineEdit',
                            'SkinLabelLabel', 'SkinLabelComboBox',
                            'MergeLabelsGoToModulePushButton']
    self.setWidgetsVisibility(advancedMergeWidgets, advanced)

    # 2) Model Maker Page
    # a) bone model maker
    # Hide all but the output and the toggle button
    advancedBoneModelMakerWidgets = ['BoneModelMakerInputLabel', 'BoneModelMakerInputNodeComboBox',
                                     'BoneModelMakerLabelsLabel', 'BoneModelMakerLabelsLineEdit',
                                     'BoneModelMakerGoToModelsModulePushButton',
                                     'BoneModelMakerGoToModulePushButton']
    self.setWidgetsVisibility(advancedBoneModelMakerWidgets, advanced)

    # b) Skin model maker
    # Hide all but the output and the toggle button
    advancedSkinModelMakerWidgets = ['SkinModelMakerNodeInputLabel', 'SkinModelMakerInputNodeComboBox',
                                     'SkinModelMakerThresholdLabel', 'SkinModelMakerThresholdSpinBox',
                                     'SkinModelMakerGoToModelsModulePushButton',
                                     'SkinModelMakerGoToModulePushButton']
    self.setWidgetsVisibility(advancedSkinModelMakerWidgets, advanced)

    # 3) Armature
    # Nothing

    # 4) Weights
    # a) Volume skinning
    # Just hide Go To
    self.get('VolumeSkinningGoToPushButton').setVisible(advanced)
    # b) Weights
    # Leave only weight folder
    advancedComputeWeightWidgets = ['ComputeArmatureWeightInputVolumeLabel', 'ComputeArmatureWeightInputVolumeNodeComboBox',
                                   'ComputeArmatureWeightArmatureLabel', 'ComputeArmatureWeightAmartureNodeComboBox',
                                   'ComputeArmatureWeightSkinnedVolumeLabel', 'ComputeArmatureWeightSkinnedVolumeVolumeNodeComboBox',
                                   'ComputeArmatureWeightGoToPushButton']
    self.setWidgetsVisibility(advancedComputeWeightWidgets, advanced)

    # 5) Pose Page
    # a) Armatures
    # Nothing
    # b) Eval Weight
    advancedEvalSurfaceWeightWidgets = ['EvalSurfaceWeightInputSurfaceLabel', 'EvalSurfaceWeightInputNodeComboBox',
                                 'EvalSurfaceWeightWeightDirectoryLabel', 'EvalSurfaceWeightWeightDirectoryButton',
                                 'EvalSurfaceWeightGoToPushButton']
    self.setWidgetsVisibility(advancedEvalSurfaceWeightWidgets, advanced)

    # c) Pose Body
    # hide almost completely
    advancedPoseSurfaceWidgets = ['PoseSurfaceArmaturesLabel', 'PoseSurfaceArmatureInputNodeComboBox',
                               'PoseSurfaceInputLabel', 'PoseSurfaceInputComboBox',
                               'PoseSurfaceWeightInputFolderLabel', 'PoseSurfaceWeightInputDirectoryButton',
                               'PoseSurfaceGoToPushButton']
    self.setWidgetsVisibility(advancedPoseSurfaceWidgets, advanced)

    # 6) Resample
    # Hide all but output
    advancedPoseLabemapWidgets = ['PoseLabelmapInputLabel', 'PoseLabelmapInputNodeComboBox',
                                  'PoseLabelmapArmatureLabel', 'PoseLabelmapArmatureNodeComboBox',
                                  'PoseLabelmapWeightDirectoryLabel', 'PoseLabelmapWeightDirectoryButton',
                                  'PoseLabelmapGoToPushButton']
    self.setWidgetsVisibility(advancedPoseLabemapWidgets, advanced)

  def setupComboboxes(self):
    # Add here the combo box that should only see labelmaps
    labeldMapComboBoxes = ['MergeLabelsInputNodeComboBox', 'MergeLabelsOutputNodeComboBox',
                           'VolumeSkinningInputVolumeNodeComboBox', 'VolumeSkinningOutputVolumeNodeComboBox',
                           'ComputeArmatureWeightInputVolumeNodeComboBox', 'ComputeArmatureWeightSkinnedVolumeVolumeNodeComboBox',
                           'PoseLabelmapInputNodeComboBox', 'PoseLabelmapOutputNodeComboBox']

    for combobox in labeldMapComboBoxes:
      self.get(combobox).addAttribute('vtkMRMLScalarVolumeNode','LabelMap','1')

  # 1) Adjust Labelmap
  def openAdjustPage(self):
    # Switch to 3D View only
    slicer.app.layoutManager().setLayout(slicer.vtkMRMLLayoutNode.SlicerLayoutFourUpView)

  #     a) Labelmap
  def updateLabelmap(self, node, event):
    volumeNode = self.get('LabelmapVolumeNodeComboBox').currentNode()
    if node != volumeNode and node != volumeNode.GetDisplayNode():
      return
    self.setupLabelmap(volumeNode)
    self.setupMergeLabels(volumeNode)

  def setupLabelmap(self, volumeNode):
    if volumeNode == None:
      return

    # Init color node combo box <=> make 'Generic Colors' labelmap visible
    model = self.get('LabelmapColorNodeComboBox').sortFilterProxyModel()
    visibleNodeIDs = []
    visibleNodeIDs.append(slicer.mrmlScene.GetFirstNodeByName('GenericColors').GetID())
    visibleNodeIDs.append(slicer.mrmlScene.GetFirstNodeByName('GenericAnatomyColors').GetID())
    model.visibleNodeIDs = visibleNodeIDs

    # Labelmapcolornode should get its scene before the volume node selector
    # gets it. That way, setCurrentNode can work at first
    self.get('LabelmapColorNodeComboBox').setCurrentNode(volumeNode.GetDisplayNode().GetColorNode())
    self.addObserver(volumeNode, 'ModifiedEvent', self.updateLabelmap)
    self.addObserver(volumeNode.GetDisplayNode(), 'ModifiedEvent', self.updateLabelmap)

  def applyColorNode(self):
    volumeNode = self.get('LabelmapVolumeNodeComboBox').currentNode()
    if volumeNode == None:
      return

    self.get('LabelMapApplyColorNodePushButton').setChecked(True)
      
    colorNode = self.get('LabelmapColorNodeComboBox').currentNode()
    volumesLogic = slicer.modules.volumes.logic()

    wasModifying = volumeNode.StartModify()
    volumesLogic.SetVolumeAsLabelMap(volumeNode, colorNode != None) # Greyscale is None

    labelmapDisplayNode = volumeNode.GetDisplayNode()
    if colorNode != None:
      labelmapDisplayNode.SetAndObserveColorNodeID(colorNode.GetID())
    volumeNode.EndModify(wasModifying)

    self.get('MergeLabelsInputNodeComboBox').setCurrentNode(volumeNode)
    self.setupMergeLabels(volumeNode)
    self.get('PoseLabelmapInputNodeComboBox').setCurrentNode(volumeNode)
    self.get('LabelMapApplyColorNodePushButton').setChecked(False)

  def openLabelmapModule(self):
    self.openModule('Volumes')

  #    b) Transform
  def runTransform(self):
    volumeNode = self.get('LabelmapVolumeNodeComboBox').currentNode()
    if volumeNode == None:
      return

    self.get('TransformApplyPushButton').setChecked(True)
      
    volumeNode.SetAndObserveTransformNodeID(self.TransformNode.GetID())
    transformLogic = slicer.modules.transforms.logic()

    if transformLogic.hardenTransform(volumeNode):
      print "Transform succesful !"
    else:
      print "Transform failure !"
      
    self.get('TransformApplyPushButton').setChecked(False)

  #    c) Merge Labels
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
    self.get('MergeLabelsInputNodeComboBox').addAttribute('vtkMRMLScalarVolumeNode','LabelMap','1')
    self.get('MergeLabelsOutputNodeComboBox').addAttribute('vtkMRMLScalarVolumeNode','LabelMap','1')
    labelmapDisplayNode = volumeNode.GetDisplayNode()
    self.removeObservers(self.updateMergeLabels)
    colorNode = labelmapDisplayNode.GetColorNode()
    if colorNode == None:
      self.get('BoneLabelComboBox').setMRMLColorNode(None)
      self.get('SkinLabelComboBox').setMRMLColorNode(None)
      self.get('BoneLabelsLineEdit').setText('')
      self.get('BoneLabelComboBox').setCurrentColor(None)
      self.get('SkinLabelsLineEdit').setText('')
      self.get('SkinLabelComboBox').setCurrentColor(None)

    else:
      self.get('BoneLabelComboBox').setMRMLColorNode(colorNode)
      self.get('SkinLabelComboBox').setMRMLColorNode(colorNode)
      boneLabels = self.searchLabels(colorNode, 'bone')
      boneLabels.update(self.searchLabels(colorNode, 'vertebr'))
      self.get('BoneLabelsLineEdit').setText(', '.join(str( val ) for val in boneLabels.keys()))
      boneLabel = self.bestLabel(boneLabels, ['bone', 'cancellous'])
      self.get('BoneLabelComboBox').setCurrentColor(boneLabel)
      skinLabels = self.searchLabels(colorNode, 'skin')
      self.get('SkinLabelsLineEdit').setText(', '.join(str(val) for val in skinLabels.keys()))
      skinLabel = self.bestLabel(skinLabels, ['skin'])
      self.get('SkinLabelComboBox').setCurrentColor(skinLabel)

      self.addObserver(volumeNode, 'ModifiedEvent', self.updateMergeLabels)
      self.addObserver(labelmapDisplayNode, 'ModifiedEvent', self.updateMergeLabels)

  def searchLabels(self, colorNode, label):
    """ Search the color node for all the labels that contain the word 'label'
    """
    labels = {}
    for index in range(colorNode.GetNumberOfColors()):
      if label in colorNode.GetColorName(index).lower():
        labels[index] = colorNode.GetColorName(index)
    return labels

  def bestLabel(self, labels, labelNames):
    """ Return the label from a [index, colorName] map that fits the best the
         label name
    """
    bestLabels = labels
    if (len(bestLabels) == 0):
      return -1

    labelIndex = 0
    for labelName in labelNames:
      newBestLabels = {}
      for key in bestLabels.keys():
        startswith = bestLabels[key].lower().startswith(labelName)
        contains = labelName in bestLabels[key].lower()
        if (labelIndex == 0 and startswith) or (labelIndex > 0 and contains):
          newBestLabels[key] = bestLabels[key]
      if len(newBestLabels) == 1:
        return newBestLabels.keys()[0]
      bestLabels = newBestLabels
      labelIndex = labelIndex + 1
    return bestLabels.keys()[0]

  def mergeLabelsParameters(self):
    boneLabels = self.get('BoneLabelsLineEdit').text
    skinLabels = self.get('SkinLabelsLineEdit').text
    parameters = {}
    parameters["InputVolume"] = self.get('MergeLabelsInputNodeComboBox').currentNode()
    parameters["OutputVolume"] = self.get('MergeLabelsOutputNodeComboBox').currentNode()
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
    return parameters

  def runMergeLabels(self):
    cliNode = self.getCLINode(slicer.modules.changelabel)
    parameters = self.mergeLabelsParameters()
    self.get('MergeLabelsApplyPushButton').setChecked(True)
    cliNode = slicer.cli.run(slicer.modules.changelabel, cliNode, parameters, wait_for_completion = True)
    self.get('MergeLabelsApplyPushButton').setChecked(False)

    if cliNode.GetStatusString() == 'Completed':
      print 'MergeLabels completed'
      # apply label map
      newNode = self.get('MergeLabelsOutputNodeComboBox').currentNode()
      colorNode = self.get('LabelmapColorNodeComboBox').currentNode()
      if newNode != None and colorNode != None:
        newNode.GetDisplayNode().SetAndObserveColorNodeID(colorNode.GetID())

    else:
      print 'MergeLabels failed'


  def openMergeLabelsModule(self):
    self.openModule('ChangeLabel')

    cliNode = self.getCLINode(slicer.modules.changelabel)
    parameters = self.mergeLabelsParameters()
    slicer.cli.setNodeParameters(cliNode, parameters)

  # 2) Model Maker
  def openExtractPage(self):
    if self.get('BoneModelMakerOutputNodeComboBox').currentNode() == None:
      self.get('BoneModelMakerOutputNodeComboBox').addNode()
    if self.get('SkinModelMakerOutputNodeComboBox').currentNode() == None:
      self.get('SkinModelMakerOutputNodeComboBox').addNode()

  #     a) Bone Model Maker
  def setupBoneModelMakerLabels(self):
    """ Update the labels of the bone model maker
    """
    labels = []
    labels.append(self.get('BoneLabelComboBox').currentColor)
    self.get('BoneModelMakerLabelsLineEdit').setText(', '.join(str(val) for val in labels))

  def boneModelMakerParameters(self):
    parameters = {}
    parameters["InputVolume"] = self.get('BoneModelMakerInputNodeComboBox').currentNode()
    parameters["ModelSceneFile"] = self.get('BoneModelMakerOutputNodeComboBox').currentNode()
    parameters["Labels"] = self.get('BoneModelMakerLabelsLineEdit').text
    parameters["Name"] = 'Bones'
    parameters['GenerateAll'] = False
    parameters["JointSmoothing"] = False
    parameters["SplitNormals"] = True
    parameters["PointNormals"] = True
    parameters["SkipUnNamed"] = True
    parameters["Decimate"] = 0.25
    parameters["Smooth"] = 10
    return parameters

  def runBoneModelMaker(self):
    cliNode = self.getCLINode(slicer.modules.modelmaker)
    parameters = self.boneModelMakerParameters()
    self.get('BoneModelMakerApplyPushButton').setChecked(True)
    cliNode = slicer.cli.run(slicer.modules.modelmaker, cliNode, parameters, wait_for_completion = True)
    self.get('BoneModelMakerApplyPushButton').setChecked(False)
    if cliNode.GetStatusString() == 'Completed':
      print 'Bone ModelMaker completed'
      self.resetCamera()
    else:
      print 'ModelMaker failed'

  def openModelsModule(self):
    self.openModule('Models')

  def openBoneModelMakerModule(self):
    self.openModule('ModelMaker')

    cliNode = self.getCLINode(slicer.modules.modelmaker)
    parameters = self.boneModelMakerParameters()
    slicer.cli.setNodeParameters(cliNode, parameters)

  #     b) Skin Model Maker
  def setupSkinModelMakerLabels(self, volumeNode):
    """ Update the labels of the skin model maker
    """
    if volumeNode == None:
      return

    labelmapDisplayNode = volumeNode.GetDisplayNode()
    if labelmapDisplayNode == None:
      return

    colorNode = labelmapDisplayNode.GetColorNode()
    if colorNode == None:
      self.get('SkinModelMakerLabelsLineEdit').setText('')
    else:
      airLabels = self.searchLabels(colorNode, 'air')
      if len(airLabels) > 0:
        self.get('SkinModelMakerThresholdSpinBox').setValue( min(airLabels) + 0.1 ) # highly probable outside is lowest label
      else:
        self.get('SkinModelMakerThresholdSpinBox').setValue(0.1) # highly probable outside is 0

  def skinModelMakerParameters(self):
    parameters = {}
    parameters["InputVolume"] = self.get('SkinModelMakerInputNodeComboBox').currentNode()
    parameters["OutputGeometry"] = self.get('SkinModelMakerOutputNodeComboBox').currentNode()
    parameters["Threshold"] = self.get('SkinModelMakerThresholdSpinBox').value + 0.1
    #parameters["SplitNormals"] = True
    #parameters["PointNormals"] = True
    #parameters["Decimate"] = 0.25
    parameters["Smooth"] = 10
    return parameters

  def runSkinModelMaker(self):
    cliNode = self.getCLINode(slicer.modules.grayscalemodelmaker)
    parameters = self.skinModelMakerParameters()
    self.get('SkinModelMakerApplyPushButton').setChecked(True)
    cliNode = slicer.cli.run(slicer.modules.grayscalemodelmaker, cliNode, parameters, wait_for_completion = True)
    self.get('SkinModelMakerApplyPushButton').setChecked(False)
    if cliNode.GetStatusString() == 'Completed':
      print 'Skin ModelMaker completed'
      # Set opacity
      newNode = self.get('SkinModelMakerOutputNodeComboBox').currentNode()
      newNodeDisplayNode = newNode.GetModelDisplayNode()
      newNodeDisplayNode.SetOpacity(0.2)

      # Set color
      colorNode = self.get('SkinModelMakerInputNodeComboBox').currentNode().GetDisplayNode().GetColorNode()
      color = [0, 0, 0]
      lookupTable = colorNode.GetLookupTable().GetColor(self.get('SkinLabelComboBox').currentColor, color)
      newNodeDisplayNode.SetColor(color)

      # Set Clip intersection ON
      newNodeDisplayNode.SetSliceIntersectionVisibility(1)

      # Reset camera
      self.resetCamera()
    else:
      print 'Skin ModelMaker failed'

  def openSkinModelMakerModule(self):
    self.openModule('GrayscaleModelMaker')

    cliNode = self.getCLINode(slicer.modules.grayscalemodelmaker)
    parameters = self.skinModelMakerParameters()
    slicer.cli.setNodeParameters(cliNode, parameters)

  def updateSkinNodeVisibility(self):
    skinModel = self.get('SkinModelMakerOutputNodeComboBox').currentNode()
    if skinModel != None:
      skinModel.SetDisplayVisibility(not skinModel.GetDisplayVisibility())

  #     c) Volume Render
  def updateVolumeRender(self, volumeNode, event):
    if volumeNode != self.get('VolumeRenderInputNodeComboBox').currentNode():
      return
    self.setupVolumeRender(volumeNode)

  def setupVolumeRender(self, volumeNode):
    self.removeObservers(self.updateVolumeRender)
    if volumeNode == None:
      return
    displayNode = volumeNode.GetNthDisplayNodeByClass(0, 'vtkMRMLVolumeRenderingDisplayNode')
    visible = False
    if displayNode != None:
      visible = displayNode.GetVisibility()
    self.get('VolumeRenderCheckBox').setChecked(visible)
    self.setupVolumeRenderLabels()
    self.addObserver(volumeNode, 'ModifiedEvent', self.updateVolumeRender)

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

  # 3) Create rest armature
  def openCreateArmaturePage(self):
    self.resetCamera()
    # Switch to 3D View only
    manager = slicer.app.layoutManager()
    manager.setLayout(slicer.vtkMRMLLayoutNode.SlicerLayoutOneUp3DView)

  # 3.A) Armature
  def loadArmatureFile(self):
    manager = slicer.app.ioManager()
    if manager.openDialog('ArmatureFile', slicer.qSlicerFileDialog.Read):
      print 'Armature loaded successfully'
    else:
      print 'Armature loading failed'

  def openArmaturesModule(self):
    # Finaly open armature module
    self.openModule('Armatures')

  # 4) Skinning
  def openSkinningPage(self):
    # Switch to FourUp
    slicer.app.layoutManager().setLayout(slicer.vtkMRMLLayoutNode.SlicerLayoutFourUpView)
    # Get model from armature
    activeArmatureNode = slicer.modules.armatures.logic().GetActiveArmature()
    if activeArmatureNode != None:
      self.get('VolumeSkinningAmartureNodeComboBox').setCurrentNode(activeArmatureNode.GetAssociatedNode())

  #  a) Volume Skinning
  def volumeSkinningParameters(self):
    parameters = {}
    parameters["RestVolume"] = self.get('VolumeSkinningInputVolumeNodeComboBox').currentNode()
    parameters["ArmaturePoly"] = self.get('VolumeSkinningAmartureNodeComboBox').currentNode()
    parameters["SkinnedVolume"] = self.get('VolumeSkinningOutputVolumeNodeComboBox').currentNode()
    #parameters["Padding"] = 1
    #parameters["Debug"] = False
    #parameters["ArmatureInRAS"] = False
    return parameters

  def runVolumeSkinning(self):
    cliNode = self.getCLINode(slicer.modules.volumeskinning)
    parameters = self.volumeSkinningParameters()
    self.get('VolumeSkinningApplyPushButton').setChecked(True)
    cliNode = slicer.cli.run(slicer.modules.volumeskinning, cliNode, parameters, wait_for_completion = True)
    self.get('VolumeSkinningApplyPushButton').setChecked(False)

    if cliNode.GetStatusString() == 'Completed':
      print 'Volume Skinning completed'
    else:
      print 'Volume Skinning failed'

  def openVolumeSkinningModule(self):
    self.openModule('VolumeSkinning')

    cliNode = self.getCLINode(slicer.modules.volumeskinning)
    parameters = self.volumeSkinningParameters()
    slicer.cli.setNodeParameters(cliNode, parameters)

  def createOutputSkinnedVolume(self, node):
    if node == None:
      return

    nodeName = '%s-skinned' % node.GetName()
    if self.getFirstNodeByNameAndClass(nodeName, 'vtkMRMLScalarVolumeNode') == None:
      newNode = self.get('VolumeSkinningOutputVolumeNodeComboBox').addNode()
      newNode.SetName(nodeName)

    #  b) Compute Armature Weight
  def computeArmatureWeightParameters(self):
    parameters = {}
    parameters["RestLabelmap"] = self.get('ComputeArmatureWeightInputVolumeNodeComboBox').currentNode()
    parameters["ArmaturePoly"] = self.get('ComputeArmatureWeightAmartureNodeComboBox').currentNode()
    parameters["SkinnedVolume"] = self.get('ComputeArmatureWeightSkinnedVolumeVolumeNodeComboBox').currentNode()
    parameters["WeightDirectory"] = str(self.get('ComputeArmatureWeightOutputDirectoryButton').directory)
    #parameters["FirstEdge"] = 0
    #parameters["LastEdge"] = -1
    #parameters["BinaryWeight"] = False
    #parameters["SmoothingIteration"] = 10
    #parameters["Debug"] = False
    #parameters["RunSequential"] = False
    return parameters

  def runComputeArmatureWeight(self):
    cliNode = self.getCLINode(slicer.modules.computearmatureweight)
    parameters = self.computeArmatureWeightParameters()
    self.get('ComputeArmatureWeightApplyPushButton').setChecked(True)
    cliNode = slicer.cli.run(slicer.modules.computearmatureweight, cliNode, parameters, wait_for_completion = True)
    self.get('ComputeArmatureWeightApplyPushButton').setChecked(False)

    if cliNode.GetStatusString() == 'Completed':
      print 'Compute Armature Weight completed'
    else:
      print 'Compute Armature Weight failed'

  def openComputeArmatureWeightModule(self):
    self.openModule('ComputeArmatureWeight')

    cliNode = self.getCLINode(slicer.modules.computearmatureweight)
    parameters = self.computeArmatureWeightParameters()
    slicer.cli.setNodeParameters(cliNode, parameters)

  # 5) (Pose) Armature And Pose Body
  def openPoseArmaturePage(self):
    armatureLogic = slicer.modules.armatures.logic()
    if armatureLogic != None:
      armatureLogic.SetActiveArmatureWidgetState(3) # 3 is Pose

  # a) Eval Weight
  def evalSurfaceWeightParameters(self):
    parameters = {}
    parameters["InputSurface"] = self.get('EvalSurfaceWeightInputNodeComboBox').currentNode()
    parameters["OutputSurface"] = self.get('EvalSurfaceWeightOutputNodeComboBox').currentNode()
    parameters["WeightDirectory"] = str(self.get('EvalSurfaceWeightWeightDirectoryButton').directory)
    #parameters["IsSurfaceInRAS"] = False
    #parameters["PrintDebug"] = False
    return parameters

  def runEvalSurfaceWeight(self):
    cliNode = self.getCLINode(slicer.modules.evalsurfaceweight)
    parameters = self.evalSurfaceWeightParameters()
    self.get('EvalSurfaceWeightApplyPushButton').setChecked(True)
    cliNode = slicer.cli.run(slicer.modules.evalsurfaceweight, cliNode, parameters, wait_for_completion = True)
    self.get('EvalSurfaceWeightApplyPushButton').setChecked(False)

    if cliNode.GetStatusString() == 'Completed':
      print 'Evaluate Weight completed'
    else:
      print 'Evaluate Weight failed'

  def openEvalSurfaceWeight(self):
    self.openModule('EvalSurfaceWeight')

    cliNode = self.getCLINode(slicer.modules.evalweight)
    parameters = self.evalWeightParameters()
    slicer.cli.setNodeParameters(cliNode, parameters)

  # b) (Pose) Armatures
  def openPosedArmatureModule(self):
    self.openModule('Armatures')

  # c) Pose Body
  def poseSurfaceParameterChanged(self):
    cliNode = self.getCLINode(slicer.modules.posesurface)
    if cliNode.IsBusy() == True:
      cliNode.Cancel()

  def poseSurfaceParameters(self):
    # Setup CLI node on input changed or apply changed
    parameters = {}
    parameters["ArmaturePoly"] = self.get('PoseSurfaceArmatureInputNodeComboBox').currentNode()
    parameters["SurfaceInput"] = self.get('PoseSurfaceInputComboBox').currentNode()
    parameters["WeightDirectory"] = str(self.get('PoseSurfaceWeightInputDirectoryButton').directory)
    parameters["OutputSurface"] = self.get('PoseSurfaceOutputNodeComboBox').currentNode()
    #parameters["IsSurfaceInRAS"] = False
    #parameters["IsArmatureInRAS"] = False
    parameters["LinearBlend"] = True
    return parameters

  def runPoseSurface(self):
    cliNode = self.getCLINode(slicer.modules.posesurface)
    parameters = self.poseSurfaceParameters()
    slicer.cli.setNodeParameters(cliNode, parameters)
    cliNode.SetAutoRunMode(cliNode.AutoRunOnAnyInputEvent)

    if self.get('PoseSurfaceApplyPushButton').checkState == False:
      if cliNode.IsBusy() == False:
        self.get('PoseSurfaceApplyPushButton').setChecked(True)
        slicer.modules.posesurface.logic().ApplyAndWait(cliNode)
        self.get('PoseSurfaceApplyPushButton').setChecked(False)
      else:
        cliNode.Cancel()
    else:
      cliNode.SetAutoRun(self.get('PoseSurfaceApplyPushButton').isChecked())

  def openPoseSurfaceModule(self):
    self.openModule('PoseSurface')

    cliNode = self.getCLINode(slicer.modules.posesurface)
    parameters = self.poseSurfaceParameterss()
    slicer.cli.setNodeParameters(cliNode, parameters)

  def setWeightDirectory(self, dir):
    if self.get('EvalSurfaceWeightWeightDirectoryButton').directory != dir:
      self.get('EvalSurfaceWeightWeightDirectoryButton').directory = dir

    if self.get('PoseSurfaceWeightInputDirectoryButton').directory != dir:
      self.get('PoseSurfaceWeightInputDirectoryButton').directory = dir

    if self.get('PoseLabelmapWeightDirectoryButton').directory != dir:
      self.get('PoseLabelmapWeightDirectoryButton').directory = dir

  def createOutputSurface(self, node):
    if node == None:
      return

    nodeName = '%s-posed' % node.GetName()
    if self.getFirstNodeByNameAndClass(nodeName, 'vtkMRMLModelNode') == None:
      newNode = self.get('PoseSurfaceOutputNodeComboBox').addNode()
      newNode.SetName(nodeName)

  # 6) Resample NOTE: SHOULD BE LAST STEP
  def openPoseLabelmapPage(self):
    pass

  def poseLabelmapParameters(self):
    parameters = {}
    parameters["RestLabelmap"] = self.get('PoseLabelmapInputNodeComboBox').currentNode()
    parameters["ArmaturePoly"] = self.get('PoseLabelmapArmatureNodeComboBox').currentNode()
    parameters["WeightDirectory"] = str(self.get('PoseLabelmapWeightDirectoryButton').directory)
    parameters["PosedLabelmap"] = self.get('PoseLabelmapOutputNodeComboBox').currentNode()
    parameters["LinearBlend"] = True
    #parameters["MaximumPass"] = 4
    #parameters["Debug"] = False
    #parameters["IsArmatureInRAS"] = False
    return parameters

  def runPoseLabelmap(self):
    cliNode = self.getCLINode(slicer.modules.poselabelmap)
    parameters = self.poseLabelmapParameters()
    self.get('PoseLabelmapApplyPushButton').setChecked(True)
    cliNode = slicer.cli.run(slicer.modules.poselabelmap, cliNode, parameters, wait_for_completion = True)
    self.get('PoseLabelmapApplyPushButton').setChecked(False)
    if cliNode.GetStatusString() == 'Completed':
      print 'Pose Labelmap completed'
    else:
      print 'Pose Labelmap failed'

  def openPoseLabelmap(self):
    self.openModule('PoseLabelmap')

    cliNode = self.getCLINode(slicer.modules.poselabelmap)
    parameters = self.poseLabelmapParameters()
    slicer.cli.setNodeParameters(cliNode, parameters)

  def createOutputPoseLabelmap(self, node):
    if node == None:
      return

    nodeName = '%s-posed' % node.GetName()
    if self.getFirstNodeByNameAndClass(nodeName, 'vtkMRMLScalarVolumeNode') == None:
      newNode = self.get('PoseLabelmapOutputNodeComboBox').addNode()
      newNode.SetName(nodeName)

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

  def removeObservers(self, method):
    for object, event, method, group, tag in self.Observations:
      if method == method:
        object.RemoveObserver(tag)

  def addObserver(self, object, event, method, group = 'none'):
    if self.hasObserver(object, event, method):
      return
    tag = object.AddObserver(event, method)
    self.Observations.append([object, event, method, group, tag])

  def hasObserver(self, object, event, method):
    for o, e, m, g, t in self.Observations:
      if o == object and e == event and m == method:
        return True
    return False

  def getCLINode(self, cliModule):
    """ Return the cli node to use for a given CLI module. Create the node in
    scene if needed.
    """
    cliNode = slicer.mrmlScene.GetFirstNodeByName(cliModule.title)
    if cliNode == None:
      cliNode = slicer.cli.createNode(cliModule)
      cliNode.SetName(cliModule.title)
    return cliNode

  def resetCamera(self):
    # Reset focal view around volumes
    manager = slicer.app.layoutManager()
    for i in range(0, manager.threeDViewCount):
      manager.threeDWidget(i).threeDView().resetFocalPoint()

  def openModule(self, moduleName):
    slicer.util.selectModule(moduleName)

  def getFirstNodeByNameAndClass(self, name, className):
    nodes = slicer.mrmlScene.GetNodesByClass(className)
    for i in range(0, nodes.GetNumberOfItems()):
      node = nodes.GetItemAsObject(i)
      if node.GetName() == name:
        return node
    return None

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
