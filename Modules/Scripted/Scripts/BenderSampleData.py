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
    #parent.dependencies = ["SampleData"]
    parent.contributors = ["Johan Andruejol (Kitware)"]
    parent.helpText = string.Template("""
      This module can be used to download data for working with in Bender. The data is downloaded into the application
      cache so it will be available directly next time.
      Use of this module requires an active network connection.
      See <a href=\"$a/Documentation/$b.$c/Modules/BenderSampleData\">$a/Documentation/$b.$c/Modules/SampleData</a> for more information.
      """).substitute({ 'a':'http://public.kitware.com/Wiki/Bender', 'b':1, 'c':0 })
    parent.acknowledgementText = """
    This work is supported by Air Force Research Laboratory (AFRL)
    """
    self.parent = parent

    # Look for sample's data action to replace it
    self.triggerReplaceMenu()

  def addMenu(self):
    actionIcon = self.parent.icon
    a = qt.QAction(actionIcon, 'Download Sample Data', slicer.util.mainWindow())
    a.setToolTip('Go to the BenderSampleData module to download data from the network')
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
    self.get('BenderSampleDataDownloadPushButton').connect('clicked()', self.downloadSelectedItems)

  def downloadSelectedItems(self):
    items = self.dataTree.selectedItems()
    if len(items) < 1:
      return

    for item in items:
      parent = self.getTopLevelItem(item)
      if parent and item:
        self.logic.download(parent.text(0), item.text(0))

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

#
# Bender Sample Data Logic
#

class BenderSampleDataLogic( SampleDataLogic ):
  """Download the selected items. Use the logic of the sample data module."""
  def __init__(self, logMessage=None):
    SampleDataLogic.__init__(self, logMessage)

    self.downloadData = (
      { 'Man 2mm Arm' :
        {
        'Volume' : ['man-arm-2mm', 'LabelmapFile', 'http://packages.kitware.com/download/item/3614/man-arm-2mm.mha', 'man-arm-2mm.mha'],
        'Color table' : ['Tissues-v1.1.0', 'ColorTableFile', 'http://packages.kitware.com/download/item/3615/Tissues-v1.1.0.txt', 'Tissues-v1.1.0.txt'],
        'Armature' : ['man-arm-2mm-armature', 'ArmatureFile', 'http://packages.kitware.com/download/item/3616/man-arm-2mm-armature.vtk', 'man-arm-2mm-armature.vtk'],
        'Bones' : ['man-arm-2mm-Bones', 'ModelFile', 'http://packages.kitware.com/download/item/3959/man-arm-2mm-Bones.vtk', 'man-arm-2mm-Bones.vtk'],
        'Skin' : ['man-arm-2mm-Skin', 'ModelFile', 'http://packages.kitware.com/download/item/3960/man-arm-2mm-Skin.vtk', 'man-arm-2mm-Skin.vtk'],
        'Merged volume' : ['man-arm-2mm-merged', 'LabelmapFile', 'http://packages.kitware.com/download/item/3961/man-arm-2mm-merged.nrrd', 'man-arm-2mm-merged.nrrd'],
        'Skinned volume' : ['man-arm-2mm-merged-skinned', 'LabelmapFile', 'http://packages.kitware.com/download/item/3618/man-arm-2mm-merged-skinned.mha', 'man-arm-2mm-merged-skinned.mha'],
        'Posed volume' : ['man-arm-2mm-posed', 'LabelmapFile', 'http://packages.kitware.com/download/item/3619/man-arm-2mm-posed.mha', 'man-arm-2mm-posed.mha'],
        },
      # Add the tree item's download data here
      })

  def download(self, case, data):
    filePath = self.downloadFileIntoCache(self.downloadData[case][data][2], self.downloadData[case][data][3])

    properties = {'name' : self.downloadData[case][data][0]}
    filetype = self.downloadData[case][data][1]
    if filetype == 'LabelmapFile':
      filetype = 'VolumeFile'
      properties['labelmap'] = True

    success, node = slicer.util.loadNodeFromFile(filePath, filetype, properties, returnNode=True)
    if success:
      self.logMessage('<b>Load finished</b>\n')
    else:
      self.logMessage('<b><font color="red">\tLoad failed!</font></b>\n')
    return node

  def downloadData(self):
    return self.downloadData
