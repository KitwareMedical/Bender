from __main__ import vtk, qt, ctk, slicer

#
# LabelStatistics
#

class BenderLabelStatistics:
  def __init__(self, parent):
    import string
    parent.title = "Bender Label Statistics"
    parent.categories = ["Quantification"]
    parent.contributors = ["Steve Pieper (Isomics), Johan Andruejol (Kitware)"]
    parent.helpText = string.Template("""
Use this module to calculate counts and volumes for different labels of a label map plus statistics on the grayscale background volume.  Note: volumes must have same dimensions.  See <a href=\"$a/Documentation/$b.$c/Modules/LabelStatistics\">$a/Documentation/$b.$c/Modules/LabelStatistics</a> for more information.
    """).substitute({ 'a':parent.slicerWikiUrl, 'b':slicer.app.majorVersion, 'c':slicer.app.minorVersion })
    parent.acknowledgementText = """
    Supported by NA-MIC, NAC, BIRN, NCIGT, and the Slicer Community. See http://www.slicer.org for details.  Module implemented by Steve Pieper.
    """
    self.parent = parent

#
# qSlicerPythonModuleExampleWidget
#

class BenderLabelStatisticsWidget:
  def __init__(self, parent=None):
    if not parent:
      self.setup()
      self.parent.show()
    else:
      self.parent = parent

    self.logic = None
    self.grayscaleNode = None
    self.labelmapNode = None
    self.fileName = None
    self.fileDialog = None

  def setup(self):
    loader = qt.QUiLoader()
    moduleName = 'BenderLabelStatistics'
    scriptedModulesPath = eval('slicer.modules.%s.path' % moduleName.lower())
    scriptedModulesPath = os.path.dirname(scriptedModulesPath)
    path = os.path.join(scriptedModulesPath, 'Resources', 'UI', 'BenderLabelStatistics.ui')

    qfile = qt.QFile(path)
    qfile.open(qt.QFile.ReadOnly)
    widget = loader.load( qfile, self.parent )
    self.layout = self.parent.layout()
    self.widget = widget;
    self.layout.addWidget(widget)

    # input filters
    self.get('labelmapSelector').addAttribute( "vtkMRMLScalarVolumeNode", "LabelMap", "1" )

    # icons
    saveIcon = self.get('BenderLabelStatistics').style().standardIcon(qt.QStyle.SP_DialogSaveButton)
    self.get('saveButton').icon = saveIcon

    # connections
    self.get('applyButton').connect('clicked(bool)', self.computeStatistics)
    self.get('chartButton').connect('clicked()', self.onChart)
    self.get('saveButton').connect('clicked()', self.onSave)
    self.get('labelmapSelector').connect('currentNodeChanged(vtkMRMLNode*)', self.onLabelSelect)

    self.widget.setMRMLScene(slicer.mrmlScene)

  def onLabelSelect(self, node):
    self.labelmapNode = node
    self.get('applyButton').enabled = (self.labelmapNode != None)

  def computeStatistics(self, run):
    """Calculate the label statistics
    """
    if not run:
      return

    self.labelmapNode = self.get('labelmapSelector').currentNode()
    self.grayscaleNode = self.get('grayscaleSelector').currentNode()

    self.get('applyButton').setChecked(True)

    self.logic = BenderLabelStatisticsLogic(self.grayscaleNode, self.labelmapNode)
    self.populateStats()
    self.get('chartFrame').enabled = (self.labelmapNode != None)
    self.get('saveButton').enabled = (self.labelmapNode != None)

    self.get('applyButton').setChecked(False)

  def onChart(self):
    """chart the label statistics
    """
    valueToPlot = self.get('chartOption').currentText
    ignoreZero = self.get('chartIgnoreZero').checked
    self.logic.createStatsChart(self.labelmapNode, valueToPlot, ignoreZero)

  def onSave(self):
    """save the label statistics
    """
    if not self.fileDialog:
      self.fileDialog = qt.QFileDialog(self.parent)
      self.fileDialog.options = self.fileDialog.DontUseNativeDialog
      self.fileDialog.acceptMode = self.fileDialog.AcceptSave
      self.fileDialog.defaultSuffix = "csv"
      self.fileDialog.setNameFilter("Comma Separated Values (*.csv)")
      self.fileDialog.connect("fileSelected(QString)", self.onFileSelected)
    self.fileDialog.show()

  def onFileSelected(self,fileName):
    self.logic.saveStats(fileName)

  def populateStats(self):
    if not self.logic:
      return

    try:
      displayNode = self.labelmapNode.GetDisplayNode()
      colorNode = displayNode.GetColorNode()
      lut = colorNode.GetLookupTable()
    except AttributeError:
      return

    self.items = []
    self.model = qt.QStandardItemModel()
    self.get('view').setModel(self.model)
    self.get('view').verticalHeader().visible = False
    row = 0
    for i in self.logic.labelStats["Labels"]:
      color = qt.QColor()
      rgb = lut.GetTableValue(i)
      color.setRgb(rgb[0]*255,rgb[1]*255,rgb[2]*255)
      item = qt.QStandardItem()
      item.setEditable(False)
      item.setData(color,1)
      item.setToolTip(colorNode.GetColorName(i))
      self.model.setItem(row,0,item)
      self.items.append(item)
      col = 1
      for k in self.logic.keys:
        item = qt.QStandardItem()
        item.setText(str(self.logic.labelStats[i,k]))
        item.setToolTip(colorNode.GetColorName(i))
        self.model.setItem(row,col,item)
        self.items.append(item)
        col += 1
      row += 1

    self.get('view').setColumnWidth(0,30)
    self.model.setHeaderData(0,1," ")
    col = 1
    for k in self.logic.keys:
      self.get('view').setColumnWidth(col,15*len(k))
      self.model.setHeaderData(col,1,k)
      col += 1

  ### Widget methods ###

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


class BenderLabelStatisticsLogic:
  """Implement the logic to calculate label statistics.
  Nodes are passed in as arguments.
  Results are stored as 'statistics' instance variable.
  """
  
  def __init__(self, grayscaleNode, labelmapNode, fileName=None):
    self.keys = ("Index",
                 "Count",
                 "Volume (cubic millimeter)",
                 "Volume (cubic centimeter)",
                 "Minimum",
                 "Maximum",
                 "Mean",
                 "Standard deviation",
                 )
    cubicMMPerVoxel = reduce(lambda x,y: x*y, labelmapNode.GetSpacing())
    ccPerCubicMM = 0.001

    # TODO: progress and status updates
    # this->InvokeEvent(vtkLabelStatisticsLogic::StartLabelStats, (void*)"start label stats")
    
    self.labelStats = {}
    self.labelStats['Labels'] = []
   
    stataccum = vtk.vtkImageAccumulate()
    stataccum.SetInput(labelmapNode.GetImageData())
    stataccum.Update()
    lo = int(stataccum.GetMin()[0])
    hi = int(stataccum.GetMax()[0])

    for i in xrange(lo,hi+1):

      # this->SetProgress((float)i/hi);
      # std::string event_message = "Label "; std::stringstream s; s << i; event_message.append(s.str());
      # this->InvokeEvent(vtkLabelStatisticsLogic::LabelStatsOuterLoop, (void*)event_message.c_str());

      # logic copied from slicer3 LabelStatistics
      # to create the binary volume of the label
      # //logic copied from slicer2 LabelStatistics MaskStat
      # // create the binary volume of the label
      thresholder = vtk.vtkImageThreshold()
      thresholder.SetInput(labelmapNode.GetImageData())
      thresholder.SetInValue(1)
      thresholder.SetOutValue(0)
      thresholder.ReplaceOutOn()
      thresholder.ThresholdBetween(i,i)
      if grayscaleNode:
        thresholder.SetOutputScalarType(grayscaleNode.GetImageData().GetScalarType())
      else:
        thresholder.SetOutputScalarType(labelmapNode.GetImageData().GetScalarType())
      thresholder.Update()
      
      # this.InvokeEvent(vtkLabelStatisticsLogic::LabelStatsInnerLoop, (void*)"0.25");
      
      #  use vtk's statistics class with the binary labelmap as a stencil
      stencil = vtk.vtkImageToImageStencil()
      stencil.SetInput(thresholder.GetOutput())
      stencil.ThresholdBetween(1, 1)
      
      # this.InvokeEvent(vtkLabelStatisticsLogic::LabelStatsInnerLoop, (void*)"0.5")
      
      stat1 = vtk.vtkImageAccumulate()
      if grayscaleNode != None:
        stat1.SetInput(grayscaleNode.GetImageData())
      else:
        stat1.SetInput(labelmapNode.GetImageData())
      stat1.SetStencil(stencil.GetOutput())
      stat1.Update()

      # this.InvokeEvent(vtkLabelStatisticsLogic::LabelStatsInnerLoop, (void*)"0.75")

      if stat1.GetVoxelCount() > 0:
        # add an entry to the LabelStats list
        self.labelStats["Labels"].append(i)
        self.labelStats[i,"Index"] = i
        self.labelStats[i,"Count"] = stat1.GetVoxelCount()
        self.labelStats[i,"Volume (cubic millimeter)"] = self.labelStats[i,"Count"] * cubicMMPerVoxel
        self.labelStats[i,"Volume (cubic centimeter)"] = self.labelStats[i,"Volume (cubic millimeter)"] * ccPerCubicMM
        self.labelStats[i,"Minimum"] = stat1.GetMin()[0]
        self.labelStats[i,"Maximum"] = stat1.GetMax()[0]
        self.labelStats[i,"Mean"] = stat1.GetMean()[0]
        self.labelStats[i,"Standard deviation"] = stat1.GetStandardDeviation()[0]
        
        # this.InvokeEvent(vtkLabelStatisticsLogic::LabelStatsInnerLoop, (void*)"1")

    # this.InvokeEvent(vtkLabelStatisticsLogic::EndLabelStats, (void*)"end label stats")

  def createStatsChart(self, labelNode, valueToPlot, ignoreZero=False):
    """Make a MRML chart of the current stats
    """
    layoutNodes = slicer.mrmlScene.GetNodesByClass('vtkMRMLLayoutNode')
    layoutNodes.SetReferenceCount(layoutNodes.GetReferenceCount()-1)
    layoutNodes.InitTraversal()
    layoutNode = layoutNodes.GetNextItemAsObject()
    layoutNode.SetViewArrangement(slicer.vtkMRMLLayoutNode.SlicerLayoutConventionalQuantitativeView)

    chartViewNodes = slicer.mrmlScene.GetNodesByClass('vtkMRMLChartViewNode')
    chartViewNodes.SetReferenceCount(chartViewNodes.GetReferenceCount()-1)
    chartViewNodes.InitTraversal()
    chartViewNode = chartViewNodes.GetNextItemAsObject()

    arrayNode = slicer.mrmlScene.AddNode(slicer.vtkMRMLDoubleArrayNode())
    array = arrayNode.GetArray()
    samples = len(self.labelStats["Labels"])
    tuples = samples
    if ignoreZero and self.labelStats["Labels"].__contains__(0):
      tuples -= 1
    array.SetNumberOfTuples(tuples)
    tuple = 0
    for i in xrange(samples):
        index = self.labelStats["Labels"][i]
        if not (ignoreZero and index == 0):
          array.SetComponent(tuple, 0, index)
          array.SetComponent(tuple, 1, self.labelStats[index,valueToPlot])
          array.SetComponent(tuple, 2, 0)
          tuple += 1

    chartNode = slicer.mrmlScene.AddNode(slicer.vtkMRMLChartNode())
    chartNode.AddArray(valueToPlot, arrayNode.GetID())

    chartViewNode.SetChartNodeID(chartNode.GetID())

    chartNode.SetProperty('default', 'title', 'Label Statistics')
    chartNode.SetProperty('default', 'xAxisLabel', 'Label')
    chartNode.SetProperty('default', 'yAxisLabel', valueToPlot)
    chartNode.SetProperty('default', 'type', 'Bar');
    chartNode.SetProperty('default', 'xAxisType', 'categorical')
    chartNode.SetProperty('default', 'showLegend', 'off')

    # series level properties
    if labelNode.GetDisplayNode() != None and labelNode.GetDisplayNode().GetColorNode() != None:
      chartNode.SetProperty(valueToPlot, 'lookupTable', labelNode.GetDisplayNode().GetColorNodeID());


  def statsAsCSV(self):
    """
    print comma separated value file with header keys in quotes
    """
    csv = ""
    header = ""
    for k in self.keys[:-1]:
      header += "\"%s\"" % k + ","
    header += "\"%s\"" % self.keys[-1] + "\n"
    csv = header
    for i in self.labelStats["Labels"]:
      line = ""
      for k in self.keys[:-1]:
        line += str(self.labelStats[i,k]) + ","
      line += str(self.labelStats[i,self.keys[-1]]) + "\n"
      csv += line
    return csv

  def saveStats(self,fileName):
    fp = open(fileName, "w")
    fp.write(self.statsAsCSV())
    fp.close()

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

class BenderLabelStatisticsSlicelet(Slicelet):
  """ Creates the interface when module is run as a stand alone gui app.
  """

  def __init__(self):
    super(BenderLabelStatisticsSlicelet,self).__init__(BenderLabelStatisticsWidget)


if __name__ == "__main__":
  # TODO: need a way to access and parse command line arguments
  # TODO: ideally command line args should handle --xml

  import sys
  print( sys.argv )

  slicelet = BenderLabelStatisticsSlicelet()
