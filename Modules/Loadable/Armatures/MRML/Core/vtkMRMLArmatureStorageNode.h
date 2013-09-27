/*=========================================================================

  Program: Bender

  Copyright (c) Kitware Inc.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

=========================================================================*/

#ifndef __vtkMRMLArmatureStorageNode_h
#define __vtkMRMLArmatureStorageNode_h

#include "vtkMRMLStorageNode.h"

// Armatures includes
#include "vtkBenderArmaturesModuleMRMLCoreExport.h"
class vtkMRMLArmatureNode;
class vtkMRMLBoneNode;
class vtkBVHReader;

// VTK includes
class vtkPolyData;
class vtkObserverManager;

// Slicer includes
class vtkSlicerAnnotationModuleLogic;

/// \brief Loads armature files
///
/// The vtkArmatureStorageNode handles the loading of armature files.
/// There is essentialy two possible treatments:
/// For *.vtk (and *.arm which are *.vtk files with a different extension),
/// the storage nodes simply loads the armature file, setting the hierarchy
/// properly.
/// For *.bvh files, the storage node keeps a reference on the bvh reader
/// so it can latter change the armature pose at convinience.
class VTK_BENDER_ARMATURES_MRML_CORE_EXPORT vtkMRMLArmatureStorageNode
  : public vtkMRMLStorageNode
{
public:
  static vtkMRMLArmatureStorageNode *New();
  vtkTypeMacro(vtkMRMLArmatureStorageNode, vtkMRMLStorageNode);
  void PrintSelf(ostream& os, vtkIndent indent);

  virtual vtkMRMLNode* CreateNodeInstance();

  ///
  /// Get node XML tag name (like Storage, Model)
  virtual const char* GetNodeTagName()  {return "BVHStorage";};

  /// Return a default file extension for writting
  virtual const char* GetDefaultWriteFileExtension();

  /// Return true if the reference node can be read in
  virtual bool CanReadInReferenceNode(vtkMRMLNode *refNode);

  /// Create an armature from a model. Returns a the armature
  ///  node added. Note that the model is not actually loaded
  /// in slicer.
  int CreateArmatureFromModel(
    vtkMRMLArmatureNode* armature, vtkPolyData* model);

  /// MRML scene callback
  static void MRMLSceneCallback(
    vtkObject *caller, unsigned long eid,
    void *clientData, void *callData);

  /// Get the total number of frames from the BVHReader.
  /// Returns 0 by default and if there's no BVH reader.
  unsigned int GetNumberOfFrames();

  /// Get the total number of frames from the BVHReader.
  /// Returns 0 by default and if there's no BVH reader.
  double GetFrameRate();

  /// Apply the frame to the armature node.
  void SetFrame(vtkMRMLArmatureNode* armatureNode, unsigned int frame);

protected:
  vtkMRMLArmatureStorageNode();
  ~vtkMRMLArmatureStorageNode();
  vtkMRMLArmatureStorageNode(const vtkMRMLArmatureStorageNode&);
  void operator=(const vtkMRMLArmatureStorageNode&);

  /// Initialize all the supported read file types
  virtual void InitializeSupportedReadFileTypes();

  /// Initialize all the supported write file types
  virtual void InitializeSupportedWriteFileTypes();

  /// Read data and set it in the referenced node
  virtual int ReadDataInternal(vtkMRMLNode *refNode);

  /// Write data from a  referenced node
  virtual int WriteDataInternal(vtkMRMLNode *refNode);

  virtual void ProcessMRMLSceneEvents(
    vtkObject *caller, unsigned long eid, void *callData);

  // Scene callback variables
  vtkMRMLBoneNode* CurrentlyAddedBoneNode;
  vtkMRMLBoneNode* CurrentlyAddedBoneNodeParent;
  vtkMRMLArmatureNode* CurrentlyAddedArmatureNode;
  vtkObserverManager* SceneObserverManager;

  // Only valid when reading a *.bvh file.
  vtkBVHReader* BVHReader;
};
#endif
