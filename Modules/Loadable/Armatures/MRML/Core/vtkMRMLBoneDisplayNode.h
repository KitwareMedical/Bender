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

#ifndef __vtkMRMLBoneDisplayNode_h
#define __vtkMRMLBoneDisplayNode_h

// Slicer includes
#include "vtkMRMLAnnotationDisplayNode.h"

// Armatures includes
#include "vtkBenderArmaturesModuleMRMLCoreExport.h"

class vtkBoneWidget;

/// \ingroup Bender_MRML
/// \brief Annotation to represent a bone.
///
/// \sa vtkMRMLBoneNode, vtkMRMLArmatureNode
class VTK_BENDER_ARMATURES_MRML_CORE_EXPORT vtkMRMLBoneDisplayNode
  : public vtkMRMLAnnotationDisplayNode
{
public:
  //--------------------------------------------------------------------------
  // VTK methods
  //--------------------------------------------------------------------------

  static vtkMRMLBoneDisplayNode *New();
  vtkTypeMacro(vtkMRMLBoneDisplayNode,vtkMRMLAnnotationDisplayNode);
  virtual void PrintSelf(ostream& os, vtkIndent indent);

  //--------------------------------------------------------------------------
  // MRMLNode methods
  //--------------------------------------------------------------------------

  /// Instantiate a bone node.
  virtual vtkMRMLNode* CreateNodeInstance();

  /// Get node XML tag name (like Volume, Model).
  virtual const char* GetNodeTagName() {return "BoneDisplay";};

  /// Read node attributes from XML file.
  virtual void ReadXMLAttributes( const char** atts);

  /// Write this node's information to a MRML file in XML format.
  virtual void WriteXML(ostream& of, int indent);

  /// Copy the node's attributes to this object.
  virtual void Copy(vtkMRMLNode *node);

  virtual void UpdateScene(vtkMRMLScene *scene);
  virtual void ProcessMRMLEvents(vtkObject* caller,
                                 unsigned long event,
                                 void* callData);

  /// Reimplement the SetColor so the new color automatically defines
  /// the selected color and the widget interaction color.
  virtual void SetColor(double color[3]);
  virtual void SetColor(double r, double g, double b);

  /// Set/Get the color of the widget when interacting.
  vtkSetVector3Macro(InteractionColor, double);
  vtkGetVector3Macro(InteractionColor, double);

  vtkSetMacro(ShowEnvelope,int);
  vtkGetMacro(ShowEnvelope,int);
  vtkBooleanMacro(ShowEnvelope,int);

  vtkSetMacro(EnvelopeRadius, double);
  vtkGetMacro(EnvelopeRadius, double);


  //--------------------------------------------------------------------------
  // Bone methods
  //--------------------------------------------------------------------------

  /// Copy the properties of the widget into the node
  /// \sa PasteBoneDisplayNodeProperties()
  void CopyBoneWidgetDisplayProperties(vtkBoneWidget* boneWidget);

  /// Paste the properties of the node into the widget
  /// \sa CopyBoneWidgetDisplayProperties()
  void PasteBoneDisplayNodeProperties(vtkBoneWidget* boneWidget);

protected:
  vtkMRMLBoneDisplayNode();
  ~vtkMRMLBoneDisplayNode();

  double InteractionColor[3];
  int ShowEnvelope;
  double EnvelopeRadius;

  vtkMRMLBoneDisplayNode(const vtkMRMLBoneDisplayNode&); /// not implemented
  void operator=(const vtkMRMLBoneDisplayNode&); /// not implemented
};

#endif
