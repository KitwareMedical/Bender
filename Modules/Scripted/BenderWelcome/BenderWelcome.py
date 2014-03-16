import os
from __main__ import slicer
import qt, ctk

#
# BenderWelcome
#

class BenderWelcome:
  def __init__(self, parent):
    import string
    parent.title = "Bender Welcome"
    parent.categories = [""]
    parent.contributors = ["Johan Andruejol (Kitware)"]
    parent.helpText = """"""
    parent.acknowledgementText = """
    This work is supported by Air Force Research Laboratory (AFRL)
    """
    self.parent = parent

#
# Welcome widget
#

class BenderWelcomeWidget:

  def __init__(self, parent=None):
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

    settings = slicer.app.settings()
    if not settings.contains('Bender/InputDirectory'):
      benderDefaultDir = qt.QDir.home().absoluteFilePath('bender')
      settings.setValue('Bender/InputDirectory', benderDefaultDir)
      settings.setValue('Bender/OutputDirectory', benderDefaultDir)

  def setup(self):

    # UI setup
    loader = qt.QUiLoader()
    moduleName = 'BenderWelcome'
    scriptedModulesPath = eval('slicer.modules.%s.path' % moduleName.lower())
    scriptedModulesPath = os.path.dirname(scriptedModulesPath)
    path = os.path.join(scriptedModulesPath, 'Resources', 'UI', '%s.ui' %moduleName)

    qfile = qt.QFile(path)
    qfile.open(qt.QFile.ReadOnly)
    widget = loader.load(qfile, self.parent)
    self.layout = self.parent.layout()
    self.widget = widget;
    self.layout.addWidget(widget)

    # GoTo buttons
    self.get('GoToSampleDataButton').icon = self.get('GoToSampleDataButton').style().standardIcon(qt.QStyle.SP_ArrowDown)
    self.get('GoToSampleDataButton').setIconSize(qt.QSize(45,45))
    self.get('GoToWorkflowMenuButton').icon = self.get('GoToWorkflowMenuButton').style().standardIcon(qt.QStyle.SP_ArrowRight)
    self.get('GoToWorkflowMenuButton').setIconSize(qt.QSize(45,45))

    self.get('GoToSampleDataButton').connect('clicked()', self.goToSampleDataModule)
    self.get('GoToWorkflowMenuButton').connect('clicked()', self.goToFEMWorkflowModule)
    self.get('GoToLabelsStatisticsButton').connect('clicked()', self.goToLabelsStatisticsModule)

    workflowMenu = qt.QMenu(self.get('GoToWorkflowMenuButton'))
    workflowMenu.addAction('Simple Workflow').connect('triggered()', self.goToSimpleWorkflowModule)
    workflowMenu.addAction('FEM Workflow').connect('triggered()', self.goToFEMWorkflowModule)
    workflowMenu.addAction('Labels Statistics').connect('triggered()', self.goToLabelsStatisticsModule)
    self.get('GoToWorkflowMenuButton').setMenu(workflowMenu)

    # Label image setup
    label = self.get('BenderMarchOfProgressLabel')
    path = os.path.join(scriptedModulesPath, 'Resources', 'Images', '%s.png' %moduleName)
    image = qt.QPixmap(path)

    # Scale image to current size
    self.filter = BenderWelcomeEventFilter(label, image, self.widget)
    label.setPixmap(self.filter.scaleImage(image, label.size))

    # Install event filter on the image so it resizes properly
    label.installEventFilter(self.filter)

    # Make the background transparent for the ctkFittedTextBrowser
    #textBrowsers = [
    #  self.get('WhatBenderFittedTextBrowser'),
    #  self.get('AcknowledgementsFittedTextBrowser')
    #  ]
    #for w in textBrowsers:
    #  p = w.palette
    #  p.setColor(p.Base, qt.QColor(0,0,0,0));
    #  w.setPalette(p);

    # Connections

  def goToSampleDataModule(self):
    slicer.util.selectModule('BenderSampleData')

  def goToSimpleWorkflowModule(self):
    slicer.util.selectModule('SimpleWorkflow')

  def goToFEMWorkflowModule(self):
    slicer.util.selectModule('Workflow')

  def goToLabelsStatisticsModule(self):
    slicer.util.selectModule('BenderLabelStatistics')


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

class BenderWelcomeEventFilter(qt.QObject):
  '''Filters resize events to make sure the image resizes properly.'''
  def __init__(self, label, image, parent):
    super(BenderWelcomeEventFilter, self).__init__(parent)
    self.label = label
    self.originalImage = image

  def eventFilter(self, obj, event):
    if event.type() == qt.QEvent.Resize:
      scaledPixmap = self.scaleImage(self.originalImage, event.size())
      self.label.setPixmap(scaledPixmap)
      return True
    return False

  def scaleImage(self, image, size):
    return image.scaled(size.width(), size.height(), 1) #1 is Qt.KeepAspectRatio
