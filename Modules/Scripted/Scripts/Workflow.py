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
Step by step workflow to reposition a labelmap. See <a href=\"$a/Documentation/$b.$c/Modules/Workflow\">$a/Documentation/$b.$c/Modules/Workflow</a> for more information.
    """).substitute({ 'a':'http://public.kitware.com/Wiki/Bender', 'b':1, 'c':0 })
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

    # Global variables
    self.StatusModifiedEvent = slicer.vtkMRMLCommandLineModuleNode().StatusModifiedEvent

    # Labelmap variables

    # Compute Weight variables
    self.volumeSkinningCreateOutputConnected = False

    # Pose surface variables
    self.poseSurfaceCreateOutputConnected = False

    # Pose surface variables
    self.poseLabelmapCreateOutputConnected = False

    # init pages
    self.initAdjustPage()

    # Load/Save icons
    loadIcon = self.WorkflowWidget.style().standardIcon(qt.QStyle.SP_DialogOpenButton)
    saveIcon = self.WorkflowWidget.style().standardIcon(qt.QStyle.SP_DialogSaveButton)
    self.get('LabelmapVolumeNodeToolButton').icon = loadIcon
    self.get('LabelmapColorNodeToolButton').icon = loadIcon
    self.get('MergeLabelsOutputNodeToolButton').icon = saveIcon
    self.get('BoneModelMakerOutputNodeToolButton').icon = saveIcon
    self.get('SkinModelMakerOutputNodeToolButton').icon = saveIcon
    self.get('ArmaturesArmatureLoadToolButton').icon = loadIcon
    self.get('ArmaturesArmatureSaveToolButton').icon = saveIcon
    self.get('VolumeSkinningInputVolumeNodeToolButton').icon = loadIcon
    self.get('VolumeSkinningOutputVolumeNodeToolButton').icon = saveIcon
    self.get('EditSkinnedVolumeNodeToolButton').icon = loadIcon
    self.get('EditSkinnedVolumeNodeSaveToolButton').icon = saveIcon
    self.get('EvalSurfaceWeightInputNodeToolButton').icon = loadIcon
    self.get('EvalSurfaceWeightOutputNodeToolButton').icon = saveIcon
    self.get('PoseArmatureArmatureNodeToolButton').icon = loadIcon
    self.get('PoseArmatureArmatureNodeSaveToolButton').icon = saveIcon
    self.get('PoseSurfaceInputNodeToolButton').icon = loadIcon
    self.get('PoseSurfaceOutputNodeToolButton').icon = saveIcon
    self.get('PoseLabelmapOutputNodeToolButton').icon = saveIcon

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
    self.get('LabelmapColorNodeComboBox').connect('nodeActivated(vtkMRMLNode*)', self.applyColorNode)
    self.get('LabelmapVolumeNodeToolButton').connect('clicked()', self.loadLabelmapVolumeNode)
    self.get('LabelmapColorNodeToolButton').connect('clicked()', self.loadLabelmapColorNode)
    self.get('LabelMapApplyColorNodePushButton').connect('clicked()', self.applyColorNode)
    self.get('LabelmapGoToModulePushButton').connect('clicked()', self.openLabelmapModule)
    self.get('LPSRASTransformPushButton').connect('clicked()', self.runLPSRASTransform)
    # 2) Model Maker
    # a) Merge Labels
    self.get('MergeLabelsInputNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.setupMergeLabels)
    self.get('MergeLabelsOutputNodeToolButton').connect('clicked()', self.saveMergeLabelsVolumeNode)
    self.get('MergeLabelsApplyPushButton').connect('clicked(bool)', self.runMergeLabels)
    self.get('MergeLabelsGoToModulePushButton').connect('clicked()', self.openMergeLabelsModule)
    # b) Bone Model Maker
    self.get('BoneLabelComboBox').connect('currentColorChanged(int)', self.setupBoneModelMakerLabels)
    self.get('BoneModelMakerOutputNodeComboBox').connect('currentColorChanged(int)', self.setupBoneModelMakerLabels)
    self.get('BoneModelMakerOutputNodeToolButton').connect('clicked()', self.saveBoneModelMakerModelNode)
    self.get('BoneModelMakerApplyPushButton').connect('clicked(bool)', self.runBoneModelMaker)
    self.get('BoneModelMakerGoToModelsModulePushButton').connect('clicked()', self.openModelsModule)
    self.get('BoneModelMakerGoToModulePushButton').connect('clicked()', self.openBoneModelMakerModule)
    # c) Skin Model Maker
    self.get('SkinModelMakerInputNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.setupSkinModelMakerLabels)
    self.get('SkinModelMakerOutputNodeToolButton').connect('clicked()', self.saveSkinModelMakerModelNode)
    self.get('SkinModelMakerToggleVisiblePushButton').connect('clicked()', self.updateSkinNodeVisibility)
    self.get('SkinModelMakerApplyPushButton').connect('clicked(bool)', self.runSkinModelMaker)
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
    self.get('ArmaturesArmatureNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.setCurrentArmatureModelNode)
    self.get('ArmaturesToggleVisiblePushButton').connect('clicked()', self.updateSkinNodeVisibility)
    self.get('ArmaturesArmatureLoadToolButton').connect('clicked()', self.loadArmatureNode)
    self.get('ArmaturesArmatureSaveToolButton').connect('clicked()', self.saveArmatureNode)
    self.get('ArmaturesGoToPushButton').connect('clicked()', self.openArmaturesModule)
    # 4) Skinning
    # a) Volume Skinning
    self.get('VolumeSkinningInputVolumeNodeToolButton').connect('clicked()', self.loadSkinningInputVolumeNode)
    self.get('VolumeSkinningOutputVolumeNodeToolButton').connect('clicked()', self.saveSkinningVolumeNode)
    self.get('VolumeSkinningApplyPushButton').connect('clicked(bool)',self.runVolumeSkinning)
    self.get('VolumeSkinningGoToPushButton').connect('clicked()', self.openVolumeSkinningModule)
    # b) Edit skinned volume
    self.get('EditSkinnedVolumeNodeToolButton').connect('clicked()', self.loadEditSkinnedVolumeNode)
    self.get('EditSkinnedVolumeNodeSaveToolButton').connect('clicked()', self.saveEditSkinnedVolumeNode)
    self.get('EditSkinnedVolumeNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.editSkinnedVolumeParameterChanged)
    self.get('EditSkinnedVolumeGoToEditorPushButton').connect('clicked()', self.openEditorModule)
    # 5) Weights
    # a) Armatures Weight
    self.get('ComputeArmatureWeightApplyPushButton').connect('clicked(bool)',self.runComputeArmatureWeight)
    self.get('ComputeArmatureWeightGoToPushButton').connect('clicked()', self.openComputeArmatureWeightModule)
    # b) Eval Weight
    self.get('EvalSurfaceWeightInputNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.evalSurfaceWeightParameterChanged)
    self.get('EvalSurfaceWeightWeightPathLineEdit').connect('currentPathChanged(QString)', self.evalSurfaceWeightParameterChanged)
    self.get('EvalSurfaceWeightOutputNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.evalSurfaceWeightParameterChanged)
    self.get('EvalSurfaceWeightInputNodeToolButton').connect('clicked()', self.loadEvalSurfaceWeightInputNode)
    self.get('EvalSurfaceWeightOutputNodeToolButton').connect('clicked()', self.saveEvalSurfaceWeightOutputNode)
    self.get('EvalSurfaceWeightApplyPushButton').connect('clicked(bool)', self.runEvalSurfaceWeight)
    self.get('EvalSurfaceWeightGoToPushButton').connect('clicked()', self.openEvalSurfaceWeight)
    self.get('EvalSurfaceWeightWeightPathLineEdit').connect('currentPathChanged(QString)', self.setWeightDirectory)
    # 6) (Pose) Armature And Pose Body
    # a) Pose Armature
    self.get('PoseArmatureArmatureNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.setPoseArmatureModelNode)
    self.get('PoseArmatureArmatureNodeToolButton').connect('clicked()', self.loadArmatureNode)
    self.get('PoseArmatureArmatureNodeSaveToolButton').connect('clicked()', self.savePoseArmatureArmatureNode)
    self.get('PoseArmaturesGoToPushButton').connect('clicked()', self.openPosedArmatureModule)
    # b) Pose Surface
    self.get('PoseSurfaceInputNodeToolButton').connect('clicked()', self.loadPoseSurfaceInputNode)
    self.get('PoseSurfaceOutputNodeToolButton').connect('clicked()', self.savePoseSurfaceOutputNode)
    self.get('PoseSurfaceOutputNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.poseSurfaceParameterChanged)
    self.get('PoseSurfaceWeightInputPathLineEdit').connect('currentPathChanged(QString)', self.poseSurfaceParameterChanged)
    self.get('PoseSurfaceInputNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.poseSurfaceParameterChanged)
    self.get('PoseSurfaceArmatureInputNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.poseSurfaceParameterChanged)
    self.get('PoseSurfaceApplyPushButton').connect('clicked(bool)', self.runPoseSurface)
    self.get('PoseSurfaceApplyPushButton').connect('checkBoxToggled(bool)', self.autoRunPoseSurface)

    self.get('PoseSurfaceGoToPushButton').connect('clicked()', self.openPoseSurfaceModule)
    self.get('ComputeArmatureWeightOutputPathLineEdit').connect('currentPathChanged(QString)', self.setWeightDirectory)
    # 7) Resample
    self.get('PoseLabelmapInputNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.poseLabelmapParameterChanged)
    self.get('PoseLabelmapArmatureNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.poseLabelmapParameterChanged)
    self.get('PoseLabelmapWeightPathLineEdit').connect('currentPathChanged(QString)', self.poseLabelmapParameterChanged)
    self.get('PoseLabelmapOutputNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.poseLabelmapParameterChanged)
    self.get('PoseLabelmapOutputNodeToolButton').connect('clicked()', self.savePoseLabelmapOutputNode)
    self.get('PoseLabelmapApplyPushButton').connect('clicked(bool)', self.runPoseLabelmap)
    self.get('PoseLabelmapGoToPushButton').connect('clicked()', self.openPoseLabelmap)

    self.openPage = { 0 : self.openAdjustPage,
                      1 : self.openExtractPage,
                      2 : self.openCreateArmaturePage,
                      3 : self.openSkinningPage,
                      4 : self.openWeightsPage,
                      5 : self.openPoseArmaturePage,
                      6 : self.openPoseLabelmapPage
                      }
    # --------------------------------------------------------------------------
    # Initialize all the MRML aware GUI elements.
    # Lots of setup methods are called from this line
    self.setupComboboxes()
    self.widget.setMRMLScene(slicer.mrmlScene)

    # Init title
    self.updateHeader()

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
    self.get('WelcomeSimpleWorkflowCheckBox').updatesEnabled = False
    self.openPage[self.WorkflowWidget.currentIndex]()
    wasChecked = self.get('WelcomeSimpleWorkflowCheckBox').checked
    self.get('WelcomeSimpleWorkflowCheckBox').checked = True
    self.WorkflowWidget.maximumHeight = self.WorkflowWidget.currentWidget().sizeHint.height()
    self.get('WelcomeSimpleWorkflowCheckBox').checked = wasChecked
    self.get('WelcomeSimpleWorkflowCheckBox').updatesEnabled = True
    self.WorkflowWidget.resize(self.WorkflowWidget.width,
      self.WorkflowWidget.currentWidget().sizeHint.height())
    # Hide the Status if not running
    cliNode = self.get('CLIProgressBar').commandLineModuleNode()
    if cliNode != None and not cliNode.IsBusy():
      self.get('CLIProgressBar').setCommandLineModuleNode(0)

  def goToPrevious(self):
    self.WorkflowWidget.setCurrentIndex(self.WorkflowWidget.currentIndex - 1)
    self.updateHeader()

  def goToNext(self):
    self.WorkflowWidget.setCurrentIndex(self.WorkflowWidget.currentIndex + 1)
    self.updateHeader()

  #----------------------------------------------------------------------------
  # 0) Welcome
  #----------------------------------------------------------------------------
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
                                     'BoneModelMakerDecimateLabel', 'BoneModelMakerDecimateSliderWidget',
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
    # VolumeSkinningInputVolumeNodeComboBox is not advanced as it can be the merged volume or the original volume.
    advancedVolumeSkinningWidgets = ['VolumeSkinningArmatureLabel','VolumeSkinningAmartureNodeComboBox',
                                    'VolumeSkinningGoToPushButton']
    self.setWidgetsVisibility(advancedVolumeSkinningWidgets, advanced)
    # b) Weights
    advancedComputeWeightWidgets = ['ComputeArmatureWeightInputVolumeLabel', 'ComputeArmatureWeightInputVolumeNodeComboBox',
                                   'ComputeArmatureWeightArmatureLabel', 'ComputeArmatureWeightAmartureNodeComboBox',
                                   'ComputeArmatureWeightPaddingLabel', 'ComputeArmatureWeightPaddingSpinBox',
                                   'ComputeArmatureWeightGoToPushButton']
    self.setWidgetsVisibility(advancedComputeWeightWidgets, advanced)

    # 5) Pose Page
    # a) Armatures
    # Nothing
    # b) Eval Weight
    advancedEvalSurfaceWeightWidgets = [
                                 'EvalSurfaceWeightWeightDirectoryLabel', 'EvalSurfaceWeightWeightPathLineEdit',
                                 'EvalSurfaceWeightGoToPushButton']
    self.setWidgetsVisibility(advancedEvalSurfaceWeightWidgets, advanced)

    # c) Pose Surface
    # hide almost completely
    advancedPoseSurfaceWidgets = ['PoseSurfaceArmaturesLabel', 'PoseSurfaceArmatureInputNodeComboBox',
                               'PoseSurfaceInputLabel', 'PoseSurfaceInputNodeComboBox', 'PoseSurfaceInputNodeToolButton',
                               'PoseSurfaceWeightInputFolderLabel', 'PoseSurfaceWeightInputPathLineEdit',
                               'PoseSurfaceGoToPushButton']
    self.setWidgetsVisibility(advancedPoseSurfaceWidgets, advanced)

    # 6) Resample
    # Hide all but output
    advancedPoseLabemapWidgets = ['PoseLabelmapInputLabel', 'PoseLabelmapInputNodeComboBox',
                                  'PoseLabelmapArmatureLabel', 'PoseLabelmapArmatureNodeComboBox',
                                  'PoseLabelmapWeightDirectoryLabel', 'PoseLabelmapWeightPathLineEdit',
                                  'PoseLabelmapGoToPushButton']
    self.setWidgetsVisibility(advancedPoseLabemapWidgets, advanced)

  def setupComboboxes(self):
    # Add here the combo box that should only see labelmaps
    labeldMapComboBoxes = ['MergeLabelsInputNodeComboBox', 'MergeLabelsOutputNodeComboBox',
                           'VolumeSkinningInputVolumeNodeComboBox', 'VolumeSkinningOutputVolumeNodeComboBox',
                           'EditSkinnedVolumeNodeComboBox',
                           'ComputeArmatureWeightInputVolumeNodeComboBox', 'ComputeArmatureWeightSkinnedVolumeVolumeNodeComboBox',
                           'PoseLabelmapInputNodeComboBox', 'PoseLabelmapOutputNodeComboBox']

    for combobox in labeldMapComboBoxes:
      self.get(combobox).addAttribute('vtkMRMLScalarVolumeNode','LabelMap','1')

  def observeCLINode(self, cliNode, onCLINodeModified = None):
    if cliNode != None and onCLINodeModified != None:
      self.addObserver(cliNode, self.StatusModifiedEvent, onCLINodeModified)
    self.get('CLIProgressBar').setCommandLineModuleNode(cliNode)

  #----------------------------------------------------------------------------
  #     c) Volume Render
  #----------------------------------------------------------------------------
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

  #----------------------------------------------------------------------------
  # 1) Load inputs
  #----------------------------------------------------------------------------

  def initAdjustPage(self):
    # Init color node combo box <=> make 'Generic Colors' labelmap visible
    model = self.get('LabelmapColorNodeComboBox').sortFilterProxyModel()
    visibleNodeIDs = []
    visibleNodeIDs.append(slicer.mrmlScene.GetFirstNodeByName('GenericAnatomyColors').GetID())
    model.visibleNodeIDs = visibleNodeIDs

    # LPS <-> RAS Transform
    transformMenu = qt.QMenu(self.get('LPSRASTransformPushButton'))
    a = transformMenu.addAction('Left <-> Right')
    a.setToolTip('Switch the volume orientation from Left to Right')
    a.connect('triggered(bool)', self.runLRTransform)
    a = transformMenu.addAction('Posterior <-> Anterior')
    a.setToolTip('Switch the volume orientation from Posterior to Anterior')
    a.connect('triggered(bool)', self.runPATransform)
    a = transformMenu.addAction('Superior <-> Inferior')
    a.setToolTip('Switch the volume orientation from Superior to Inferior')
    a.connect('triggered(bool)', self.runSITransform)
    self.get('LPSRASTransformPushButton').setMenu(transformMenu)
    a = transformMenu.addAction('Center')
    a.setToolTip('Center volume on (0,0,0)')
    a.connect('triggered(bool)', self.runCenter)
    self.get('LPSRASTransformPushButton').setMenu(transformMenu)

    self.get('LabelMapApplyColorNodePushButton').visible = False

  def openAdjustPage(self):
    # Switch to 3D View only
    slicer.app.layoutManager().setLayout(slicer.vtkMRMLLayoutNode.SlicerLayoutFourUpView)

  #----------------------------------------------------------------------------
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

    # Labelmapcolornode should get its scene before the volume node selector
    # gets it. That way, setCurrentNode can work at first
    self.get('LabelmapColorNodeComboBox').setCurrentNode(volumeNode.GetDisplayNode().GetColorNode())
    self.addObserver(volumeNode, 'ModifiedEvent', self.updateLabelmap)
    self.addObserver(volumeNode.GetDisplayNode(), 'ModifiedEvent', self.updateLabelmap)

  def loadLabelmapVolumeNode(self):
    self.loadFile('Volume/Labelmap to reposition', 'VolumeFile', self.get('LabelmapVolumeNodeComboBox'))

  def loadLabelmapColorNode(self):
    self.loadFile('Tissue/Color file', 'ColorTableFile', self.get('LabelmapColorNodeComboBox'))

  def applyColorNode(self):
    volumeNode = self.get('LabelmapVolumeNodeComboBox').currentNode()
    if volumeNode == None:
      return

    colorNode = self.get('LabelmapColorNodeComboBox').currentNode()
    volumesLogic = slicer.modules.volumes.logic()

    wasModifying = volumeNode.StartModify()
    volumesLogic.SetVolumeAsLabelMap(volumeNode, colorNode != None) # Greyscale is None

    labelmapDisplayNode = volumeNode.GetDisplayNode()
    if colorNode != None:
      labelmapDisplayNode.SetAndObserveColorNodeID(colorNode.GetID())
      print '>>>>>%s'% colorNode.GetID()
    volumeNode.EndModify(wasModifying)

    # We can't just use a regular qt signal/slot connection because the input
    # node might not be a labelmap at the time it becomes current, which would
    # not show up in the combobox.
    self.get('MergeLabelsInputNodeComboBox').setCurrentNode(volumeNode)
    self.setupMergeLabels(volumeNode)
    self.get('PoseLabelmapInputNodeComboBox').setCurrentNode(volumeNode)

  def openLabelmapModule(self):
    self.openModule('Volumes')

  #    b) Transform
  def runLPSRASTransform(self):
    self.runTransform((-1.0, 0.0, 0.0, 0.0,
                        0.0, -1.0, 0.0, 0.0,
                        0.0, 0.0, 1.0, 0.0,
                        0.0, 0.0, 0.0, 1.0))

  def runLRTransform(self):
    self.runTransform((-1.0, 0.0, 0.0, 0.0,
                        0.0, 1.0, 0.0, 0.0,
                        0.0, 0.0, 1.0, 0.0,
                        0.0, 0.0, 0.0, 1.0))

  def runPATransform(self):
    self.runTransform((1.0, 0.0, 0.0, 0.0,
                       0.0, -1.0, 0.0, 0.0,
                       0.0, 0.0, 1.0, 0.0,
                       0.0, 0.0, 0.0, 1.0))

  def runSITransform(self):
    self.runTransform((1.0, 0.0, 0.0, 0.0,
                       0.0, 1.0, 0.0, 0.0,
                       0.0, 0.0, -1.0, 0.0,
                       0.0, 0.0, 0.0, 1.0))

  def runTransform(self, matrix):
    volumeNode = self.get('LabelmapVolumeNodeComboBox').currentNode()
    if volumeNode == None:
      return

    transform = vtk.vtkMatrix4x4()
    transform.DeepCopy(matrix)

    volumeNode.ApplyTransformMatrix(transform)
    volumeNode.Modified()

  def runCenter(self):
    volumeNode = self.get('LabelmapVolumeNodeComboBox').currentNode()
    volumesLogic = slicer.modules.volumes.logic()
    if volumesLogic != None and volumeNode != None:
      volumesLogic.CenterVolume(volumeNode)
    # need to refresh the views
    self.reset3DViews()
    self.resetSliceViews()

  #----------------------------------------------------------------------------
  # 2) Model Maker
  #----------------------------------------------------------------------------
  def openExtractPage(self):
    if self.get('BoneModelMakerOutputNodeComboBox').currentNode() == None:
      self.get('BoneModelMakerOutputNodeComboBox').addNode()
    if self.get('SkinModelMakerOutputNodeComboBox').currentNode() == None:
      self.get('SkinModelMakerOutputNodeComboBox').addNode()

  #----------------------------------------------------------------------------
  #    a) Merge Labels
  def updateMergeLabels(self, node, event):
    volumeNode = self.get('MergeLabelsInputNodeComboBox').currentNode()
    if volumeNode == None or (node.IsA('vtkMRMLScalarVolumeNode') and node != volumeNode):
      return
    elif node.IsA('vtkMRMLVolumeDisplayNode'):
      if node != volumeNode.GetDisplayNode():
        return
    self.setupMergeLabels(volumeNode)

  def setupMergeLabels(self, volumeNode):
    if volumeNode == None or not volumeNode.GetLabelMap():
      return
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
      boneLabels.update(self.searchLabels(colorNode, 'mandible'))
      self.get('BoneLabelsLineEdit').setText(', '.join(str( val ) for val in boneLabels.keys()))
      boneLabel = self.bestLabel(boneLabels, ['bone', 'cancellous'])
      self.get('BoneLabelComboBox').setCurrentColor(boneLabel)
      skinLabels = self.searchLabels(colorNode, 'skin')
      self.get('SkinLabelsLineEdit').setText(', '.join(str(val) for val in skinLabels.keys()))
      skinLabel = self.bestLabel(skinLabels, ['skin'])
      self.get('SkinLabelComboBox').setCurrentColor(skinLabel)

      self.createMergeLabelsOutput(volumeNode)
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

  def createMergeLabelsOutput(self, node):
    """ Make sure the output scalar volume node is a node with a -posed suffix.
        Note that the merged volume is used only by the model makers. This
        Should not be used by the PoseLabelmap filter.
    """
    if node == None:
      return
    self.get('MergeLabelsOutputNodeToolButton').enabled = False
    # Don't create the node if the name already contains "merged"
    if node.GetName().lower().find('merged') != -1:
      return
    nodeName = '%s-merged' % node.GetName()
    # make sure such node does not already exist.
    mergedNode = self.getFirstNodeByNameAndClass(nodeName, 'vtkMRMLScalarVolumeNode')
    if mergedNode == None:
      newNode = self.get('MergeLabelsOutputNodeComboBox').addNode()
      newNode.SetName(nodeName)
    else:
      self.get('MergeLabelsOutputNodeComboBox').setCurrentNode(mergedNode)

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

  def runMergeLabels(self, run):
    if run:
      cliNode = self.getCLINode(slicer.modules.changelabel)
      parameters = self.mergeLabelsParameters()
      self.get('MergeLabelsApplyPushButton').setChecked(True)
      self.observeCLINode(cliNode, self.onMergeLabelsCLIModified)
      cliNode = slicer.cli.run(slicer.modules.changelabel, cliNode, parameters, wait_for_completion = False)
    else:
      cliNode = self.observer(self.StatusModifiedEvent, self.onMergeLabelsCLIModified)
      self.get('MergeLabelsApplyPushButton').setEnabled(False)
      cliNode.Cancel()

  def onMergeLabelsCLIModified(self, cliNode, event):
    if cliNode.GetStatusString() == 'Completed':
      # apply label map
      newNode = self.get('MergeLabelsOutputNodeComboBox').currentNode()
      if newNode != None:
        displayNode = newNode.GetDisplayNode()
        if displayNode == None:
          volumesLogic = slicer.modules.volumes.logic()
          volumesLogic.SetVolumeAsLabelMap(newNode, 1)
          displayNode = newNode.GetDisplayNode()
        colorNode = self.get('LabelmapColorNodeComboBox').currentNode()
        if displayNode != None and colorNode != None:
          displayNode.SetAndObserveColorNodeID(colorNode.GetID())
      self.get('MergeLabelsOutputNodeToolButton').enabled = True

    if not cliNode.IsBusy():
      self.get('MergeLabelsApplyPushButton').setChecked(False)
      self.get('MergeLabelsApplyPushButton').setEnabled(True)
      print 'MergeLabels %s' % cliNode.GetStatusString()
      self.removeObservers(self.onMergeLabelsCLIModified)

  def saveMergeLabelsVolumeNode(self):
    self.saveFile('Merged label volume', 'VolumeFile', '.mha', self.get('MergeLabelsOutputNodeComboBox'))

  def openMergeLabelsModule(self):
    self.openModule('ChangeLabel')

    cliNode = self.getCLINode(slicer.modules.changelabel)
    parameters = self.mergeLabelsParameters()
    slicer.cli.setNodeParameters(cliNode, parameters)

  #----------------------------------------------------------------------------
  #     b) Bone Model Maker
  def setupBoneModelMakerLabels(self):
    """ Update the labels of the bone model maker
    """
    labels = []
    labels.append(self.get('BoneLabelComboBox').currentColor)
    self.get('BoneModelMakerLabelsLineEdit').setText(', '.join(str(val) for val in labels))
    self.get('BoneModelMakerOutputNodeToolButton').enabled = False

  def boneModelFromModelHierarchyNode(self, modelHierarchyNode):
    models = vtk.vtkCollection()
    modelHierarchyNode.GetChildrenModelNodes(models)
    return models.GetItemAsObject(0)

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
    parameters["Decimate"] = self.get('BoneModelMakerDecimateSliderWidget').value
    parameters["Smooth"] = 10
    return parameters

  def runBoneModelMaker(self, run):
    if run:
      cliNode = self.getCLINode(slicer.modules.modelmaker)
      parameters = self.boneModelMakerParameters()
      self.get('BoneModelMakerApplyPushButton').setChecked(True)
      self.observeCLINode(cliNode, self.onBoneModelMakerCLIModified)
      cliNode = slicer.cli.run(slicer.modules.modelmaker, cliNode, parameters, wait_for_completion = False)
    else:
      cliNode = self.observer(self.StatusModifiedEvent, self.onBoneModelMakerCLIModified)
      self.get('BoneModelMakerApplyPushButton').setEnabled(False)
      cliNode.Cancel()

  def onBoneModelMakerCLIModified(self, cliNode, event):
    if cliNode.GetStatusString() == 'Completed':
      self.reset3DViews()
      modelNode = self.boneModelFromModelHierarchyNode(self.get('BoneModelMakerOutputNodeComboBox').currentNode())
      self.get('EvalSurfaceWeightInputNodeComboBox').setCurrentNode(modelNode)
      self.get('BoneModelMakerOutputNodeToolButton').enabled = True
    if not cliNode.IsBusy():
      self.get('BoneModelMakerApplyPushButton').setChecked(False)
      self.get('BoneModelMakerApplyPushButton').setEnabled(True)
      print 'Bone ModelMaker %s' % cliNode.GetStatusString()
      self.removeObservers(self.onBoneModelMakerCLIModified)

  def saveBoneModelMakerModelNode(self):
    modelNode = self.boneModelFromModelHierarchyNode(self.get('BoneModelMakerOutputNodeComboBox').currentNode())
    self.saveNode('Bone model', 'ModelFile', '.vtk', modelNode)

  def openModelsModule(self):
    self.openModule('Models')

  def openBoneModelMakerModule(self):
    self.openModule('ModelMaker')

    cliNode = self.getCLINode(slicer.modules.modelmaker)
    parameters = self.boneModelMakerParameters()
    slicer.cli.setNodeParameters(cliNode, parameters)

  #----------------------------------------------------------------------------
  #     c) Skin Model Maker
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
    self.get('SkinModelMakerOutputNodeToolButton').enabled = False
    self.get('SkinModelMakerToggleVisiblePushButton').enabled = False
    self.get('ArmaturesToggleVisiblePushButton').enabled = False

  def skinModelMakerParameters(self):
    parameters = {}
    parameters["InputVolume"] = self.get('SkinModelMakerInputNodeComboBox').currentNode()
    parameters["OutputGeometry"] = self.get('SkinModelMakerOutputNodeComboBox').currentNode()
    parameters["Threshold"] = self.get('SkinModelMakerThresholdSpinBox').value + 0.1
    #parameters["SplitNormals"] = True
    #parameters["PointNormals"] = True
    #parameters["Decimate"] = 0.25
    parameters["Smooth"] = 10
    parameters["Name"] = 'Skin'
    return parameters

  def runSkinModelMaker(self, run):
    if run:
      cliNode = self.getCLINode(slicer.modules.grayscalemodelmaker)
      parameters = self.skinModelMakerParameters()
      self.get('SkinModelMakerApplyPushButton').setChecked(True)
      self.observeCLINode(cliNode, self.onSkinModelMakerCLIModified)
      cliNode = slicer.cli.run(slicer.modules.grayscalemodelmaker, cliNode, parameters, wait_for_completion = False)
    else:
      cliNode = self.observer(self.StatusModifiedEvent, self.onSkinModelMakerCLIModified)
      self.get('SkinModelMakerApplyPushButton').setEnabled(False)
      cliNode.Cancel()

  def onSkinModelMakerCLIModified(self, cliNode, event):
    if cliNode.GetStatusString() == 'Completed':
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
      self.reset3DViews()

      # Enable saving
      self.get('SkinModelMakerOutputNodeToolButton').enabled = True
      self.get('SkinModelMakerToggleVisiblePushButton').enabled = True
      self.get('ArmaturesToggleVisiblePushButton').enabled = True

    if not cliNode.IsBusy():
      self.get('SkinModelMakerApplyPushButton').setChecked(False)
      self.get('SkinModelMakerApplyPushButton').setEnabled(True)
      print 'Skin ModelMaker %s' % cliNode.GetStatusString()
      self.removeObservers(self.onSkinModelMakerCLIModified)

  def saveSkinModelMakerModelNode(self):
    self.saveFile('Skin model', 'ModelFile', '.vtk', self.get('SkinModelMakerOutputNodeComboBox'))

  def openSkinModelMakerModule(self):
    self.openModule('GrayscaleModelMaker')

    cliNode = self.getCLINode(slicer.modules.grayscalemodelmaker)
    parameters = self.skinModelMakerParameters()
    slicer.cli.setNodeParameters(cliNode, parameters)

  def updateSkinNodeVisibility(self):
    skinModel = self.get('SkinModelMakerOutputNodeComboBox').currentNode()
    if skinModel != None:
      skinModel.SetDisplayVisibility(not skinModel.GetDisplayVisibility())

  #----------------------------------------------------------------------------
  # 3) Rigging
  #----------------------------------------------------------------------------
  def openCreateArmaturePage(self):
    self.reset3DViews()
    # Switch to 3D View only
    manager = slicer.app.layoutManager()
    manager.setLayout(slicer.vtkMRMLLayoutNode.SlicerLayoutOneUp3DView)

  #----------------------------------------------------------------------------
  # 3.A) Armature
  def setCurrentArmatureModelNode(self, armatureNode):
    if armatureNode == None:
      return
    modelNode = armatureNode.GetAssociatedNode()
    self.get('VolumeSkinningAmartureNodeComboBox').setCurrentNode(modelNode)

  def loadArmatureNode(self):
    self.loadFile('Armature', 'ArmatureFile', self.get('ArmaturesArmatureNodeComboBox'))

  def saveArmatureNode(self):
    armatureNode = self.get('ArmaturesArmatureNodeComboBox').currentNode()
    modelNode = armatureNode.GetAssociatedNode()
    self.saveNode('Armature', 'ModelFile', '.vtk', modelNode)

  def openArmaturesModule(self):
    # Finaly open armature module
    self.openModule('Armatures')

  #----------------------------------------------------------------------------
  # 4) Skinning
  #----------------------------------------------------------------------------
  def openSkinningPage(self):
    # Switch to FourUp
    slicer.app.layoutManager().setLayout(slicer.vtkMRMLLayoutNode.SlicerLayoutFourUpView)

    # Create output if necessary
    if not self.volumeSkinningCreateOutputConnected:
      self.get('VolumeSkinningInputVolumeNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.createOutputSkinnedVolume)
      self.volumeSkinningCreateOutputConnected = True
    self.createOutputSkinnedVolume(self.get('VolumeSkinningInputVolumeNodeComboBox').currentNode())

  #----------------------------------------------------------------------------
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

  def runVolumeSkinning(self, run):
    if run:
      cliNode = self.getCLINode(slicer.modules.volumeskinning)
      parameters = self.volumeSkinningParameters()
      self.get('VolumeSkinningApplyPushButton').setChecked(True)
      self.observeCLINode(cliNode, self.onVolumeSkinningCLIModified)
      cliNode = slicer.cli.run(slicer.modules.volumeskinning, cliNode, parameters, wait_for_completion = False)
    else:
      cliNode = self.observer(self.StatusModifiedEvent, self.onVolumeSkinningCLIModified)
      self.get('VolumeSkinningApplyPushButton').setEnabled(False)
      cliNode.Cancel()

  def onVolumeSkinningCLIModified(self, cliNode, event):
    if cliNode.GetStatusString() == 'Completed':
      self.get('VolumeSkinningOutputVolumeNodeToolButton').enabled = True

    if not cliNode.IsBusy():
      self.get('VolumeSkinningApplyPushButton').setChecked(False)
      self.get('VolumeSkinningApplyPushButton').setEnabled(True)
      print 'VolumeSkinning %s' % cliNode.GetStatusString()
      self.removeObservers(self.onVolumeSkinningCLIModified)

  def loadSkinningInputVolumeNode(self):
    self.loadLabelmapFile('Input volume', 'VolumeFile', self.get('VolumeSkinningInputVolumeNodeComboBox'))

  def saveSkinningVolumeNode(self):
    self.saveFile('Skinned volume', 'VolumeFile', '.mha', self.get('VolumeSkinningOutputVolumeNodeComboBox'))

  def openVolumeSkinningModule(self):
    self.openModule('VolumeSkinning')

    cliNode = self.getCLINode(slicer.modules.volumeskinning)
    parameters = self.volumeSkinningParameters()
    slicer.cli.setNodeParameters(cliNode, parameters)

  def createOutputSkinnedVolume(self, node):
    if node == None:
      return
    self.get('VolumeSkinningOutputVolumeNodeToolButton').enabled = False

    nodeName = '%s-skinned' % node.GetName()
    if self.getFirstNodeByNameAndClass(nodeName, 'vtkMRMLScalarVolumeNode') == None:
      newNode = self.get('VolumeSkinningOutputVolumeNodeComboBox').addNode()
      newNode.SetName(nodeName)

  #----------------------------------------------------------------------------
  #    b) Edit skinned volume
  def editSkinnedVolumeParameterChanged(self, skinnedVolume = None, event = None):
    canEdit = False
    canSave = False
    self.removeObservers(self.editSkinnedVolumeParameterChanged)
    if skinnedVolume != None:
      canEdit = skinnedVolume.GetDisplayNode() != None
      canSave = canEdit and skinnedVolume.GetModifiedSinceRead()
      self.addObserver(skinnedVolume, 'ModifiedEvent', self.editSkinnedVolumeParameterChanged)
    self.get('EditSkinnedVolumeGoToEditorPushButton').enabled = canEdit
    self.get('EditSkinnedVolumeNodeSaveToolButton').enabled = canSave

  def loadEditSkinnedVolumeNode(self):
    self.loadLabelmapFile('Skinning volume', 'VolumeFile', self.get('EditSkinnedVolumeNodeComboBox'))

  def saveEditSkinnedVolumeNode(self):
    self.saveFile('Skinned volume', 'VolumeFile', '.mha', self.get('EditSkinnedVolumeNodeComboBox'))

  def openEditorModule(self):
    self.removeObservers(self.editSkinnedVolumeParameterChanged)
    self.openModule('Editor')
    editorWidget = slicer.modules.editor.widgetRepresentation()
    masterVolumeNodeComboBox = editorWidget.findChild('qMRMLNodeComboBox')
    masterVolumeNodeComboBox.addAttribute('vtkMRMLScalarVolumeNode', 'LabelMap', 1)
    masterVolumeNodeComboBox.setCurrentNode(self.get('EditSkinnedVolumeNodeComboBox').currentNode())
    setButton = editorWidget.findChild('QPushButton')
    setButton.click()

  #----------------------------------------------------------------------------
  # 5) Weights
  #----------------------------------------------------------------------------
  def openWeightsPage(self):
    pass
  #----------------------------------------------------------------------------
  #    a) Compute Armature Weight
  def computeArmatureWeightParameters(self):
    parameters = {}
    parameters["RestLabelmap"] = self.get('ComputeArmatureWeightInputVolumeNodeComboBox').currentNode()
    parameters["ArmaturePoly"] = self.get('ComputeArmatureWeightAmartureNodeComboBox').currentNode()
    parameters["SkinnedVolume"] = self.get('ComputeArmatureWeightSkinnedVolumeVolumeNodeComboBox').currentNode()
    parameters["WeightDirectory"] = str(self.get('ComputeArmatureWeightOutputPathLineEdit').currentPath)
    parameters["Padding"] = self.get('ComputeArmatureWeightPaddingSpinBox').value
    parameters["ScaleFactor"] = self.get('ComputeArmatureWeightScaleFactorSpinBox').value
    parameters["MaximumParenthoodDistance"] = '4'
    #parameters["FirstEdge"] = '0'
    #parameters["LastEdge"] = '-1'
    #parameters["BinaryWeight"] = False
    #parameters["SmoothingIteration"] = '10'
    #parameters["Debug"] = False
    #parameters["RunSequential"] = False
    return parameters

  def runComputeArmatureWeight(self, run):
    if run:
      cliNode = self.getCLINode(slicer.modules.computearmatureweight)
      parameters = self.computeArmatureWeightParameters()
      self.get('ComputeArmatureWeightApplyPushButton').setChecked(True)
      self.observeCLINode(cliNode, self.onComputeArmatureWeightCLIModified)
      cliNode = slicer.cli.run(slicer.modules.computearmatureweight, cliNode, parameters, wait_for_completion = False)
    else:
      cliNode = self.observer(self.StatusModifiedEvent, self.onComputeArmatureWeightCLIModified)
      self.get('ComputeArmatureWeightApplyPushButton').setEnabled(False)
      cliNode.Cancel()

  def onComputeArmatureWeightCLIModified(self, cliNode, event):
    if not cliNode.IsBusy():
      self.get('ComputeArmatureWeightApplyPushButton').setChecked(False)
      self.get('ComputeArmatureWeightApplyPushButton').setEnabled(True)
      print 'ComputeArmatureWeight %s' % cliNode.GetStatusString()
      self.removeObservers(self.onComputeArmatureWeightCLIModified)

  def openComputeArmatureWeightModule(self):
    self.openModule('ComputeArmatureWeight')

    cliNode = self.getCLINode(slicer.modules.computearmatureweight)
    parameters = self.computeArmatureWeightParameters()
    slicer.cli.setNodeParameters(cliNode, parameters)

  #----------------------------------------------------------------------------
  # c) Eval Weight
  def evalSurfaceWeightParameterChanged(self):
    self.get('EvalSurfaceWeightOutputNodeToolButton').enabled = False

  def evalSurfaceWeightParameters(self):
    parameters = {}
    parameters["InputSurface"] = self.get('EvalSurfaceWeightInputNodeComboBox').currentNode()
    parameters["OutputSurface"] = self.get('EvalSurfaceWeightOutputNodeComboBox').currentNode()
    parameters["WeightDirectory"] = str(self.get('EvalSurfaceWeightWeightPathLineEdit').currentPath)
    #parameters["IsSurfaceInRAS"] = False
    #parameters["PrintDebug"] = False
    return parameters

  def runEvalSurfaceWeight(self, run):
    if run:
      cliNode = self.getCLINode(slicer.modules.evalsurfaceweight)
      parameters = self.evalSurfaceWeightParameters()
      self.get('EvalSurfaceWeightApplyPushButton').setChecked(True)
      self.observeCLINode(cliNode, self.onEvalSurfaceWeightCLIModified)
      cliNode = slicer.cli.run(slicer.modules.evalsurfaceweight, cliNode, parameters, wait_for_completion = False)
    else:
      cliNode = self.observer(self.StatusModifiedEvent, self.onEvalSurfaceWeightCLIModified)
      self.get('EvalSurfaceWeightApplyPushButton').setEnabled(False)
      cliNode.Cancel()

  def onEvalSurfaceWeightCLIModified(self, cliNode, event):
    if cliNode.GetStatusString() == 'Completed':
      self.get('EvalSurfaceWeightOutputNodeToolButton').enabled = True

    if not cliNode.IsBusy():
      self.get('EvalSurfaceWeightApplyPushButton').setChecked(False)
      self.get('EvalSurfaceWeightApplyPushButton').setEnabled(True)
      print 'EvalSurfaceWeight %s' % cliNode.GetStatusString()
      self.removeObservers(self.onEvalSurfaceWeightCLIModified)

  def loadEvalSurfaceWeightInputNode(self):
    self.loadFile('Model to eval', 'ModelFile', self.get('EvalSurfaceWeightInputNodeComboBox'))

  def saveEvalSurfaceWeightOutputNode(self):
    self.saveFile('Evaluated Model', 'ModelFile', '.vtk', self.get('EvalSurfaceWeightOutputNodeComboBox'))

  def openEvalSurfaceWeight(self):
    self.openModule('EvalSurfaceWeight')

    cliNode = self.getCLINode(slicer.modules.evalweight)
    parameters = self.evalWeightParameters()
    slicer.cli.setNodeParameters(cliNode, parameters)

  #----------------------------------------------------------------------------
  # 6) Pose Armature & Pose surface
  #----------------------------------------------------------------------------
  def openPoseArmaturePage(self):
    armatureLogic = slicer.modules.armatures.logic()
    if armatureLogic != None:
      armatureLogic.SetActiveArmatureWidgetState(3) # 3 is Pose

    # Create output if necessary
    if not self.poseSurfaceCreateOutputConnected:
      self.get('PoseSurfaceInputNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.createOutputPoseSurface)
      self.poseSurfaceCreateOutputConnected = True
    self.createOutputPoseSurface(self.get('PoseSurfaceInputNodeComboBox').currentNode())

    self.autoRunPoseSurface(self.get('PoseSurfaceApplyPushButton').checkState)

  #----------------------------------------------------------------------------
  # a) Pose Armature
  def setPoseArmatureModelNode(self, armatureNode):
    if armatureNode == None:
      return
    modelNode = armatureNode.GetAssociatedNode()
    self.get('PoseSurfaceArmatureInputNodeComboBox').setCurrentNode(modelNode)

    armatureLogic = slicer.modules.armatures.logic()
    if armatureLogic != None and self.WorkflowWidget.currentIndex == 4:
      armatureLogic.SetActiveArmature(armatureNode)
      armatureLogic.SetActiveArmatureWidgetState(3) # 3 is Pose

  def savePoseArmatureArmatureNode(self):
    armatureNode = self.get('PoseArmatureArmatureNodeComboBox').currentNode()
    modelNode = armatureNode.GetAssociatedNode()
    self.saveNode('Armature', 'ModelFile', '.vtk', modelNode)

  def openPosedArmatureModule(self):
    self.openModule('Armatures')

  #----------------------------------------------------------------------------
  # b) Pose Surface
  def poseSurfaceParameterChanged(self):
    self.get('PoseSurfaceOutputNodeToolButton').enabled = False

    cliNode = self.getCLINode(slicer.modules.posesurface)
    parameters = self.poseSurfaceParameters()
    slicer.cli.setNodeParameters(cliNode, parameters)

  def poseSurfaceParameters(self):
    # Setup CLI node on input changed or apply changed
    parameters = {}
    parameters["ArmaturePoly"] = self.get('PoseSurfaceArmatureInputNodeComboBox').currentNode()
    parameters["SurfaceInput"] = self.get('PoseSurfaceInputNodeComboBox').currentNode()
    parameters["WeightDirectory"] = str(self.get('PoseSurfaceWeightInputPathLineEdit').currentPath)
    parameters["OutputSurface"] = self.get('PoseSurfaceOutputNodeComboBox').currentNode()
    parameters["MaximumParenthoodDistance"] = '4'
    #parameters["IsSurfaceInRAS"] = False
    #parameters["IsArmatureInRAS"] = False
    parameters["LinearBlend"] = True # much faster
    return parameters

  def autoRunPoseSurface(self, autoRun):
    if autoRun:
      cliNode = self.getCLINode(slicer.modules.posesurface)
      parameters = self.poseSurfaceParameters()
      slicer.cli.setNodeParameters(cliNode, parameters)
      cliNode.SetAutoRunMode(cliNode.AutoRunOnAnyInputEvent)
      cliNode.SetAutoRun(autoRun)
      self.observeCLINode(cliNode)
    else:
      cliNode = self.getCLINode(slicer.modules.posesurface)
      cliNode.SetAutoRun(autoRun)

  def runPoseSurface(self, run):
    if run:
      cliNode = self.getCLINode(slicer.modules.posesurface)
      parameters = self.poseSurfaceParameters()
      slicer.cli.setNodeParameters(cliNode, parameters)
      cliNode.SetAutoRunMode(cliNode.AutoRunOnAnyInputEvent)
      self.get('PoseSurfaceApplyPushButton').setChecked(True)
      self.observeCLINode(cliNode, self.onPoseSurfaceCLIModified)
      cliNode = slicer.cli.run(slicer.modules.poselabelmap, cliNode, parameters, wait_for_completion = False)
    else:
      cliNode = self.observer(self.StatusModifiedEvent, self.onPoseSurfaceCLIModified)
      self.get('PoseSurfaceApplyPushButton').setEnabled(False)
      cliNode.Cancel()

  def onPoseSurfaceCLIModified(self, cliNode, event):
    if cliNode.GetStatusString() == 'Completed':
      self.get('PoseSurfaceOutputNodeToolButton').enabled = True
      if self.get('PoseSurfaceInputNodeComboBox').currentNode() != self.get('PoseSurfaceOutputNodeComboBox').currentNode():
        self.get('PoseSurfaceInputNodeComboBox').currentNode().GetDisplayNode().SetVisibility(0)
    if not cliNode.IsBusy():
      self.get('PoseSurfaceApplyPushButton').setChecked(False)
      self.get('PoseSurfaceApplyPushButton').setEnabled(True)
      print 'PoseSurface %s' % cliNode.GetStatusString()
      self.removeObservers(self.onPoseSurfaceCLIModified)

  def loadPoseSurfaceInputNode(self):
    self.loadFile('Model to pose', 'ModelFile', self.get('PoseSurfaceInputNodeComboBox'))

  def savePoseSurfaceOutputNode(self):
    self.saveFile('Posed model', 'ModelFile', '.vtk', self.get('PoseSurfaceOutputNodeComboBox'))

  def openPoseSurfaceModule(self):
    self.openModule('PoseSurface')

    cliNode = self.getCLINode(slicer.modules.posesurface)
    parameters = self.poseSurfaceParameterss()
    slicer.cli.setNodeParameters(cliNode, parameters)

  def setWeightDirectory(self, dir):
    self.get('EvalSurfaceWeightWeightPathLineEdit').currentPath = dir
    self.get('PoseSurfaceWeightInputPathLineEdit').currentPath = dir
    self.get('PoseLabelmapWeightPathLineEdit').currentPath = dir

  def createOutputPoseSurface(self, node):
    if node == None:
      return

    nodeName = '%s-posed' % node.GetName()
    if self.getFirstNodeByNameAndClass(nodeName, 'vtkMRMLModelNode') == None:
      newNode = self.get('PoseSurfaceOutputNodeComboBox').addNode()
      newNode.SetName(nodeName)

  #----------------------------------------------------------------------------
  # 7) Resample NOTE: SHOULD BE LAST STEP
  #----------------------------------------------------------------------------
  def openPoseLabelmapPage(self):
    # Create output if necessary
    if not self.poseLabelmapCreateOutputConnected:
      self.get('PoseLabelmapInputNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.createOutputPoseLabelmap)
      self.poseLabelmapCreateOutputConnected = True
    self.createOutputPoseLabelmap(self.get('PoseLabelmapInputNodeComboBox').currentNode())

  def poseLabelmapParameterChanged(self):
    self.get('PoseLabelmapOutputNodeToolButton').enabled = False

  def poseLabelmapParameters(self):
    parameters = {}
    parameters["RestLabelmap"] = self.get('PoseLabelmapInputNodeComboBox').currentNode()
    parameters["ArmaturePoly"] = self.get('PoseLabelmapArmatureNodeComboBox').currentNode()
    parameters["WeightDirectory"] = str(self.get('PoseLabelmapWeightPathLineEdit').currentPath)
    parameters["PosedLabelmap"] = self.get('PoseLabelmapOutputNodeComboBox').currentNode()
    parameters["LinearBlend"] = True
    parameters["MaximumParenthoodDistance"] = '4'
    #parameters["MaximumRadius"] = '64'
    #parameters["Debug"] = False
    #parameters["IsArmatureInRAS"] = False
    return parameters

  def runPoseLabelmap(self, run):
    if run:
      cliNode = self.getCLINode(slicer.modules.poselabelmap)
      parameters = self.poseLabelmapParameters()
      self.get('PoseLabelmapApplyPushButton').setChecked(True)
      self.observeCLINode(cliNode, self.onPoseLabelmapCLIModified)
      cliNode = slicer.cli.run(slicer.modules.poselabelmap, cliNode, parameters, wait_for_completion = False)
    else:
      cliNode = self.observer(self.StatusModifiedEvent, self.onPoseLabelmapCLIModified())
      self.get('PoseLabelmapApplyPushButton').setEnabled(False)
      cliNode.Cancel()

  def onPoseLabelmapCLIModified(self, cliNode, event):
    if cliNode.GetStatusString() == 'Completed':
      self.get('PoseLabelmapOutputNodeComboBox').enabled = True
    if not cliNode.IsBusy():
      self.get('PoseLabelmapApplyPushButton').setChecked(False)
      self.get('PoseLabelmapApplyPushButton').setEnabled(True)
      print 'PoseLabelmap %s' % cliNode.GetStatusString()
      self.removeObservers(self.onPoseLabelmapCLIModified)

  def savePoseLabelmapOutputNode(self):
    self.saveFile('Posed labelmap', 'VolumeFile', '.mha', self.get('PoseLabelmapOutputNodeComboBox'))

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

  def observer(self, event, method):
    for o, e, m, g, t in self.Observations:
      if e == event and m == method:
        return o
    return None

  def getCLINode(self, cliModule):
    """ Return the cli node to use for a given CLI module. Create the node in
    scene if needed.
    """
    cliNode = slicer.mrmlScene.GetFirstNodeByName(cliModule.title)
    if cliNode == None:
      cliNode = slicer.cli.createNode(cliModule)
      cliNode.SetName(cliModule.title)
    return cliNode

  def loadLabelmapFile(self, title, fileType, nodeComboBox):
    volumeNode = self.loadFile(title, fileType, nodeComboBox)
    if volumeNode != None:
      volumesLogic = slicer.modules.volumes.logic()
      volumesLogic.SetVolumeAsLabelMap(volumeNode, 1)
      nodeComboBox.setCurrentNode(volumeNode)

  def loadFile(self, title, fileType, nodeComboBox):
    manager = slicer.app.ioManager()
    loadedNodes = vtk.vtkCollection()
    properties = {}
    res = manager.openDialog(fileType, slicer.qSlicerFileDialog.Read, properties, loadedNodes)
    loadedNode = loadedNodes.GetItemAsObject(0)
    if res == True:
      nodeComboBox.setCurrentNode(loadedNode)
    self.reset3DViews()
    return loadedNode

  def saveFile(self, title, fileType, fileSuffix, nodeComboBox):
    self.saveNode(title, fileType, fileSuffix, nodeComboBox.currentNode())

  def saveNode(self, title, fileType, fileSuffix, node):
    manager = slicer.app.ioManager()
    properties = {}
    properties['nodeID'] = node.GetID()
    properties['defaultFileName'] = node.GetName() + fileSuffix
    manager.openDialog(fileType, slicer.qSlicerFileDialog.Write, properties)

  def reset3DViews(self):
    # Reset focal view around volumes
    manager = slicer.app.layoutManager()
    for i in range(0, manager.threeDViewCount):
      manager.threeDWidget(i).threeDView().resetFocalPoint()
      rendererCollection = manager.threeDWidget(i).threeDView().renderWindow().GetRenderers()
      for i in range(0, rendererCollection.GetNumberOfItems()):
        rendererCollection.GetItemAsObject(i).ResetCamera()

  def resetSliceViews(self):
    # Reset focal view around volumes
    manager = slicer.app.layoutManager()
    for i in manager.sliceViewNames():
      manager.sliceWidget(i).sliceController().fitSliceToBackground()

  def openModule(self, moduleName):
    slicer.util.selectModule(moduleName)

  def getFirstNodeByNameAndClass(self, name, className):
    nodes = slicer.mrmlScene.GetNodesByClass(className)
    nodes.UnRegister(nodes)
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
