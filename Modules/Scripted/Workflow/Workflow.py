# Regular Scripted module import
from __main__ import vtk, qt, ctk, slicer

# Other scripted modules import
import SkinModelMaker

#
# Workflow
#

class Workflow:
  def __init__(self, parent):
    import string
    parent.title = "Bender FEM Workflow"
    parent.categories = ["", "Segmentation.Bender"]
    parent.contributors = ["Julien Finet (Kitware), Johan Andruejol (Kitware)"]
    parent.helpText = string.Template("""
Step by step workflow to reposition a labelmap. See <a href=\"$a/Documentation/$b.$c/Modules/Workflow\">$a/Documentation/$b.$c/Modules/Workflow</a> for more information.
    """).substitute({ 'a':'http://public.kitware.com/Wiki/Bender', 'b':1, 'c':1 })
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

    self.IsSetup = True
    self.Observations = []

    import imp, sys, os, slicer, qt
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

    self.WorkflowWidget = self.get('WorkflowWidget')
    self.TitleLabel = self.get('TitleLabel')

    # Global variables
    self.StatusModifiedEvent = slicer.vtkMRMLCommandLineModuleNode().StatusModifiedEvent

    # Labelmap variables

    # Extract variables
    # List here the equivalent labels to the structures. For example, an equivalent
    # label to bone would be mandible.
    self.equivalentLabelStructures = {
      'bone' :
        [
        'bone',
        'vertebr',
        'mandible',
        'cartilage',
        ],
      'bonebestlabelbestlabel' : 'cancellous',
      'skin' :
        [
        'skin',
        ],
      'background' :
        [
        'none',
        'air',
        ],
      'muscle' :
        [
        'muscle',
        ],
      }

    # Compute Weight variables
    self.volumeSkinningCreateOutputConnected = False

    # Pose surface variables
    self.simulatePoseCreateOutputConnected = False

    self.pages = { 0 : 'Adjust',
                   1 : 'Extract',
                   2 : 'Mesh',
                   3 : 'Armature',
                   4 : 'Skinning',
                   5 : 'Weights',
                   6 : 'PoseArmature',
                   }

    # Load/Save icons
    loadIcon = self.WorkflowWidget.style().standardIcon(qt.QStyle.SP_DialogOpenButton)
    saveIcon = self.WorkflowWidget.style().standardIcon(qt.QStyle.SP_DialogSaveButton)

    # --------------------------------------------------------------------------
    # Connections
    # 0) Workflow
    self.get('NextPageToolButton').connect('clicked()', self.goToNext)
    self.get('PreviousPageToolButton').connect('clicked()', self.goToPrevious)
    #  a) Settings
    self.get('SettingsWorkflowComboBox').connect('currentIndexChanged(int)', self.setupWorkflow)
    self.get('SettingsReloadPushButton').connect('clicked()', self.reloadModule)
    #  b) Data
    self.get('VisibleNodesComboBox').connect('checkedNodesChanged()', self.setNodesVisibility)
    self.get('VisibleNodesComboBox').connect('nodeAdded(vtkMRMLNode*)', self.onNodeAdded)
    #  c) Volume Render
    self.get('LabelsTableWidget').connect('itemChanged(QTableWidgetItem*)', self.setupVolumeRenderLabels)
    self.get('VolumeRenderInputNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.setupVolumeRender)
    self.get('VolumeRenderLabelsLineEdit').connect('editingFinished()', self.updateVolumeRenderLabels)
    self.get('VolumeRenderCheckBox').connect('toggled(bool)',self.runVolumeRender)
    self.get('VolumeRenderCropCheckBox').connect('toggled(bool)', self.onCropVolumeRender)
    self.get('VolumeRenderGoToModulePushButton').connect('clicked()', self.openVolumeRenderModule)

    # 1) Adjust
    #  a) Labelmap
    #   - Icons
    self.get('LabelmapVolumeNodeToolButton').icon = loadIcon
    self.get('LabelmapColorNodeToolButton').icon = loadIcon
    #   - Signals/Slots
    self.get('LabelmapVolumeNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.setupLabelmap)
    self.get('LabelmapColorNodeComboBox').connect('nodeActivated(vtkMRMLNode*)', self.applyColorNode)
    self.get('LabelmapVolumeNodeToolButton').connect('clicked()', self.loadLabelmapVolumeNode)
    self.get('LabelmapColorNodeToolButton').connect('clicked()', self.loadLabelmapColorNode)
    self.get('LabelMapApplyColorNodePushButton').connect('clicked()', self.applyColorNode)
    self.get('LabelmapGoToModulePushButton').connect('clicked()', self.openLabelmapModule)
    self.get('LPSRASTransformPushButton').connect('clicked()', self.runLPSRASTransform)

    # 2) Extract
    #  a) Merge Labels
    #    - Icons
    self.get('MergeLabelsOutputNodeToolButton').icon = saveIcon
    self.get('MergeLabelsSaveToolButton').icon = saveIcon
    self.get('MergeLabelsOutputNodeLoadToolButton').icon = loadIcon
    self.get('MergeLabelsLoadToolButton').icon = loadIcon
    #    - Signals/Slots
    self.get('MergeLabelsInputNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.setupMergeLabels)
    self.get('MergeLabelsOutputNodeLoadToolButton').connect('clicked()', self.loadMergeLabelsVolumeNode)
    self.get('MergeLabelsOutputNodeToolButton').connect('clicked()', self.saveMergeLabelsVolumeNode)
    self.get('MergeLabelsApplyPushButton').connect('clicked(bool)', self.runMergeLabels)
    self.get('MergeLabelsGoToModulePushButton').connect('clicked()', self.openMergeLabelsModule)
    #  b) Pad Image
    #    - Icons
    self.get('PadImageOutputNodeToolButton').icon = saveIcon
    self.get('PadImageSaveToolButton').icon = saveIcon
    self.get('PadImageOutputNodeLoadToolButton').icon = loadIcon
    self.get('PadImageLoadToolButton').icon = loadIcon
    #    - Signals/Slots
    self.get('PadImageInputNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.setupPadImage)
    self.get('PadImageOutputNodeLoadToolButton').connect('clicked()', self.loadPadImageVolumeNode)
    self.get('PadImageOutputNodeToolButton').connect('clicked()', self.savePadImageVolumeNode)
    self.get('PadImageApplyPushButton').connect('clicked(bool)', self.runPadImage)
    self.get('PadImageGoToModulePushButton').connect('clicked()', self.openPadImageModule)

    # 3) Mesh
    # a) Tet-Mesh generator
        #    - Icons
    self.get('CreateMeshOutputToolButton').icon = saveIcon
    self.get('CreateMeshToolButton').icon = saveIcon
    self.get('CreateMeshOutputLoadToolButton').icon = loadIcon
    self.get('CreateMeshLoadToolButton').icon = loadIcon
    #    - Signals/Slots
    self.get('CreateMeshOutputLoadToolButton').connect('clicked()', self.loadMeshNode)
    self.get('CreateMeshOutputToolButton').connect('clicked()', self.saveMeshNode)
    self.get('CreateMeshPushButton').connect('clicked(bool)', self.runCreateMesh)
    self.get('CreateMeshInputNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.createMeshOutput)
    self.get('CreateMeshGoToButton').connect('clicked()', self.openCreateMeshModule)
    # b) Bone mesh extractor
        #    - Icons
    self.get('ExtractBoneOutputLoadToolButton').icon = loadIcon
    self.get('ExtractBoneLoadToolButton').icon = loadIcon
    self.get('ExtractBoneOutputToolButton').icon = saveIcon
    self.get('ExtractBoneToolButton').icon = saveIcon
    #    - Signals/Slots
    self.get('ExtractBoneOutputLoadToolButton').connect('clicked()', self.loadExtractBone)
    self.get('ExtractBoneOutputToolButton').connect('clicked()', self.saveExtractBone)
    self.get('ExtractBonePushButton').connect('clicked(bool)', self.runExtractBone)
    self.get('ExtractBoneInputComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.createExtractBoneOutput)
    self.get('ExtractBoneGoToPushButton').connect('clicked()', self.openExtractBoneModule)
    self.get('LabelsTableWidget').connect('itemChanged(QTableWidgetItem*)', self.onBoneMaterialChanged)
    # c) Skin Model Maker
        #    - Icons
    self.get('SkinModelMakerOutputNodeToolButton').icon = saveIcon
    self.get('SkinModelMakerSaveToolButton').icon = saveIcon
    self.get('SkinModelMakerOutputNodeLoadToolButton').icon = loadIcon
    self.get('SkinModelMakerLoadToolButton').icon = loadIcon
    #    - Signals/Slots
    self.get('SkinModelMakerOutputNodeLoadToolButton').connect('clicked()', self.loadExtractSkin)
    self.get('SkinModelMakerOutputNodeToolButton').connect('clicked()', self.saveExtractSkin)
    self.get('SkinModelMakerApplyPushButton').connect('clicked(bool)', self.runExtractSkin)
    self.get('SkinModelMakerInputNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.createExtractSkinOutput)
    self.get('SkinModelMakerGoToModelsModulePushButton').connect('clicked()', self.openModelsModule)
    self.get('SkinModelMakerGoToModulePushButton').connect('clicked()', self.openExtractSkinModule)
    self.get('LabelsTableWidget').connect('itemChanged(QTableWidgetItem*)', self.onSkinMaterialChanged)
    self.get('LabelsTableWidget').connect('itemChanged(QTableWidgetItem*)', self.onBackgroundMaterialChanged)
    self.get('SkinModelMakerToggleVisiblePushButton').connect('clicked()', self.updateSkinNodeVisibility)

    # 4) Armature
    #    - Icons
    self.get('ArmaturesArmatureLoadToolButton').icon = loadIcon
    self.get('ArmaturesArmatureSaveToolButton').icon = saveIcon
    #    - Signals/Slots
    self.get('ArmaturesPresetComboBox').connect('activated(int)', self.loadArmaturePreset)
    self.get('ArmaturesArmatureNodeComboBox').connect('nodeAdded(vtkMRMLNode*)',self.onArmatureNodeAdded)
    self.get('ArmaturesArmatureNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.setCurrentArmatureModelNode)
    self.get('ArmaturesToggleVisiblePushButton').connect('clicked()', self.updateSkinNodeVisibility)
    self.get('ArmaturesArmatureLoadToolButton').connect('clicked()', self.loadArmatureNode)
    self.get('ArmaturesArmatureSaveToolButton').connect('clicked()', self.saveArmatureNode)
    self.get('ArmaturesGoToPushButton').connect('clicked()', self.openArmaturesModule)

    # 5) Skinning
    # a) Volume Skinning
    #    - Icons
    self.get('VolumeSkinningInputVolumeNodeToolButton').icon = loadIcon
    self.get('VolumeSkinningOutputVolumeNodeLoadToolButton').icon = loadIcon
    self.get('VolumeSkinningOutputVolumeNodeToolButton').icon = saveIcon
    self.get('VolumeSkinningLoadToolButton').icon = loadIcon
    self.get('VolumeSkinningSaveToolButton').icon = saveIcon
    #    - Signals/Slots
    self.get('VolumeSkinningInputVolumeNodeToolButton').connect('clicked()', self.loadSkinningInputVolumeNode)
    self.get('VolumeSkinningOutputVolumeNodeLoadToolButton').connect('clicked()', self.loadSkinningVolumeNode)
    self.get('VolumeSkinningOutputVolumeNodeToolButton').connect('clicked()', self.saveSkinningVolumeNode)
    self.get('VolumeSkinningApplyPushButton').connect('clicked(bool)',self.runVolumeSkinning)
    self.get('VolumeSkinningGoToPushButton').connect('clicked()', self.openVolumeSkinningModule)
    # b) Edit skinned volume
    #    - Icons
    self.get('EditSkinnedVolumeNodeLoadToolButton').icon = loadIcon
    self.get('EditSkinnedVolumeNodeSaveToolButton').icon = saveIcon
    self.get('EditSkinnedVolumeLoadToolButton').icon = loadIcon
    self.get('EditSkinnedVolumeSaveToolButton').icon = saveIcon
    #    - Signals/Slots
    self.get('EditSkinnedVolumeNodeLoadToolButton').connect('clicked()', self.loadEditSkinnedVolumeNode)
    self.get('EditSkinnedVolumeNodeSaveToolButton').connect('clicked()', self.saveEditSkinnedVolumeNode)
    self.get('EditSkinnedVolumeNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.editSkinnedVolumeParameterChanged)
    self.get('EditSkinnedVolumeGoToEditorPushButton').connect('clicked()', self.openEditorModule)
    # 6) Weights
    # a) Armatures Weight
    self.get('ComputeArmatureWeightInputVolumeNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.setDefaultPath)
    self.get('ComputeArmatureWeightScaleFactorSpinBox').connect('valueChanged(double)', self.setDefaultPath)
    self.get('ComputeArmatureWeightApplyPushButton').connect('clicked(bool)',self.runComputeArmatureWeight)
    self.get('ComputeArmatureWeightGoToPushButton').connect('clicked()', self.openComputeArmatureWeightModule)
    self.get('ComputeArmatureWeightOutputPathLineEdit').connect('currentPathChanged(QString)', self.setWeightDirectory)
    # b) Eval Weight
    #    - Icons
    self.get('EvalSurfaceWeightInputNodeToolButton').icon = loadIcon
    self.get('EvalSurfaceWeightOutputNodeLoadToolButton').icon = loadIcon
    self.get('EvalSurfaceWeightOutputNodeToolButton').icon = saveIcon
    self.get('EvalSurfaceWeightLoadToolButton').icon = loadIcon
    self.get('EvalSurfaceWeightSaveToolButton').icon = saveIcon
    #    - Signals/Slots
    self.get('EvalSurfaceWeightInputNodeToolButton').connect('clicked()', self.loadEvalSurfaceWeightInputNode)
    self.get('EvalSurfaceWeightOutputNodeLoadToolButton').connect('clicked()', self.loadEvalSurfaceWeightOutputNode)
    self.get('EvalSurfaceWeightOutputNodeToolButton').connect('clicked()', self.saveEvalSurfaceWeightOutputNode)
    self.get('EvalSurfaceWeightApplyPushButton').connect('clicked(bool)', self.runEvalSurfaceWeight)
    self.get('EvalSurfaceWeightGoToPushButton').connect('clicked()', self.openEvalSurfaceWeight)
    self.get('EvalSurfaceWeightWeightPathLineEdit').connect('currentPathChanged(QString)', self.setWeightDirectory)
    # c) Material properties
    #    - Icons
    self.get('MaterialReaderInputMeshNodeToolButton').icon = loadIcon
    self.get('MaterialReaderOutputMeshNodeLoadToolButton').icon = loadIcon
    self.get('MaterialReaderOutputMeshNodeToolButton').icon = saveIcon
    self.get('MaterialReaderLoadToolButton').icon = loadIcon
    self.get('MaterialReaderSaveToolButton').icon = saveIcon
     #    - Signals/Slots
    self.get('MaterialReaderInputMeshNodeToolButton').connect('clicked()', self.loadMaterialReaderInputMeshNode)
    self.get('MaterialReaderOutputMeshNodeLoadToolButton').connect('clicked()', self.loadMaterialReaderMeshNode)
    self.get('MaterialReaderOutputMeshNodeToolButton').connect('clicked()', self.saveMaterialReaderMeshNode)
    self.get('MaterialReaderApplyPushButton').connect('clicked(bool)',self.runMaterialReader)
    self.get('MaterialReaderGoToPushButton').connect('clicked()', self.openMaterialReaderModule)
    self.get('ComputeArmatureWeightOutputPathLineEdit').connect('currentPathChanged(QString)', self.setMaterialReaderDefaultPath)

    self.get('ComputeArmatureWeightOutputPathLineEdit').connect('currentPathChanged(QString)', self.setWeightDirectory)

    # 7) (Pose) Armature And Pose Body
    # a) Pose Armature
    #    - Icons
    self.get('PoseArmatureArmatureNodeLoadToolButton').icon = loadIcon
    self.get('PoseArmatureArmatureNodeSaveToolButton').icon = saveIcon
    self.get('PoseArmatureSaveToolButton').icon = saveIcon
    #    - Signals/Slots
    self.get('PoseArmatureArmatureNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.setPoseArmatureModelNode)
    self.get('PoseArmatureArmatureNodeLoadToolButton').connect('clicked()', self.loadArmatureNode)
    self.get('PoseArmatureArmatureNodeSaveToolButton').connect('clicked()', self.savePoseArmatureArmatureNode)
    self.get('PoseArmaturesGoToPushButton').connect('clicked()', self.openPosedArmatureModule)
    # b) Simulate Pose
    #    - Icons
    self.get('SimulatePoseInputNodeToolButton').icon = loadIcon
    self.get('SimulatePoseOutputNodeLoadToolButton').icon = loadIcon
    self.get('SimulatePoseLoadToolButton').icon = loadIcon
    self.get('SimulatePoseSaveToolButton').icon = saveIcon
    self.get('SimulatePoseOutputNodeToolButton').icon = saveIcon
    #    - Signals/Slots
    self.get('SimulatePoseApplyPushButton').connect('clicked(bool)', self.runSimulatePose)
    self.get('SimulatePoseInputNodeToolButton').connect('clicked()', self.loadSimulatePoseInputNode)
    self.get('SimulatePoseOutputNodeLoadToolButton').connect('clicked()', self.loadSimulatePoseOutputNode)
    self.get('SimulatePoseOutputNodeToolButton').connect('clicked()', self.saveSimulatePoseOutputNode)
    self.get('SimulatePoseGoToPushButton').connect('clicked()', self.openSimulatePoseModule)

    # --------------------------------------------------------------------------
    # Initialize all the MRML aware GUI elements.
    # Lots of setup methods are called from this line
    self.setupComboboxes()
    self.widget.setMRMLScene(slicer.mrmlScene)
    # can be used to prevent processing when setting the scene. Other items
    # might not have the scene set yet.
    self.IsSetup = False

    # init pages after the scene is set.
    self.initWelcomePage()
    for page in self.pages.values():
      initMethod = getattr(self, 'init' + page + 'Page')
      initMethod()

    # Init title
    self.updateHeader()

    # Workflow page
    self.setupWorkflow(self.get('SettingsWorkflowComboBox').currentIndex)
    self.get('AdvancedPropertiesWidget').setVisible(self.get('ExpandAdvancedPropertiesButton').isChecked())

  # Worflow
  def updateHeader(self):
    # title
    title = self.WorkflowWidget.currentWidget().accessibleName
    self.TitleLabel.setText('<h2>%i/%i<br>%s</h2>' % (self.WorkflowWidget.currentIndex + 1, self.WorkflowWidget.count, title))

    # help
    self.get('HelpCollapsibleButton').setText('Help')
    self.get('HelpLabel').setText(self.WorkflowWidget.currentWidget().accessibleDescription)

    # Hide the Status if not running
    cliNode = self.get('CLIProgressBar').commandLineModuleNode()
    if cliNode != None and not cliNode.IsBusy():
      self.get('CLIProgressBar').setCommandLineModuleNode(0)

    # previous
    if self.WorkflowWidget.currentIndex > 0:
      self.get('PreviousPageToolButton').setVisible(True)
      previousIndex = self.WorkflowWidget.currentIndex - 1
      previousWidget = self.WorkflowWidget.widget(previousIndex)

      previous = previousWidget.accessibleName
      self.get('PreviousPageToolButton').setText('< %i/%i - %s' %(previousIndex + 1, self.WorkflowWidget.count, previous))
    else:
      self.get('PreviousPageToolButton').setVisible(False)

    # next
    if self.WorkflowWidget.currentIndex < self.WorkflowWidget.count - 1:
      self.get('NextPageToolButton').setVisible(True)
      nextIndex = self.WorkflowWidget.currentIndex + 1
      nextWidget = self.WorkflowWidget.widget(nextIndex)

      next = nextWidget.accessibleName
      self.get('NextPageToolButton').setText('%i/%i - %s >' %(nextIndex + 1, self.WorkflowWidget.count, next))
    else:
      self.get('NextPageToolButton').setVisible(False)
    self.get('NextPageToolButton').enabled = not self.isWorkflow( 0 )
    # disable the refreshes to avoid flickering
    self.WorkflowWidget.updatesEnabled = False
    # initialize the module
    openMethod = getattr(self,'open' + self.pages[self.WorkflowWidget.currentIndex] + 'Page')
    openMethod()
    workflowMode = self.get('SettingsWorkflowComboBox').currentIndex
    # turn the widget in advanced mode to show all the GUI components
    # so it takes as much space as possible.
    self.get('SettingsWorkflowComboBox').currentIndex = 2
    # expand all the collapsible group boxes to compute the minimum height.
    collapsedGroupBox = []
    for collapsibleGroupBox in self.WorkflowWidget.currentWidget().findChildren(ctk.ctkCollapsibleGroupBox):
      collapsedGroupBox.append(collapsibleGroupBox.collapsed)
      collapsibleGroupBox.collapsed = False
    self.WorkflowWidget.maximumHeight = self.WorkflowWidget.currentWidget().sizeHint.height()
    # restore the groupbox collapse mode
    for collapsibleGroupBox in self.WorkflowWidget.currentWidget().findChildren(ctk.ctkCollapsibleGroupBox):
      collapsibleGroupBox.collapsed = collapsedGroupBox.pop(0)
    self.get('SettingsWorkflowComboBox').currentIndex = workflowMode
    # validate to enable/disable next button
    validateMethod = getattr(self,'validate' + self.pages[self.WorkflowWidget.currentIndex] + 'Page')
    validateMethod()
    self.WorkflowWidget.updatesEnabled = True
    self.WorkflowWidget.resize(self.WorkflowWidget.width,
      self.WorkflowWidget.currentWidget().sizeHint.height())

  def goToPrevious(self):
    self.WorkflowWidget.setCurrentIndex(self.WorkflowWidget.currentIndex - 1)
    self.updateHeader()

  def goToNext(self):
    self.WorkflowWidget.setCurrentIndex(self.WorkflowWidget.currentIndex + 1)
    self.updateHeader()

  #----------------------------------------------------------------------------
  # 0) Workflow
  #  a) Settings
  #----------------------------------------------------------------------------
  def initWelcomePage(self):
    self.initData()
    # Collapse DataProbe as it takes screen real estate
    dataProbeCollapsibleWidget = self.findWidget(
      slicer.util.mainWindow(), 'DataProbeCollapsibleWidget')
    dataProbeCollapsibleWidget.checked = False

  def openWelcomePage(self):
    print('welcome')

  def isWorkflow(self, level):
    return self.get('SettingsWorkflowComboBox').currentIndex == level

  # Helper function for setting the visibility of a list of widgets
  def setWidgetsVisibility(self, widgets, level):
    for widget in widgets:
      workflow = widget.property('workflow')
      if workflow != None:
        widget.setVisible( str(level) in workflow )

  def setupWorkflow(self, level):
    self.setWidgetsVisibility(self.getChildren(self.WorkflowWidget), level)
    self.setWidgetsVisibility(self.getChildren(self.get('AdvancedTabWidget')), level)
    # Validate the current page (to disable/enable the next page tool button if needed)
    self.get('NextPageToolButton').enabled = True
    validateMethod = getattr(self,'validate' + self.pages[self.WorkflowWidget.currentIndex] + 'Page')
    validateMethod()

  def setupComboboxes(self):
    # Add here the combo box that should only see labelmaps
    labeldMapComboBoxes = ['MergeLabelsInputNodeComboBox',
                           'MergeLabelsOutputNodeComboBox',
                           'PadImageInputNodeComboBox',
                           'PadImageOutputNodeComboBox',
                           'CreateMeshInputNodeComboBox',
                           'CreateMeshOutputNodeComboBox',
                           'SkinModelMakerInputNodeComboBox',
                           'VolumeSkinningInputVolumeNodeComboBox',
                           'VolumeSkinningOutputVolumeNodeComboBox',
                           'EditSkinnedVolumeNodeComboBox',
                           'ComputeArmatureWeightInputVolumeNodeComboBox',
                           'ComputeArmatureWeightSkinnedVolumeVolumeNodeComboBox',
                           ]

    for combobox in labeldMapComboBoxes:
      self.get(combobox).addAttribute('vtkMRMLScalarVolumeNode','LabelMap','1')

  def observeCLINode(self, cliNode, onCLINodeModified = None):
    if cliNode != None and onCLINodeModified != None:
      self.addObserver(cliNode, self.StatusModifiedEvent, onCLINodeModified)
    self.get('CLIProgressBar').setCommandLineModuleNode(cliNode)

  #----------------------------------------------------------------------------
  #  b) Data
  #----------------------------------------------------------------------------
  def initData(self):
    self.IgnoreSetNodesVisibility = False
    self.get('VisibleNodesComboBox').sortFilterProxyModel().filterCaseSensitivity = qt.Qt.CaseInsensitive
    self.get('VisibleNodesComboBox').sortFilterProxyModel().sort(0)

    selectionNode = slicer.app.applicationLogic().GetSelectionNode()
    self.addObserver(selectionNode, 'ModifiedEvent', self.onNodeModified)

  def setNodesVisibility(self):
    """Set the visibility of nodes based on their check marks."""
    visibleNodes = self.get('VisibleNodesComboBox').checkedNodes()
    for node in visibleNodes:
      self.setNodeVisibility(node, 1)
    hiddenNodes =  self.get('VisibleNodesComboBox').uncheckedNodes()
    for node in hiddenNodes:
      self.setNodeVisibility(node, 0)

  def setNodeVisibility(self, node, visible):
    """Set the visiblity of a displayable node when the user checks it."""
    if self.IgnoreSetNodesVisibility == True:
      return
    selectionNode = slicer.app.applicationLogic().GetSelectionNode()
    if (node.IsA('vtkMRMLScalarVolumeNode')):
      if (not visible):
        if (selectionNode.GetActiveVolumeID() == node.GetID()):
          selectionNode.SetActiveVolumeID(None)
        if (selectionNode.GetActiveLabelVolumeID() == node.GetID()):
          selectionNode.SetActiveLabelVolumeID(None)
      else:
        if (node.GetLabelMap() == 0):
          selectionNode.SetActiveVolumeID(node.GetID())
        else:
          selectionNode.SetActiveLabelVolumeID(node.GetID())
      slicer.app.applicationLogic().PropagateVolumeSelection()
    elif node.IsA('vtkMRMLArmatureNode'):
      armatureLogic = slicer.modules.armatures.logic()
      armatureLogic.SetArmatureVisibility(node, visible)
    else:
      displayNode = node.GetDisplayNode()
      if displayNode != None:
        displayNode.SetVisibility(visible)

  def nodeVisibility(self, node):
    """Return true if the node is visible, false if it is hidden."""
    selectionNode = slicer.app.applicationLogic().GetSelectionNode()
    visible = False
    if (node.IsA('vtkMRMLScalarVolumeNode')):
      visible = (selectionNode.GetActiveVolumeID() == node.GetID() or
                 selectionNode.GetActiveLabelVolumeID() == node.GetID())
    elif node.IsA('vtkMRMLArmatureNode'):
      armatureLogic = slicer.modules.armatures.logic()
      visible = armatureLogic.GetArmatureVisibility(node)
    else:
      displayNode = node.GetDisplayNode()
      if (displayNode != None):
        visible = displayNode.GetVisibility() == 1
    return visible

  def onNodeAdded(self, node):
    """Observe the node to synchronize its visibility with the checkmarks"""
    self.addObserver(node, slicer.vtkMRMLDisplayableNode.DisplayModifiedEvent, self.onNodeModified)
    self.onNodeModified(node, 'DisplayModifiedEvent')

  def onNodeModified(self, node, event):
    """Update the node checkmark based on its visibility"""
    # Selection node is a special case
    if node.IsA('vtkMRMLSelectionNode'):
      # check all the volumes to see which one is active
      volumeNodes = slicer.mrmlScene.GetNodesByClass('vtkMRMLScalarVolumeNode')
      volumeNodes.UnRegister(slicer.mrmlScene)
      for i in range(0, volumeNodes.GetNumberOfItems()):
        volumeNode = volumeNodes.GetItemAsObject(i)
        self.onNodeModified(volumeNode, 'ModifiedEvent')
      return
    elif node.IsA('vtkMRMLArmatureNode'):
      # Hide the armature model node, it is not to be displayed
      armatureLogic = slicer.modules.armatures.logic()
      modelNode = armatureLogic.GetArmatureModel(node)
      if modelNode != None:
        self.get('VisibleNodesComboBox').sortFilterProxyModel().hiddenNodeIDs = [modelNode.GetID()]

    visible = self.nodeVisibility(node)
    checkState = qt.Qt.Checked if visible else qt.Qt.Unchecked
    self.IgnoreSetNodesVisibility = True
    self.get('VisibleNodesComboBox').setCheckState(node, checkState)
    self.IgnoreSetNodesVisibility = False

  #----------------------------------------------------------------------------
  #  c) Volume Render
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
    table = self.get('LabelsTableWidget')
    for row in range(table.rowCount):
      currentStructure = table.verticalHeaderItem(row).text()
      item = table.item(row, 0)
      if currentStructure.lower() != 'background' and item:
        labels.append(item.text())
    self.get('VolumeRenderLabelsLineEdit').setText(', '.join(str(val) for val in labels))

  def getVolumeRenderLabels(self):
    labels = self.get('VolumeRenderLabelsLineEdit').text.split(', ')
    labels = filter(lambda x: x != '', labels)
    return labels

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
      if (str(i) in labels) or (i != 0 and len(labels) == 0):
        node[1] = 0.5
        node[3] = 1
      else:
        node[1] = 0
        node[3] = 1
      opacities.SetNodeValue(i, node)
    opacities.Modified()

  def runVolumeRender(self, show):
    """Start/stop to volume render a volume"""
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
      self.onCropVolumeRender(self.get('VolumeRenderCropCheckBox').checked)

  def onCropVolumeRender(self, crop):
    volumeNode = self.get('VolumeRenderInputNodeComboBox').currentNode()
    if volumeNode == None:
      return
    displayNode = volumeNode.GetNthDisplayNodeByClass(0, 'vtkMRMLVolumeRenderingDisplayNode')
    if displayNode == None:
      return
    roiNode = displayNode.GetROINode()
    roiNode.SetDisplayVisibility(crop)
    displayNode.SetCroppingEnabled(crop)

  def openVolumeRenderModule(self):
    self.openModule('VolumeRendering')

  #----------------------------------------------------------------------------
  # 1) Adjust
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

    self.initLabelmap()

  def validateAdjustPage(self, validateSections = True):
    if validateSections:
      self.validateLabelmap()
    valid = self.get('LabelmapCollapsibleGroupBox').property('valid')
    self.get('NextPageToolButton').enabled = not self.isWorkflow(0) or valid

  def openAdjustPage(self):
    # Switch to 3D View only
    slicer.app.layoutManager().setLayout(slicer.vtkMRMLLayoutNode.SlicerLayoutFourUpView)

  #----------------------------------------------------------------------------
  #     a) Labelmap
  def initLabelmap(self):
    self.validateLabelmap()

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

  def validateLabelmap(self):
    volumeNode = self.get('LabelmapVolumeNodeComboBox').currentNode()
    colorNode = self.get('LabelmapColorNodeComboBox').currentNode()
    valid = volumeNode != None and colorNode != None
    self.get('LabelmapCollapsibleGroupBox').setProperty('valid', valid)
    if valid:
      self.get('VolumeRenderInputNodeComboBox').setCurrentNode(
        self.get('LabelmapVolumeNodeComboBox').currentNode())
    self.validateAdjustPage(validateSections = False)

  def loadLabelmapVolumeNode(self):
    self.loadFile('Volume/Labelmap to reposition', 'VolumeFile', self.get('LabelmapVolumeNodeComboBox'))

  def loadLabelmapColorNode(self):
    self.loadFile('Tissue/Color file', 'ColorTableFile', self.get('LabelmapColorNodeComboBox'))

  def applyColorNode(self):
    volumeNode = self.get('LabelmapVolumeNodeComboBox').currentNode()
    if volumeNode == None:
      self.validateLabelmap()
      return

    colorNode = self.get('LabelmapColorNodeComboBox').currentNode()
    volumesLogic = slicer.modules.volumes.logic()

    wasModifying = volumeNode.StartModify()
    volumesLogic.SetVolumeAsLabelMap(volumeNode, colorNode != None) # Greyscale is None

    labelmapDisplayNode = volumeNode.GetDisplayNode()
    if colorNode != None:
      labelmapDisplayNode.SetAndObserveColorNodeID(colorNode.GetID())
    volumeNode.EndModify(wasModifying)

    # We can't just use a regular qt signal/slot connection because the input
    # node might not be a labelmap at the time it becomes current, which would
    # not show up in the combobox.
    self.get('MergeLabelsInputNodeComboBox').setCurrentNode(volumeNode)
    self.setupMergeLabels(volumeNode)

    self.validateLabelmap()

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
  # 2) Extract
  #----------------------------------------------------------------------------
  def initExtractPage(self):
    self.initMergeLabels()
    self.initPadImage()

  def validateExtractPage(self, validateSections = True):
    if validateSections:
      self.validateMergeLabels()
      self.validatePadImage()
    valid = self.get('PadImageCollapsibleGroupBox').property('valid')
    self.get('NextPageToolButton').enabled = not self.isWorkflow(0) or valid

  def openExtractPage(self):
    pass

  #----------------------------------------------------------------------------
  #    a) Merge Labels
  def initMergeLabels(self):
    self.setupMergeLabels(self.get('MergeLabelsInputNodeComboBox').currentNode())
    self.validateMergeLabels()

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

    table = self.get('LabelsTableWidget')
    if colorNode == None: # No color node -> empty columns
      for row in range(table.rowCount):
        for column in range(table.columnCount()):
          table.item(row, column).setText('')

    else:
      nonMuscleLabels = []
      for row in range(table.rowCount):
        currentStructure = table.verticalHeaderItem(row).text()
        currentStructure = currentStructure.lower()

        # Get labels
        labels = {}
        for equivalentStructure in self.equivalentLabelStructures[currentStructure]:
          labels.update(self.searchLabels(colorNode, equivalentStructure))

        # Get corresponding best label
        if currentStructure == 'background':
          labelNames = ['air']
        else:
          labelNames = [currentStructure]
        try:
          labelNames.append(self.equivalentLabelStructures[currentStructure + 'bestlabel'])
        except KeyError:
          pass # Do nothing
        label = self.bestLabel(labels, labelNames)

        table.item(row, 0).setText(label)
        table.item(row, 1).setText(', '.join(str( val ) for val in labels.keys()))

        if currentStructure != 'muscle':
          nonMuscleLabels.extend(labels.keys())

      # Now the muscle case. Muscle has all the labels but the one already selected
      muscleLabels = []
      for l in range(colorNode.GetNumberOfColors()):
        if l not in nonMuscleLabels:
          muscleLabels.append(l)
      table.item(2, 1).setText(', '.join(str( val ) for val in muscleLabels))

      self.createMergeLabelsOutput(volumeNode)
      self.addObserver(volumeNode, 'ModifiedEvent', self.updateMergeLabels)
      self.addObserver(labelmapDisplayNode, 'ModifiedEvent', self.updateMergeLabels)
    self.validateMergeLabels()

  def validateMergeLabels(self):
    cliNode = self.getCLINode(slicer.modules.changelabel)
    valid = (cliNode.GetStatusString() == 'Completed')
    self.get('MergeLabelsOutputNodeToolButton').enabled = valid
    self.get('MergeLabelsSaveToolButton').enabled = valid
    self.get('MergeLabelsCollapsibleGroupBox').setProperty('valid',valid)
    if valid:
      self.get('VolumeRenderInputNodeComboBox').setCurrentNode(
        self.get('MergeLabelsOutputNodeComboBox').currentNode())

    enablePadImage = not self.isWorkflow(0) or valid
    self.get('PadImageCollapsibleGroupBox').collapsed = not enablePadImage
    self.get('PadImageCollapsibleGroupBox').setEnabled(enablePadImage)

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
    if node == None or self.IsSetup:
      return
    # Don't create the node if the name already contains "merged"
    if node.GetName().lower().find('merged') != -1:
      return
    nodeName = '%s-merged' % node.GetName()
    # make sure such node does not already exist.
    mergedNode = self.getFirstNodeByNameAndClass(nodeName, 'vtkMRMLScalarVolumeNode')
    if mergedNode == None:
      self.get('MergeLabelsOutputNodeComboBox').selectNodeUponCreation = False
      newNode = self.get('MergeLabelsOutputNodeComboBox').addNode()
      self.get('MergeLabelsOutputNodeComboBox').selectNodeUponCreation = True
      newNode.SetName(nodeName)
      mergedNode = newNode

    self.get('MergeLabelsOutputNodeComboBox').setCurrentNode(mergedNode)

  def mergeLabelsParameters(self):
    parameters = {}
    parameters["InputVolume"] = self.get('MergeLabelsInputNodeComboBox').currentNode()
    parameters["OutputVolume"] = self.get('MergeLabelsOutputNodeComboBox').currentNode()

    inputLabels = ''
    inputLabelNumber = ''
    outputLabels = ''
    for row in range(self.get('LabelsTableWidget').rowCount):
      labelsItem = self.get('LabelsTableWidget').item(row, 1)
      outputItem = self.get('LabelsTableWidget').item(row, 0)
      if labelsItem and labelsItem.text() != '' and outputItem and outputItem.text() != '':
        labels = labelsItem.text()
        inputLabelNumber = inputLabelNumber + str(len(labels.split(','))) + ','
        inputLabels = inputLabels + labels + ','
        outputLabels = outputLabels + outputItem.text() + ','

    parameters["InputLabelNumber"] = inputLabelNumber
    parameters["InputLabel"] = inputLabels
    parameters["OutputLabel"] = outputLabels
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
      self.get('MergeLabelsApplyPushButton').enabled = False
      cliNode.Cancel()

  def onMergeLabelsCLIModified(self, cliNode, event):
    if cliNode.GetStatusString() == 'Completed':
      # apply label map
      newNode = self.get('MergeLabelsOutputNodeComboBox').currentNode()
      self.applyLabelmap(newNode)
      self.validateMergeLabels()

    if not cliNode.IsBusy():
      self.get('MergeLabelsApplyPushButton').setChecked(False)
      self.get('MergeLabelsApplyPushButton').enabled = True
      print 'MergeLabels %s' % cliNode.GetStatusString()
      self.removeObservers(self.onMergeLabelsCLIModified)

  def applyLabelmap(self, volumeNode):
    if volumeNode == None:
      return
    displayNode = volumeNode.GetDisplayNode()
    if displayNode == None:
      volumesLogic = slicer.modules.volumes.logic()
      volumesLogic.SetVolumeAsLabelMap(volumeNode, 1)
      displayNode = volumeNode.GetDisplayNode()
    colorNode = self.get('LabelmapColorNodeComboBox').currentNode()
    if displayNode != None and colorNode != None:
      displayNode.SetAndObserveColorNodeID(colorNode.GetID())

  def loadMergeLabelsVolumeNode(self):
    loadedNode = self.loadLabelmapFile('Merged label volume', 'VolumeFile', self.get('MergeLabelsOutputNodeComboBox'))
    if loadedNode != None:
      cliNode = self.getCLINode(slicer.modules.changelabel)
      self.observeCLINode(cliNode, self.onMergeLabelsCLIModified)
      cliNode.SetStatus(slicer.vtkMRMLCommandLineModuleNode.Completed)

  def saveMergeLabelsVolumeNode(self):
    self.saveFile('Merged label volume', 'VolumeFile', '.mha', self.get('MergeLabelsOutputNodeComboBox'))

  def openMergeLabelsModule(self):
    self.openModule('ChangeLabel')

    cliNode = self.getCLINode(slicer.modules.changelabel)
    parameters = self.mergeLabelsParameters()
    slicer.cli.setNodeParameters(cliNode, parameters)

  def getMergedLabel(self, structure):
    '''Returns the label of the given structure as a string'''
    table = self.get('LabelsTableWidget')
    for row in range(table.rowCount):
      currentStructure = table.verticalHeaderItem(row).text()
      if currentStructure.lower() == structure:
        return self.get('LabelsTableWidget').item(row, 0).text()

  #----------------------------------------------------------------------------
  #    b) Pad Image
  def initPadImage(self):
    self.setupPadImage(self.get('PadImageInputNodeComboBox').currentNode())
    self.validatePadImage()

  def updatePadImage(self, node, event):
    volumeNode = self.get('PadImageInputNodeComboBox').currentNode()
    if volumeNode == None or (node.IsA('vtkMRMLScalarVolumeNode') and node != volumeNode):
      return
    elif node.IsA('vtkMRMLVolumeDisplayNode'):
      if node != volumeNode.GetDisplayNode():
        return
    self.setupPadImage(volumeNode)

  def setupPadImage(self, volumeNode):
    if volumeNode == None or not volumeNode.GetLabelMap():
      return
    self.removeObservers(self.updatePadImage)

    self.createPadImageOutput(volumeNode)
    self.addObserver(volumeNode, 'ModifiedEvent', self.updatePadImage)
    self.validatePadImage()

  def validatePadImage(self):
    cliNode = self.getCLINode(slicer.modules.padimage)
    valid = (cliNode.GetStatusString() == 'Completed')
    self.get('PadImageOutputNodeToolButton').enabled = valid
    self.get('PadImageSaveToolButton').enabled = valid
    self.get('PadImageCollapsibleGroupBox').setProperty('valid',valid)

    self.validateExtractPage(validateSections = False)

  def createPadImageOutput(self, node):
    """ Make sure the output scalar volume node is a node with a -padded suffix.
        Note that the padded volume is used only by the merge labels module. This
        should not be used by the PoseLabelmap filter.
    """
    if node == None or self.IsSetup:
      return
    # Don't create the node if the name already contains "padded"
    if node.GetName().lower().find('padded') != -1:
      return
    nodeName = '%s-padded' % node.GetName()
    # make sure such node does not already exist.
    paddedNode = self.getFirstNodeByNameAndClass(nodeName, 'vtkMRMLScalarVolumeNode')
    if paddedNode == None:
      self.get('PadImageOutputNodeComboBox').selectNodeUponCreation = False
      newNode = self.get('PadImageOutputNodeComboBox').addNode()
      self.get('PadImageOutputNodeComboBox').selectNodeUponCreation = True
      newNode.SetName(nodeName)
      paddedNode = newNode

    self.get('PadImageOutputNodeComboBox').setCurrentNode(paddedNode)

  def padImageParameters(self):
    parameters = {}
    parameters["InputVolume"] = self.get('PadImageInputNodeComboBox').currentNode()
    parameters["OutputVolume"] = self.get('PadImageOutputNodeComboBox').currentNode()

    parameters["PadValue"] = self.get('PadImageValueSpinBox').value
    parameters["PadThickness"] = self.get('PadImageThicknessSpinBox').value
    return parameters

  def runPadImage(self, run):
    if run:
      cliNode = self.getCLINode(slicer.modules.padimage)
      parameters = self.padImageParameters()
      self.get('PadImageApplyPushButton').setChecked(True)
      self.observeCLINode(cliNode, self.onPadImageCLIModified)
      cliNode = slicer.cli.run(slicer.modules.padimage, cliNode, parameters, wait_for_completion = False)
    else:
      cliNode = self.observer(self.StatusModifiedEvent, self.onPadImageCLIModified)
      self.get('PadImageApplyPushButton').enabled = False
      cliNode.Cancel()

  def onPadImageCLIModified(self, cliNode, event):
    if cliNode.GetStatusString() == 'Completed':
      # apply label map
      newNode = self.get('PadImageOutputNodeComboBox').currentNode()
      if newNode != None:
        displayNode = newNode.GetDisplayNode()
        if displayNode == None:
          volumesLogic = slicer.modules.volumes.logic()
          volumesLogic.SetVolumeAsLabelMap(newNode, 1)
          displayNode = newNode.GetDisplayNode()
        colorNode = self.get('LabelmapColorNodeComboBox').currentNode()
        if displayNode != None and colorNode != None:
          displayNode.SetAndObserveColorNodeID(colorNode.GetID())
      self.validatePadImage()

    if not cliNode.IsBusy():
      self.get('PadImageApplyPushButton').setChecked(False)
      self.get('PadImageApplyPushButton').enabled = True
      print 'PadImage %s' % cliNode.GetStatusString()
      self.removeObservers(self.onPadImageCLIModified)

  def loadPadImageVolumeNode(self):
    loadedNode = self.loadLabelmap('Padded volume', self.get('PadImageOutputNodeComboBox'))
    if loadedNode != None:
      cliNode = self.getCLINode(slicer.modules.padimage)
      self.observeCLINode(cliNode, self.onPadImageCLIModified)
      cliNode.SetStatus(slicer.vtkMRMLCommandLineModuleNode.Completed)

  def savePadImageVolumeNode(self):
    self.saveFile('Padded volume', 'VolumeFile', '.mha', self.get('PadImageOutputNodeComboBox'))

  def openPadImageModule(self):
    self.openModule('PadImage')

    cliNode = self.getCLINode(slicer.modules.padimage)
    parameters = self.padImageParameters()
    slicer.cli.setNodeParameters(cliNode, parameters)

  #----------------------------------------------------------------------------
  # 3) Mesh
  #----------------------------------------------------------------------------
  def initMeshPage(self):
    self.initCreateMesh()
    self.initExtractBone()
    self.initExtractSkin()

  def validateMeshPage(self, validateSections = True):
    if validateSections:
      self.validateCreateTetrahedralMesh()
      self.validateExtractBone()
      self.validateExtractSkin()
    valid = self.get('SkinModelMakerCollapsibleGroupBox').property('valid')
    self.get('NextPageToolButton').enabled = not self.isWorkflow(0) or valid

  def openMeshPage(self):
    pass

  #----------------------------------------------------------------------------
  #    a) Create  mesh
  def initCreateMesh(self):
    self.validateCreateTetrahedralMesh()

  def validateCreateTetrahedralMesh(self):
    cliNode = self.getCLINode(slicer.modules.createtetrahedralmesh)
    valid = (cliNode.GetStatusString() == 'Completed'
            and self.get('CreateMeshOutputNodeComboBox').currentNode())

    self.get('CreateMeshOutputToolButton').enabled = valid
    self.get('CreateMeshToolButton').enabled = valid
    self.get('CreateMeshCollapsibleGroupBox').setProperty('valid',valid)

    enableExtractBone = not self.isWorkflow(0) or valid
    self.get('ExtractBoneCollapsibleGroupBox').collapsed = not enableExtractBone
    self.get('ExtractBoneCollapsibleGroupBox').setEnabled(enableExtractBone)

    self.validateMeshPage(validateSections = False)

  def createMeshOutput(self, node):
    if node == None or self.IsSetup:
      return
    # Don't create the node if the name already contains "tetmesh"
    if node.GetName().lower().find('tetmesh') != -1:
      return
    nodeName = '%s-tetmesh' % node.GetName()
    # make sure such node does not already exist.
    meshNode = self.getFirstNodeByNameAndClass(nodeName, 'vtkMRMLModelNode')
    if meshNode == None:
      self.get('CreateMeshOutputNodeComboBox').selectNodeUponCreation = False
      newNode = self.get('CreateMeshOutputNodeComboBox').addNode()
      self.get('CreateMeshOutputNodeComboBox').selectNodeUponCreation = True
      newNode.SetName(nodeName)
      meshNode = newNode

    self.get('CreateMeshOutputNodeComboBox').setCurrentNode(meshNode)

  def createMeshParameters(self):
    parameters = {}
    parameters["InputVolume"] = self.get('CreateMeshInputNodeComboBox').currentNode()
    parameters["OutputMesh"] = self.get('CreateMeshOutputNodeComboBox').currentNode()
    parameters["Padding"] = self.get('CreateMeshPadImageCheckBox').isChecked()
    parameters["Verbose"] = False
    return parameters

  def runCreateMesh(self, run):
    if run:
      cliNode = self.getCLINode(slicer.modules.createtetrahedralmesh)
      parameters = self.createMeshParameters()
      self.get('CreateMeshPushButton').setChecked(True)
      self.observeCLINode(cliNode, self.onCreateMeshCLIModified)
      cliNode = slicer.cli.run(slicer.modules.createtetrahedralmesh, cliNode, parameters, wait_for_completion = False)
    else:
      cliNode = self.observer(self.StatusModifiedEvent, self.onCreateMeshCLIModified)
      self.get('CreateMeshPushButton').enabled = False
      cliNode.Cancel()

  def onCreateMeshCLIModified(self, cliNode, event):
    if cliNode.GetStatusString() == 'Completed':
      # Set color
      newNode = self.get('CreateMeshOutputNodeComboBox').currentNode()
      newNodeDisplayNode = newNode.GetModelDisplayNode()
      colorNode = self.get('CreateMeshInputNodeComboBox').currentNode().GetDisplayNode().GetColorNode()
      if newNodeDisplayNode and colorNode:
        newNodeDisplayNode.SetActiveScalarName('MaterialId')
        newNodeDisplayNode.SetActiveAttributeLocation(1) # MaterialId is a cell data array
        newNodeDisplayNode.SetScalarVisibility(True)
        newNodeDisplayNode.SetAndObserveColorNodeID(colorNode.GetID())
        newNodeDisplayNode.UpdatePolyDataPipeline()

      # Reset camera
      self.reset3DViews()

      self.validateCreateTetrahedralMesh()

    if not cliNode.IsBusy():
      self.get('CreateMeshPushButton').setChecked(False)
      self.get('CreateMeshPushButton').enabled = True
      print 'CreateMesh %s' % cliNode.GetStatusString()
      self.removeObservers(self.onCreateMeshCLIModified)

  def loadMeshNode(self):
    loadedNode = self.loadFile('Tetrahedral Mesh', 'ModelFile',
                               self.get('CreateMeshOutputNodeComboBox'))
    if loadedNode != None:
      cliNode = self.getCLINode(slicer.modules.createtetrahedralmesh)
      self.observeCLINode(cliNode, self.onCreateMeshCLIModified)
      cliNode.SetStatus(slicer.vtkMRMLCommandLineModuleNode.Completed)

  def saveMeshNode(self):
    self.saveFile('Tetrahedral Mesh', 'ModelFile', '.vtk', self.get('CreateMeshOutputNodeComboBox'))

  def openCreateMeshModule(self):
    self.openModule('CreateTetrahedralMesh')

    cliNode = self.getCLINode(slicer.modules.createtetrahedralmesh)
    parameters = self.createMeshParameters()
    slicer.cli.setNodeParameters(cliNode, parameters)

  #----------------------------------------------------------------------------
  #    b) Extract bone
  def initExtractBone(self):
    self.validateExtractBone()

  def validateExtractBone(self):
    cliNode = self.getCLINode(slicer.modules.volumematerialextractor)
    valid = (cliNode.GetStatusString() == 'Completed'
            and self.get('ExtractBoneOutputComboBox').currentNode())

    self.get('ExtractBoneOutputToolButton').enabled = valid
    self.get('ExtractBoneToolButton').enabled = valid
    self.get('ExtractBoneCollapsibleGroupBox').setProperty('valid',valid)

    enableExtractSkin = not self.isWorkflow(0) or valid
    self.get('SkinModelMakerCollapsibleGroupBox').collapsed = not enableExtractSkin
    self.get('SkinModelMakerCollapsibleGroupBox').setEnabled(enableExtractSkin)

    self.validateMeshPage(validateSections = False)

  def createExtractBoneOutput(self, node):
    if node == None or self.IsSetup:
      return
    # Don't create the node if the name already contains "bones"
    if node.GetName().lower().find('bones') != -1:
      return
    nodeName = '%s-bones' % node.GetName()
    # make sure such node does not already exist.
    meshNode = self.getFirstNodeByNameAndClass(nodeName, 'vtkMRMLModelNode')
    if meshNode == None:
      self.get('ExtractBoneOutputComboBox').selectNodeUponCreation = False
      newNode = self.get('ExtractBoneOutputComboBox').addNode()
      self.get('ExtractBoneOutputComboBox').selectNodeUponCreation = True
      newNode.SetName(nodeName)
      meshNode = newNode

    self.get('ExtractBoneOutputComboBox').setCurrentNode(meshNode)

  def extractBoneParameters(self):
    parameters = {}
    parameters["InputTetMesh"] = self.get('ExtractBoneInputComboBox').currentNode()
    parameters["OutputTetMesh"] = self.get('ExtractBoneOutputComboBox').currentNode()
    parameters["MaterialLabel"] = self.get('ExtractBoneMaterialSpinBox').value

    return parameters

  def runExtractBone(self, run):
    if run:
      cliNode = self.getCLINode(slicer.modules.volumematerialextractor)
      parameters = self.extractBoneParameters()
      self.get('ExtractBonePushButton').setChecked(True)
      self.observeCLINode(cliNode, self.onExtractBoneCLIModified)
      cliNode = slicer.cli.run(slicer.modules.volumematerialextractor, cliNode, parameters, wait_for_completion = False)
    else:
      cliNode = self.observer(self.StatusModifiedEvent, self.onExtractBoneCLIModified)
      self.get('ExtractBonePushButton').enabled = False
      cliNode.Cancel()

  def onExtractBoneCLIModified(self, cliNode, event):
    if cliNode.GetStatusString() == 'Completed':
      # Hide input
      inputMesh = self.get('ExtractBoneInputComboBox').currentNode()
      displayNode = inputMesh.GetDisplayNode()
      if displayNode != None:
        displayNode.SetVisibility(False)

      # Set color
      newNode = self.get('ExtractBoneOutputComboBox').currentNode()
      newNodeDisplayNode = newNode.GetModelDisplayNode()
      colorNode = self.get('CreateMeshInputNodeComboBox').currentNode().GetDisplayNode().GetColorNode()
      if colorNode:
        color = [0, 0, 0]
        lookupTable = colorNode.GetLookupTable().GetColor(self.get('ExtractBoneMaterialSpinBox').value, color)
        newNodeDisplayNode.SetColor(color)

      # Set Clip intersection ON
      newNodeDisplayNode.SetSliceIntersectionVisibility(1)

      # Reset camera
      self.reset3DViews()

      self.validateExtractBone()

    if not cliNode.IsBusy():
      self.get('ExtractBonePushButton').setChecked(False)
      self.get('ExtractBonePushButton').enabled = True
      print 'Extract Bone %s' % cliNode.GetStatusString()
      self.removeObservers(self.onExtractBoneCLIModified)

  def loadExtractBone(self):
    loadedNode = self.loadFile('Bone Mesh', 'ModelFile',
                               self.get('ExtractBoneOutputComboBox'))
    if loadedNode != None:
      cliNode = self.getCLINode(slicer.modules.volumematerialextractor)
      self.observeCLINode(cliNode, self.onExtractBoneCLIModified)
      cliNode.SetStatus(slicer.vtkMRMLCommandLineModuleNode.Completed)


  def saveExtractBone(self):
    self.saveFile('Bone Mesh', 'ModelFile', '.vtk', self.get('ExtractBoneOutputComboBox'))

  def openExtractBoneModule(self):
    self.openModule('VolumeMaterialExtractor')

    cliNode = self.getCLINode(slicer.modules.volumematerialextractor)
    parameters = self.extractBoneParameters()
    slicer.cli.setNodeParameters(cliNode, parameters)

  def onBoneMaterialChanged(self):
    try:
      value = int(self.getMergedLabel('bone'))
    except ValueError:
      value = 0
    self.get('ExtractBoneMaterialSpinBox').value = value
    self.get('ComputeArmatureWeightBoneSpinBox').value = value

  #----------------------------------------------------------------------------
  #    c) Extract skin
  def initExtractSkin(self):
    import SkinModelMaker

    self.SkinModelMakerLogic = SkinModelMaker.SkinModelMakerLogic()
    self.validateExtractSkin()

  def validateExtractSkin(self):
    cliNode = self.SkinModelMakerLogic.GetCLINode()
    valid = cliNode.GetStatusString() == 'Completed'
    self.get('SkinModelMakerOutputNodeToolButton').enabled = valid
    self.get('SkinModelMakerSaveToolButton').enabled = valid
    self.get('SkinModelMakerToggleVisiblePushButton').enabled = valid
    self.get('ArmaturesToggleVisiblePushButton').enabled = valid
    self.get('SkinModelMakerCollapsibleGroupBox').setProperty('valid',valid)
    self.validateExtractPage(validateSections = False)

    self.validateMeshPage(validateSections = False)

  def createExtractSkinOutput(self, node):
    if node == None or self.IsSetup:
      return
    # Don't create the node if the name already contains "skin"
    if node.GetName().lower().find('skin') != -1:
      return
    nodeName = '%s-skin' % node.GetName()
    # make sure such node does not already exist.
    meshNode = self.getFirstNodeByNameAndClass(nodeName, 'vtkMRMLModelNode')
    if meshNode == None:
      self.get('SkinModelMakerOutputNodeComboBox').selectNodeUponCreation = False
      newNode = self.get('SkinModelMakerOutputNodeComboBox').addNode()
      self.get('SkinModelMakerOutputNodeComboBox').selectNodeUponCreation = True
      newNode.SetName(nodeName)
      meshNode = newNode

    self.get('SkinModelMakerOutputNodeComboBox').setCurrentNode(meshNode)

  def extractSkinParameters(self):
    parameters = {}
    parameters["InputVolume"] = self.get('SkinModelMakerInputNodeComboBox').currentNode()
    parameters["OutputGeometry"] = self.get('SkinModelMakerOutputNodeComboBox').currentNode()
    parameters["BackgroundLabel"] = self.get('SkinModelMakerBackgroundLabelSpinBox').value
    parameters["SkinLabel"] = str(self.get('SkinModelMakerSkinLabelSpinBox').value)
    parameters["Decimate"] = True
    parameters["Spacing"] = '6,6,6'

    return parameters

  def runExtractSkin(self, run):
    if run:
      parameters = self.extractSkinParameters()
      self.get('SkinModelMakerApplyPushButton').setChecked(True)
      self.observeCLINode(self.SkinModelMakerLogic.GetCLINode(), self.onExtractSkinCLIModified)
      self.SkinModelMakerLogic.CreateSkinModel(parameters, wait_for_completion = False)
    else:
      self.get('SkinModelMakerApplyPushButton').enabled = False
      self.SkinModelMakerLogic.Cancel()

  def onExtractSkinCLIModified(self, cliNode, event):
    if cliNode.GetStatusString() == 'Completed':
      # Hide input
      input = self.get('SkinModelMakerInputNodeComboBox').currentNode()
      displayNode = input.GetDisplayNode()
      if displayNode != None:
        displayNode.SetVisibility(False)

      # Set opacity
      newNode = self.get('SkinModelMakerOutputNodeComboBox').currentNode()
      newNodeDisplayNode = newNode.GetModelDisplayNode()
      newNodeDisplayNode.SetOpacity(0.2)

      # Set color
      colorNode = self.get('CreateMeshInputNodeComboBox').currentNode().GetDisplayNode().GetColorNode()
      if colorNode:
        color = [0, 0, 0]
        lookupTable = colorNode.GetLookupTable().GetColor(self.get('SkinModelMakerSkinLabelSpinBox').value, color)
        newNodeDisplayNode.SetColor(color)

      # Set Clip intersection ON
      newNodeDisplayNode.SetSliceIntersectionVisibility(1)

      # Reset camera
      self.reset3DViews()

      self.validateExtractSkin()

    if not cliNode.IsBusy():
      self.get('SkinModelMakerApplyPushButton').setChecked(False)
      self.get('SkinModelMakerApplyPushButton').enabled = True
      print 'Extract Skin %s' % cliNode.GetStatusString()
      self.removeObservers(self.onExtractSkinCLIModified)

  def loadExtractSkin(self):
    loadedNode = self.loadFile('Bone Skin', 'ModelFile',
                               self.get('SkinModelMakerOutputNodeComboBox'))
    if loadedNode != None:
      cliNode = self.SkinModelMakerLogic.GetCLINode()
      self.observeCLINode(cliNode, self.onExtractSkinCLIModified)
      cliNode.SetStatus(slicer.vtkMRMLCommandLineModuleNode.Completed)

  def saveExtractSkin(self):
    self.saveFile('Bone Skin', 'ModelFile', '.vtk', self.get('SkinModelMakerOutputNodeComboBox'))

  def openModelsModule(self):
    self.openModule('Models')

  def openExtractSkinModule(self):
    self.openModule('SkinModelMaker')

    cliNode = self.getCLINode(slicer.modules.skinmodelmaker)
    parameters = self.extractSkinParameters()
    slicer.cli.setNodeParameters(cliNode, parameters)

  def onSkinMaterialChanged(self):
    try:
      value = int(self.getMergedLabel('skin'))
    except ValueError:
      value = 0
    self.get('SkinModelMakerSkinLabelSpinBox').value = value

  def onBackgroundMaterialChanged(self):
    try:
      value = int(self.getMergedLabel('background'))
    except ValueError:
      value = 0
    self.get('SkinModelMakerBackgroundLabelSpinBox').value = value
    self.get('ComputeArmatureWeightBackgroundSpinBox').value = value

  #----------------------------------------------------------------------------
  # 4) Armature
  #----------------------------------------------------------------------------
  def initArmaturePage(self):
    self.initArmature()

  def validateArmaturePage(self, validateSections = True):
    if validateSections:
      self.validateArmature()
    valid = self.get('ArmaturesCollapsibleGroupBox').property('valid')
    self.get('NextPageToolButton').enabled = not self.isWorkflow(0) or valid

  def openArmaturePage(self):
    self.reset3DViews()
    # Switch to 3D View only
    manager = slicer.app.layoutManager()
    manager.setLayout(slicer.vtkMRMLLayoutNode.SlicerLayoutOneUp3DView)

  #----------------------------------------------------------------------------
  #  Armature
  def initArmature(self):
    presetComboBox = self.findWidget(slicer.modules.armatures.widgetRepresentation(), 'LoadArmatureFromModelComboBox')
    for i in range(presetComboBox.count):
      text = presetComboBox.itemText(i)
      if text:
        self.get('ArmaturesPresetComboBox').addItem( text )
      else:
        self.get('ArmaturesPresetComboBox').insertSeparator(self.get('ArmaturesPresetComboBox').count)
    self.get('ArmaturesPresetComboBox').setCurrentIndex(-1)
    self.validateArmature()

  def validateArmature(self):
    valid = (self.get('ArmaturesArmatureNodeComboBox').currentNode() != None and
             self.get('ArmaturesArmatureNodeComboBox').currentNode().GetAssociatedNode() != None)
    self.get('ArmaturesCollapsibleGroupBox').setProperty('valid', valid)
    self.validateArmaturePage(validateSections = False)

  def setCurrentArmatureModelNode(self, armatureNode):
    if armatureNode != None:
      modelNode = armatureNode.GetAssociatedNode()
      if modelNode != None:
        self.get('VolumeSkinningAmartureNodeComboBox').setCurrentNode(modelNode)
      else:
        self.addObserver(armatureNode, 'ModifiedEvent', self.onArmatureNodeModified)
    self.validateArmature()

  def loadArmaturePreset(self, index):
    if index == -1:
      return
    presetComboBox = self.findWidget(slicer.modules.armatures.widgetRepresentation(), 'LoadArmatureFromModelComboBox')
    presetComboBox.setCurrentIndex(index)
    self.get('ArmaturesPresetComboBox').setCurrentIndex(-1)

  def onArmatureNodeAdded(self, armatureNode):
    self.get('ArmaturesArmatureNodeComboBox').setCurrentNode(armatureNode)
    name = 'armature'
    if self.get('LabelmapVolumeNodeComboBox').currentNode() != None:
      name = self.get('LabelmapVolumeNodeComboBox').currentNode().GetName() + '-armature'
    name = slicer.mrmlScene.GenerateUniqueName(name)
    armatureNode.SetName(name)

  def onArmatureNodeModified(self, armatureNode, event):
    '''This method can be called when a previously (or still) current armature
    node is modified but that did not have a model node at the time it was set
    current. It now try to recall the method that set the armature model to
    the model node comboboxes.'''
    self.removeObservers(self.onArmatureNodeModified)
    if self.get('ArmaturesArmatureNodeComboBox').currentNode() == armatureNode:
      self.setCurrentArmatureModelNode(armatureNode)

  def updateSkinNodeVisibility(self):
    skinNode = self.get('SkinModelMakerOutputNodeComboBox').currentNode()
    if skinNode:
      skinDisplayNode = skinNode.GetModelDisplayNode()
      if skinDisplayNode:
        skinDisplayNode.SetVisibility(not skinDisplayNode.GetVisibility())

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
  # 5) Skinning
  #----------------------------------------------------------------------------
  def initSkinningPage(self):
    self.initVolumeSkinning()
    self.initEditSkinnedVolume()

  def validateSkinningPage(self, validateSections = True):
    if validateSections:
      self.validateVolumeSkinning()
      self.validateEditSkinnedVolume()
    valid = self.get('EditSkinnedVolumeCollapsibleGroupBox').property('valid')
    self.get('NextPageToolButton').enabled = not self.isWorkflow(0) or valid

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
  def initVolumeSkinning(self):
    self.validateVolumeSkinning()

  def validateVolumeSkinning(self):
    cliNode = self.getCLINode(slicer.modules.volumeskinning)
    valid = cliNode.GetStatusString() == 'Completed'
    self.get('VolumeSkinningOutputVolumeNodeToolButton').enabled = valid
    self.get('VolumeSkinningSaveToolButton').enabled = valid
    self.get('VolumeSkinningCollapsibleGroupBox').setProperty('valid', valid)
    self.get('EditSkinnedVolumeGoToEditorPushButton').enabled = not self.isWorkflow(0) or valid

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
      self.get('VolumeSkinningApplyPushButton').enabled = False
      cliNode.Cancel()

  def onVolumeSkinningCLIModified(self, cliNode, event):
    if cliNode.GetStatusString() == 'Completed':
      self.validateVolumeSkinning()
    if not cliNode.IsBusy():
      self.get('VolumeSkinningApplyPushButton').setChecked(False)
      self.get('VolumeSkinningApplyPushButton').enabled = True
      print 'VolumeSkinning %s' % cliNode.GetStatusString()
      self.removeObservers(self.onVolumeSkinningCLIModified)

  def loadSkinningInputVolumeNode(self):
    self.loadLabelmap('Input volume', self.get('VolumeSkinningInputVolumeNodeComboBox'))

  def loadSkinningVolumeNode(self):
    loadedNode = self.loadLabelmapFile('Skinned volume', 'VolumeFile',
                                       self.get('VolumeSkinningOutputVolumeNodeComboBox'))
    if loadedNode != None:
      cliNode = self.getCLINode(slicer.modules.volumeskinning)
      self.observeCLINode(cliNode, self.onVolumeSkinningCLIModified)
      cliNode.SetStatus(slicer.vtkMRMLCommandLineModuleNode.Completed)

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

    nodeName = '%s-skinned' % node.GetName()
    skinnedNode = self.getFirstNodeByNameAndClass(nodeName, 'vtkMRMLScalarVolumeNode')
    if skinnedNode == None:
      newNode = self.get('VolumeSkinningOutputVolumeNodeComboBox').addNode()
      newNode.SetName(nodeName)
    else:
      self.get('VolumeSkinningOutputVolumeNodeComboBox').setCurrentNode(skinnedNode)

  #----------------------------------------------------------------------------
  #    b) Edit skinned volume
  def initEditSkinnedVolume(self):
    self.validateEditSkinnedVolume()

  def validateEditSkinnedVolume(self):
    skinnedVolume = self.get('EditSkinnedVolumeNodeComboBox').currentNode()
    canEdit = False
    canSave = False
    if skinnedVolume != None:
      canEdit = skinnedVolume.GetDisplayNode() != None
      canSave = canEdit and skinnedVolume.GetModifiedSinceRead()
    self.get('EditSkinnedVolumeGoToEditorPushButton').enabled = canEdit
    self.get('EditSkinnedVolumeNodeSaveToolButton').enabled = canSave
    self.get('EditSkinnedVolumeSaveToolButton').enabled = canSave
    valid = canEdit
    self.get('EditSkinnedVolumeCollapsibleGroupBox').setProperty('valid', valid)
    if valid:
      self.get('VolumeRenderInputNodeComboBox').setCurrentNode(
        self.get('EditSkinnedVolumeNodeComboBox').currentNode())
      self.get('VolumeRenderLabelsLineEdit').text = ''
    self.validateSkinningPage(validateSections = False)

  def editSkinnedVolumeParameterChanged(self, skinnedVolume = None, event = None):
    self.removeObservers(self.editSkinnedVolumeParameterChanged)
    if skinnedVolume != None:
      self.addObserver(skinnedVolume, 'ModifiedEvent', self.editSkinnedVolumeParameterChanged)
    self.validateEditSkinnedVolume()

  def loadEditSkinnedVolumeNode(self):
    loadedNode = self.loadLabelmapFile('Skinning volume', 'VolumeFile', self.get('EditSkinnedVolumeNodeComboBox'))
    if loadedNode != None:
      self.validateEditSkinnedVolume()

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
  # 6) Weights
  #----------------------------------------------------------------------------
  def initWeightsPage(self):
    self.initComputeArmatureWeight()
    self.initEvalSurfaceWeight()
    self.initMaterialReader()

  def setDefaultPath(self, *args):
    defaultName = 'weights-%sx' % self.get('ComputeArmatureWeightScaleFactorSpinBox').value
    currentNode = self.get('ComputeArmatureWeightInputVolumeNodeComboBox').currentNode()
    if currentNode != None:
      defaultName = '%s-%s' % (currentNode.GetName(), defaultName)
    defaultPath = qt.QDir.home().absoluteFilePath(defaultName)
    self.get('ComputeArmatureWeightOutputPathLineEdit').setCurrentPath(defaultPath)
    # observe the input volume node in case its name is changed
    self.removeObservers(self.setDefaultPath)
    self.addObserver(currentNode, 'ModifiedEvent', self.setDefaultPath)

  def validateWeightsPage(self, validateSections = True):
    if validateSections:
      self.validateComputeArmatureWeight()
      self.validateEvalSurfaceWeight()
      self.validateMaterialReader()
    valid = self.get('MaterialReaderCollapsibleGroupBox').property('valid')
    self.get('NextPageToolButton').enabled = not self.isWorkflow(0) or valid

  def openWeightsPage(self):
    pass

  #----------------------------------------------------------------------------
  # a) Compute Armature Weight
  def initComputeArmatureWeight(self):
    self.validateComputeArmatureWeight()

  def validateComputeArmatureWeight(self):
    cliNode = self.getCLINode(slicer.modules.computearmatureweight)
    valid = cliNode.GetStatusString() == 'Completed'
    self.get('ComputeArmatureWeightCollapsibleGroupBox').setProperty('valid', valid)

    enableEvalWeight = not self.isWorkflow(0) or valid
    self.get('EvalSurfaceWeightCollapsibleGroupBox').collapsed = not enableEvalWeight
    self.get('EvalSurfaceWeightCollapsibleGroupBox').setEnabled(enableEvalWeight)

    self.validateWeightsPage(validateSections = False)

  def computeArmatureWeightParameters(self):
    parameters = {}
    parameters["RestLabelmap"] = self.get('ComputeArmatureWeightInputVolumeNodeComboBox').currentNode()
    parameters["ArmaturePoly"] = self.get('ComputeArmatureWeightAmartureNodeComboBox').currentNode()
    parameters["SkinnedVolume"] = self.get('ComputeArmatureWeightSkinnedVolumeVolumeNodeComboBox').currentNode()
    parameters["WeightDirectory"] = str(self.get('ComputeArmatureWeightOutputPathLineEdit').currentPath)
    parameters["BackgroundValue"] = self.get('ComputeArmatureWeightBackgroundSpinBox').value
    parameters["BoneLabel"] = self.get('ComputeArmatureWeightBoneSpinBox').value
    parameters["Padding"] = self.get('ComputeArmatureWeightPaddingSpinBox').value
    parameters["ScaleFactor"] = self.get('ComputeArmatureWeightScaleFactorSpinBox').value
    parameters["MaximumParenthoodDistance"] = '4'
    #parameters["FirstEdge"] = '0'
    #parameters["LastEdge"] = '-1'
    #parameters["BinaryWeight"] = False
    #parameters["SmoothingIteration"] = '10'
    #parameters["Debug"] = False
    parameters["RunSequential"] = True
    return parameters

  def runComputeArmatureWeight(self, run):
    if run:
      cliNode = self.getCLINode(slicer.modules.computearmatureweight)
      parameters = self.computeArmatureWeightParameters()
      if not qt.QDir(parameters["WeightDirectory"]).exists():
        answer = qt.QMessageBox.question(0, 'Create directory ?',
          'The path %s does not exist, do you want to create it ?' % parameters["WeightDirectory"],
          qt.QMessageBox.Yes | qt.QMessageBox.No | qt.QMessageBox.Cancel)
        if answer == qt.QMessageBox.Yes:
          qt.QDir(parameters["WeightDirectory"]).mkpath(parameters["WeightDirectory"])
        else:
          self.get('ComputeArmatureWeightApplyPushButton').setChecked(False)
          return
      self.get('ComputeArmatureWeightApplyPushButton').setChecked(True)
      self.observeCLINode(cliNode, self.onComputeArmatureWeightCLIModified)
      cliNode = slicer.cli.run(slicer.modules.computearmatureweight, cliNode, parameters, wait_for_completion = False)
    else:
      cliNode = self.observer(self.StatusModifiedEvent, self.onComputeArmatureWeightCLIModified)
      self.get('ComputeArmatureWeightApplyPushButton').enabled = False
      cliNode.Cancel()

  def onComputeArmatureWeightCLIModified(self, cliNode, event):
    if cliNode.GetStatusString() == 'Completed':
      # add path if not already added (bug fixed in CTK #b277f5d4)
      if self.get('ComputeArmatureWeightOutputPathLineEdit').findChild('QComboBox').findText(
        self.get('ComputeArmatureWeightOutputPathLineEdit').currentPath) == -1:
        self.get('ComputeArmatureWeightOutputPathLineEdit').addCurrentPathToHistory()
      self.validateComputeArmatureWeight()
    if not cliNode.IsBusy():
      self.get('ComputeArmatureWeightApplyPushButton').setChecked(False)
      self.get('ComputeArmatureWeightApplyPushButton').enabled = True
      print 'ComputeArmatureWeight %s' % cliNode.GetStatusString()
      self.removeObservers(self.onComputeArmatureWeightCLIModified)

  def openComputeArmatureWeightModule(self):
    self.openModule('ComputeArmatureWeight')

    cliNode = self.getCLINode(slicer.modules.computearmatureweight)
    parameters = self.computeArmatureWeightParameters()
    slicer.cli.setNodeParameters(cliNode, parameters)

  #----------------------------------------------------------------------------
  # b) Eval Weight
  def initEvalSurfaceWeight(self):
    self.validateEvalSurfaceWeight()

  def validateEvalSurfaceWeight(self):
    cliNode = self.getCLINode(slicer.modules.evalsurfaceweight)
    valid = cliNode.GetStatusString() == 'Completed'
    self.get('EvalSurfaceWeightCollapsibleGroupBox').setProperty('valid', valid)
    self.get('EvalSurfaceWeightOutputNodeToolButton').enabled = valid
    self.get('EvalSurfaceWeightSaveToolButton').enabled = valid

    enableMaterialReader = not self.isWorkflow(0) or valid
    self.get('MaterialReaderCollapsibleGroupBox').collapsed = not enableMaterialReader
    self.get('MaterialReaderCollapsibleGroupBox').setEnabled(enableMaterialReader)

    self.validateWeightsPage(validateSections = False)

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
      self.get('EvalSurfaceWeightApplyPushButton').enabled = False
      cliNode.Cancel()

  def onEvalSurfaceWeightCLIModified(self, cliNode, event):
    if cliNode.GetStatusString() == 'Completed':
      self.validateEvalSurfaceWeight()

    if not cliNode.IsBusy():
      self.get('EvalSurfaceWeightApplyPushButton').setChecked(False)
      self.get('EvalSurfaceWeightApplyPushButton').enabled = True
      print 'EvalSurfaceWeight %s' % cliNode.GetStatusString()
      self.removeObservers(self.onEvalSurfaceWeightCLIModified)

  def loadEvalSurfaceWeightInputNode(self):
    self.loadFile('Model to eval', 'ModelFile', self.get('EvalSurfaceWeightInputNodeComboBox'))

  def loadEvalSurfaceWeightOutputNode(self):
    loadedNode = self.loadFile('Evaluated Model', 'ModelFile', self.get('EvalSurfaceWeightOutputNodeComboBox'))
    if loadedNode != None:
      cliNode = self.getCLINode(slicer.modules.evalsurfaceweight)
      self.observeCLINode(cliNode, self.onEvalSurfaceWeightCLIModified)
      cliNode.SetStatus(slicer.vtkMRMLCommandLineModuleNode.Completed)

  def saveEvalSurfaceWeightOutputNode(self):
    self.saveFile('Evaluated Model', 'ModelFile', '.vtk', self.get('EvalSurfaceWeightOutputNodeComboBox'))

  def openEvalSurfaceWeight(self):
    self.openModule('EvalSurfaceWeight')

    cliNode = self.getCLINode(slicer.modules.evalweight)
    parameters = self.evalWeightParameters()
    slicer.cli.setNodeParameters(cliNode, parameters)

  #----------------------------------------------------------------------------
  # c) Material Reader
  def initMaterialReader(self):
    self.validateMaterialReader()

  def validateMaterialReader(self):
    cliNode = self.getCLINode(slicer.modules.materialpropertyreader)
    valid = cliNode.GetStatusString() == 'Completed'
    self.get('MaterialReaderOutputMeshNodeToolButton').enabled = valid
    self.get('MaterialReaderSaveToolButton').enabled = valid
    self.get('MaterialReaderCollapsibleGroupBox').setProperty('valid', valid)

    self.validateWeightsPage(validateSections = False)

  def materialReaderParameters(self):
    parameters = {}
    parameters["MeshPoly"] = self.get('MaterialReaderInputMeshNodeComboBox').currentNode()
    parameters["MaterialFile"] = self.get('MaterialReaderFileLineEdit').currentPath
    parameters["OutputMesh"] = self.get('MaterialReaderOutputMeshNodeComboBox').currentNode()
    return parameters

  def runMaterialReader(self, run):
    if run:
      cliNode = self.getCLINode(slicer.modules.materialpropertyreader)
      parameters = self.materialReaderParameters()
      self.get('MaterialReaderApplyPushButton').setChecked(True)
      self.observeCLINode(cliNode, self.onMaterialPropertyCLIModified)
      cliNode = slicer.cli.run(slicer.modules.materialpropertyreader, cliNode, parameters, wait_for_completion = False)
    else:
      cliNode = self.observer(self.StatusModifiedEvent, self.onMaterialPropertyCLIModified)
      self.get('MaterialReaderApplyPushButton').enabled = False
      cliNode.Cancel()

  def onMaterialPropertyCLIModified(self, cliNode, event):
    if cliNode.GetStatusString() == 'Completed':
      # Hide result
      outputMesh = self.get('MaterialReaderOutputMeshNodeComboBox').currentNode()
      displayNode = outputMesh.GetDisplayNode()
      if displayNode != None:
        displayNode.SetVisibility(False)

      self.validateMaterialReader()
    if not cliNode.IsBusy():
      self.get('MaterialReaderApplyPushButton').setChecked(False)
      self.get('MaterialReaderApplyPushButton').enabled = True
      print 'Material Property Reader %s' % cliNode.GetStatusString()
      self.removeObservers(self.onMaterialPropertyCLIModified)

  def loadMaterialReaderInputMeshNode(self):
    self.loadFile('Input tetrahedral mesh', 'ModelFile', self.get('MaterialReaderInputMeshNodeComboBox'))

  def loadMaterialReaderMeshNode(self):
    loadedNode = self.loadFile('Tetrahedral mesh with material properties', 'ModelFile', self.get('MaterialReaderOutputMeshNodeComboBox'))
    if loadedNode != None:
      cliNode = self.getCLINode(slicer.modules.materialpropertyreader)
      self.observeCLINode(cliNode, self.onMaterialPropertyCLIModified)
      cliNode.SetStatus(slicer.vtkMRMLCommandLineModuleNode.Completed)


  def saveMaterialReaderMeshNode(self):
    self.saveFile('Tetrahedral mesh with material properties', 'ModelFile', '.vtk', self.get('MaterialReaderOutputMeshNodeComboBox'))

  def openMaterialReaderModule(self):
    self.openModule('MaterialPropertyReader')

    cliNode = self.getCLINode(slicer.modules.materialpropertyreader)
    parameters = self.materialReaderParameters()
    slicer.cli.setNodeParameters(cliNode, parameters)

  def setMaterialReaderDefaultPath(self):
    weightPath = self.get('ComputeArmatureWeightOutputPathLineEdit').currentPath
    weightDir = qt.QDir(weightPath)
    weightDir.cdUp()
    defaultPath = weightDir.absoluteFilePath('materials.txt')
    self.get('MaterialReaderFileLineEdit').setCurrentPath(defaultPath)

  #----------------------------------------------------------------------------
  # 7) Pose Armature & Pose surface
  #----------------------------------------------------------------------------
  def initPoseArmaturePage(self):
    self.initPoseArmature()
    self.initSimulatePose()

  def validatePoseArmaturePage(self, validateSections = True):
    if validateSections:
      self.validatePoseArmature()
      self.validateSimulatePose()
    valid = self.get('SimulatePoseCollapsibleGroupBox').property('valid')
    self.get('NextPageToolButton').enabled = not self.isWorkflow(0) or valid

  def openPoseArmaturePage(self):
    # Create output if necessary
    if not self.simulatePoseCreateOutputConnected:
      self.get('SimulatePoseInputNodeComboBox').connect('currentNodeChanged(vtkMRMLNode*)', self.createOutputSimulatePose)
      self.simulatePoseCreateOutputConnected = True
    self.createOutputSimulatePose(self.get('SimulatePoseInputNodeComboBox').currentNode())

    armatureLogic = slicer.modules.armatures.logic()
    if armatureLogic != None:
      armatureLogic.SetActiveArmatureWidgetState(3) # 3 is Pose

    slicer.app.layoutManager().setLayout(slicer.vtkMRMLLayoutNode.SlicerLayoutOneUp3DView)

  #----------------------------------------------------------------------------
  # a) Pose Armature
  def initPoseArmature(self):
    self.validatePoseArmature()

  def validatePoseArmature(self):
    valid = self.get('PoseArmatureArmatureNodeComboBox').currentNode() != None
    self.get('PoseArmaturesCollapsibleGroupBox').setProperty('valid', valid)
    self.get('SimulatePoseApplyPushButton').enabled = not self.isWorkflow(0) or valid

  def setPoseArmatureModelNode(self, armatureNode):
    if armatureNode == None:
      return
    #Delayed event if the model isn't set yet
    self.addObserver(armatureNode, 'ModifiedEvent', self.onArmatureHierarchyModified)
    self.onArmatureHierarchyModified(armatureNode)

  def onArmatureHierarchyModified(self, node, event = None):
    if not node:
      return

    modelNode = node.GetAssociatedNode()
    if not modelNode:
      return

    self.get('SimulatePoseArmatureInputNodeComboBox').setCurrentNode(modelNode)
    self.removeObservers(self.onArmatureHierarchyModified)

    armatureLogic = slicer.modules.armatures.logic()
    if armatureLogic != None and self.WorkflowWidget.currentIndex == 4:
      armatureLogic.SetActiveArmature(node)
      armatureLogic.SetActiveArmatureWidgetState(3) # 3 is Pose
    self.validatePoseArmature()

  def savePoseArmatureArmatureNode(self):
    armatureNode = self.get('PoseArmatureArmatureNodeComboBox').currentNode()
    modelNode = armatureNode.GetAssociatedNode()
    self.saveNode('Armature', 'ModelFile', '.vtk', modelNode)

  def openPosedArmatureModule(self):
    self.openModule('Armatures')

  #----------------------------------------------------------------------------
  # b) Simulate Pose
  def initSimulatePose(self):
    self.validateSimulatePose()

  def validateSimulatePose(self):
    cliNode = self.getCLINode(slicer.modules.simulatepose)
    valid = cliNode.GetStatusString() == 'Completed'
    self.get('SimulatePoseOutputNodeToolButton').enabled = True
    self.get('SimulatePoseCollapsibleGroupBox').setProperty('valid', valid)
    self.validatePoseArmaturePage(validateSections = False)

  def simulatePoseParameters(self):
    # Setup CLI node on input changed or apply changed
    parameters = {}
    parameters["ArmaturePoly"] = self.get('SimulatePoseArmatureInputNodeComboBox').currentNode()
    parameters["InputTetMesh"] = self.get('SimulatePoseInputNodeComboBox').currentNode()
    parameters["OutputTetMesh"] = self.get('SimulatePoseOutputNodeComboBox').currentNode()
    parameters["InputSurface"] = self.get('SimulatePoseSkinComboBox').currentNode()
    parameters["EnableCollision"] = self.get('SimulatePoseCollisionCheckBox').isChecked()
    return parameters

  def runSimulatePose(self, run):
    if run:
      cliNode = self.getCLINode(slicer.modules.simulatepose)
      parameters = self.simulatePoseParameters()
      slicer.cli.setNodeParameters(cliNode, parameters)
      self.get('SimulatePoseApplyPushButton').setChecked(True)
      self.observeCLINode(cliNode, self.onSimulatePoseCLIModified)
      cliNode = slicer.cli.run(slicer.modules.simulatepose, cliNode, parameters, wait_for_completion = False)
    else:
      cliNode = self.observer(self.StatusModifiedEvent, self.onSimulatePoseCLIModified)
      self.get('SimulatePoseApplyPushButton').enabled = False
      if cliNode != None:
        cliNode.Cancel()

  def onSimulatePoseCLIModified(self, cliNode, event):
    if cliNode.GetStatusString() == 'Completed':
      if self.get('SimulatePoseInputNodeComboBox').currentNode() != self.get('SimulatePoseOutputNodeComboBox').currentNode():
        self.get('SimulatePoseInputNodeComboBox').currentNode().GetDisplayNode().SetVisibility(0)
        self.get('SimulatePoseOutputNodeComboBox').currentNode().GetDisplayNode().SetOpacity(
          self.get('SimulatePoseInputNodeComboBox').currentNode().GetDisplayNode().GetOpacity())
        self.get('SimulatePoseOutputNodeComboBox').currentNode().GetDisplayNode().SetColor(
          self.get('SimulatePoseInputNodeComboBox').currentNode().GetDisplayNode().GetColor())
          
      self.validateSimulatePose()
    if not cliNode.IsBusy():
      self.get('SimulatePoseApplyPushButton').setChecked(False)
      self.get('SimulatePoseApplyPushButton').enabled = True
      print 'Simulate Pose %s' % cliNode.GetStatusString()
      self.removeObservers(self.onSimulatePoseCLIModified)

  def loadSimulatePoseInputNode(self):
    self.loadFile('Model to pose', 'ModelFile', self.get('SimulatePoseInputNodeComboBox'))

  def loadSimulatePoseOutputNode(self):
    loadedNode = self.loadFile('Posed model', 'ModelFile', self.get('SimulatePoseOutputNodeComboBox'))
    if loadedNode != None:
      cliNode = self.getCLINode(slicer.modules.simulatepose)
      self.observeCLINode(cliNode, self.onSimulatePoseCLIModified)
      cliNode.SetStatus(slicer.vtkMRMLCommandLineModuleNode.Completed)


  def saveSimulatePoseOutputNode(self):
    self.saveFile('Posed model', 'ModelFile', '.vtk', self.get('SimulatePoseOutputNodeComboBox'))

  def openSimulatePoseModule(self):
    self.openModule('SimulatePose')

    cliNode = self.getCLINode(slicer.modules.simulatepose)
    parameters = self.SimulatePoseParameterss()
    slicer.cli.setNodeParameters(cliNode, parameters)

  def setWeightDirectory(self, dir):
    self.get('EvalSurfaceWeightWeightPathLineEdit').currentPath = dir

  def createOutputSimulatePose(self, node):
    if node == None:
      return

    nodeName = '%s-posed' % node.GetName()
    posedNode = self.getFirstNodeByNameAndClass(nodeName, 'vtkMRMLModelNode')
    if posedNode == None:
      newNode = self.get('SimulatePoseOutputNodeComboBox').addNode()
      newNode.SetName(nodeName)
    else:
      self.get('SimulatePoseOutputNodeComboBox').setCurrentNode(posedNode)

  # =================== END ==============
  def get(self, objectName):
    return self.findWidget(self.widget, objectName)

  def getChildren(self, object):
    '''Return the list of the children and grand children of a Qt object'''
    children = object.children()
    allChildren = list(children)
    for child in children:
      allChildren.extend( self.getChildren(child) )
    return allChildren

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
    if object == None:
      return
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
    scene if needed. Return None in the case of scripted module.
    """
    cliNode = slicer.mrmlScene.GetFirstNodeByName(cliModule.title)
    # Also check path to make sure the CLI isn't a scripted module
    if (cliNode == None) and ('qt-scripted-modules' not in cliModule.path):
      cliNode = slicer.cli.createNode(cliModule)
      cliNode.SetName(cliModule.title)
    return cliNode

  def loadLabelmapFile(self, title, fileType, nodeComboBox):
    volumeNode = self.loadFile(title, fileType, nodeComboBox)
    if volumeNode != None:
      volumesLogic = slicer.modules.volumes.logic()
      volumesLogic.SetVolumeAsLabelMap(volumeNode, 1)
      nodeComboBox.setCurrentNode(volumeNode)
    return volumeNode

  def loadLabelmap(self, title, nodeComboBox):
    volumeNode = self.loadLabelmapFile(title, 'VolumeFile', nodeComboBox)
    self.applyLabelmap(volumeNode)
    return volumeNode

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
    import imp, sys, os, slicer, qt

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
    parent = self.widget.parent()
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
