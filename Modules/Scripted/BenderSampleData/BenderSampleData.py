import os
from __main__ import slicer
import qt, ctk

from SampleData import *

#
# BenderSampleData
#
# To add new data, edit the ui file to add case to the tree.
# and fill the corresponding information in the BenderSampleDataLogic
# download data dictionnary.

class BenderSampleData:
  def __init__(self, parent):
    import string
    parent.title = "Bender Sample Data"
    parent.categories = ["Informatics"]
    parent.dependencies = ["SampleData"]
    parent.contributors = ["Johan Andruejol (Kitware), Julien Finet (Kitware)"]
    parent.helpText = string.Template("""
      This module can be used to download data for working with in Bender. The data is downloaded into the application
      cache so it will be available directly next time.
      Use of this module requires an active network connection.
      See <a href=\"$a/Documentation/$b.$c/Modules/SampleData\">$a/Documentation/$b.$c/Modules/SampleData</a> for more information.
      """).substitute({ 'a':'http://public.kitware.com/Wiki/Bender', 'b':1, 'c':1 })
    parent.acknowledgementText = """
    This work is supported by Air Force Research Laboratory (AFRL)
    """
    self.parent = parent

    # Look for sample's data action to replace it
    self.triggerReplaceMenu()

  def addMenu(self):
    try:
      slicer.modules.sampledata.hidden = True
      slicer.modules.sampledata.action().visible = False
    except:
      pass

    actionIcon = self.parent.icon
    a = qt.QAction(actionIcon, 'Download Sample Data', slicer.util.mainWindow())
    a.setToolTip('Go to the Bender SampleData module to download data from the network')
    a.connect('triggered()', self.select)

    menuFile = slicer.util.lookupTopLevelWidget('menuFile')
    if menuFile:
      for action in menuFile.actions():
        if action.text == 'Save':
          menuFile.insertAction(action,a)

  def select(self):
    m = slicer.util.mainWindow()
    m.moduleSelector().selectModule('BenderSampleData')

  def triggerReplaceMenu(self):
    if not slicer.app.commandOptions().noMainWindow:
      qt.QTimer.singleShot(0, self.addMenu);

#
# SampleData widget
#

class BenderSampleDataWidget:

  def __init__(self, parent=None):
    self.observerTags = []

    if not parent:
      self.parent = slicer.qMRMLWidget()
      self.parent.setLayout(qt.QVBoxLayout())
      self.parent.setMRMLScene(slicer.mrmlScene)
      self.layout = self.parent.layout()
      self.setup()
      self.parent.show()
    else:
      self.parent = parent
      self.layout = parent.layout()

  def setup(self):

    # UI setup
    loader = qt.QUiLoader()
    moduleName = 'BenderSampleData'
    scriptedModulesPath = eval('slicer.modules.%s.path' % moduleName.lower())
    scriptedModulesPath = os.path.dirname(scriptedModulesPath)
    path = os.path.join(scriptedModulesPath, 'Resources', 'UI', '%s.ui' %moduleName)

    qfile = qt.QFile(path)
    qfile.open(qt.QFile.ReadOnly)
    widget = loader.load(qfile, self.parent)
    self.layout = self.parent.layout()
    self.widget = widget;
    self.layout.addWidget(widget)

    # widget setup
    self.log = self.get('BenderSampleDataLog')
    self.logic = BenderSampleDataLogic(self.logMessage)
    self.dataTree = self.get('BenderSampleDataTree')
    self.dataTree.expandAll()
    self.downloadDirectoryPathLineEdit = self.get('DownloadDirectoryPathLineEdit')
    self.downloadDirectoryPathLineEdit.currentPath = qt.QDir.homePath()
    self.get('BenderSampleDataDownloadPushButton').connect('clicked()', self.downloadCheckedItems)

  def downloadCheckedItems(self):
    items = self.modelMatch(
      self.dataTree, self.dataTree.invisibleRootItem(), qt.Qt.CheckStateRole, qt.Qt.Checked, -1)
    if len(items) < 1:
      return
    qt.QDir().mkpath( self.downloadDirectoryPathLineEdit.currentPath )

    for item in items:
      parent = self.getTopLevelItem(item)
      if parent and item:
        self.logic.download(parent.text(0),
                            item.text(0),
                            self.downloadDirectoryPathLineEdit.currentPath)

  def getTopLevelItem(self, item):
    if not item or not item.parent():
      return item
    return self.getTopLevelItem(item.parent())

  def logMessage(self,message):
    self.log.insertHtml(message)
    self.log.insertPlainText('\n')
    self.log.ensureCursorVisible()
    self.log.repaint()
    slicer.app.processEvents(qt.QEventLoop.ExcludeUserInputEvents)

  ### === Convenience python widget methods === ###
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
  def modelMatch(self, view, startItem, role, value, hits):
    res = []
    column = 0
    if startItem.data(column, role) == value:
      res.append(startItem)
      hits = hits - 1
    if hits == 0:
      return res
    for childRow in range(0, startItem.childCount()):
      childItem = startItem.child(childRow)
      childRes = self.modelMatch(view, childItem, role, value, hits)
      res.extend(childRes)
    return res

#
# Bender Sample Data Logic
#

class BenderSampleDataLogic( SampleDataLogic ):
  """Download the selected items. Use the logic of the sample data module."""
  def __init__(self, logMessage=None):
    if logMessage:
      self.logMessage = logMessage

    self.downloadData = (
      {
      'Common' :
        {
        'Volume' : ['man-arm-2mm', 'LabelmapFile', 'http://packages.kitware.com/download/item/3614/man-arm-2mm.mha', 'man-arm-2mm.mha'],
        'Tissues' : ['Tissues-v1.1.0', 'ColorTableFile', 'http://packages.kitware.com/download/item/3615/Tissues-v1.1.0.txt', 'Tissues-v1.1.0.txt'],
        'Armature' : ['man-arm-2mm-armature', 'ArmatureFile', 'http://packages.kitware.com/download/item/4986/man-arm-2mm-armature.vtk', 'man-arm-2mm-armature.vtk'],
        'Materials' : ['Materials-v2.0.0', 'ColorTableFile', 'http://packages.kitware.com/download/item/4964/Materials-v2.0.0.txt', 'Materials-v2.0.0.txt'],
        },
      'Simple Workflow' :
        {
        'Merged volume' : ['man-arm-2mm-merged', 'LabelmapFile', 'http://packages.kitware.com/download/item/4970/man-arm-2mm-merged.nrrd', 'man-arm-2mm-merged.nrrd'],
        'Bones' : ['man-arm-2mm-Bones', 'ModelFile', 'http://packages.kitware.com/download/item/4969/man-arm-2mm-Bones.vtk', 'man-arm-2mm-Bones.vtk'],
        'Skin' : ['man-arm-2mm-Skin', 'ModelFile', 'http://packages.kitware.com/download/item/4966/man-arm-2mm-Skin.vtk', 'man-arm-2mm-Skin.vtk'],
        'Skinned volume' : ['man-arm-2mm-merged-skinned', 'LabelmapFile', 'http://packages.kitware.com/download/item/4971/man-arm-2mm-merged-skinned.mha', 'man-arm-2mm-merged-skinned.mha'],
        'Posed volume' : ['man-arm-2mm-posed', 'LabelmapFile', 'http://packages.kitware.com/download/item/4965/man-arm-2mm-posed.mha', 'man-arm-2mm-posed.mha'],
        },
      'FEM Workflow' :
        {
        'Merged volume' : ['man-arm-2mm-merged', 'LabelmapFile', 'http://packages.kitware.com/download/item/4987/man-arm-2mm-merged.mha', 'man-arm-2mm-merged.mha'],
        'Padded volume' : ['man-arm-2mm-merged-padded', 'LabelmapFile', 'http://packages.kitware.com/download/item/4976/man-arm-2mm-merged-padded.nrrd', 'man-arm-2mm-merged-padded.nrrd'],
        'Tetrahedral mesh' : ['man-arm-2mm-padded-tetmesh', 'ModelFile', 'http://packages.kitware.com/download/item/4983/man-arm-2mm-merged-padded-tetmesh.vtk', 'man-arm-2mm-padded-tetmesh.vtk'],
        'Bones' : ['man-arm-2mm-merged-padded-tetmesh-bones', 'ModelFile', 'http://packages.kitware.com/download/item/4978/man-arm-2mm-merged-padded-tetmesh-bones.vtk', 'man-arm-2mm-merged-padded-tetmesh-bones.vtk'],
        'Skin' : ['man-arm-2mm-merged-padded-skin', 'ModelFile', 'http://packages.kitware.com/download/item/4977/man-arm-2mm-merged-padded-skin.vtk', 'man-arm-2mm-merged-padded-skin.vtk'],
        'Skinned volume' : ['man-arm-2mm-merged-padded-skinned', 'LabelmapFile', 'http://packages.kitware.com/download/item/4984/man-arm-2mm-merged-padded-skinned.nrrd', 'man-arm-2mm-merged-padded-skinned.nrrd'],
        'Posed mesh' : ['man-arm-2mm-padded-tetmesh-posed.vtk', 'ModelFile', '', 'man-arm-2mm-padded-tetmesh-posed.vtk'],
        },
      # Add the tree item's download data here
      })

  def download(self, case, data, path):
    filePath = self.downloadFile(self.downloadData[case][data][2],
                                 path, self.downloadData[case][data][3])

    #properties = {'name' : self.downloadData[case][data][0]}
    #filetype = self.downloadData[case][data][1]
    #if filetype == 'LabelmapFile':
    #filetype = 'VolumeFile'
    #properties['labelmap'] = True

    #success, node = slicer.util.loadNodeFromFile(filePath, filetype, properties, returnNode=True)
    #if success:
    #  self.logMessage('<b>Load finished</b>\n')
    #else:
    #  self.logMessage('<b><font color="red">\tLoad failed!</font></b>\n')
    #return node

  def downloadData(self):
    return self.downloadData
