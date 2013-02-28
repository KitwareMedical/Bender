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

// .NAME vtkBoneEnvelopeRepresentation - a class defining and envelope
// representation for a bone widget
// .SECTION Description
// \todo
// .SECTION See Also
// vtkBoneWidget

#ifndef __vtkBoneenvelopeRepresentation_h
#define __vtkBoneenvelopeRepresentation_h

// Bender includes
#include "vtkBenderWidgetsExport.h"

// VTK includes
#include "vtkWidgetRepresentation.h"

class vtkActor;

class vtkBooleanOperationPolyDataFilter;
class vtkAppendPolyData;

class vtkCapsuleSource;
class vtkPolyDataMapper;
class vtkProperty;
class vtkPolyData;
class vtkTransform;

class VTK_BENDER_WIDGETS_EXPORT vtkBoneEnvelopeRepresentation
  : public vtkWidgetRepresentation
{
public:
  // Description:
  // Instantiate the class.
  static vtkBoneEnvelopeRepresentation *New();

  // Description:
  // Standard methods for the class.
  vtkTypeMacro(vtkBoneEnvelopeRepresentation,vtkWidgetRepresentation);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Get the envelope properties.
  vtkGetObjectMacro(Property,vtkProperty);

  // Description:
  // Set/Get the resolution of the envelope.
  // The corresponds to the theta and phi resolution of the capsule.
  // \sa vtkCapsuleSource::SetThetaResolution()
  // \sa vtkCapsuleSource::SetPhiResolution()
  void SetResolution(int res);
  int GetResolution();

  // Description:
  // Retrieve the polydata that defines the envellope.
  // To use this method, the user provides the vtkPolyData
  // as an input argument, and the points and
  // polyline are copied into it.
  void GetPolyData(vtkPolyData *pd);

  // Description:
  // These are methods that satisfy vtkWidgetRepresentation's API.
  virtual void BuildRepresentation();

  // Description:
  // Methods supporting the rendering process.
  virtual void GetActors(vtkPropCollection *pc);
  virtual void ReleaseGraphicsResources(vtkWindow*);
  virtual int RenderOpaqueGeometry(vtkViewport*);
  virtual int RenderTranslucentPolygonalGeometry(vtkViewport*);
  virtual int HasTranslucentPolygonalGeometry();

  // Description:
  // Overload the superclasses' GetMTime() because internal classes
  // are used to keep the state of the representation.
  virtual unsigned long GetMTime();

  // Description:
  // Set/Get the envelope head or tail.
  vtkSetVector3Macro(Head, double);
  vtkGetVector3Macro(Head, double);
  vtkSetVector3Macro(Tail, double);
  vtkGetVector3Macro(Tail, double);

  // Description:
  // Set/Get the envelope radius.
  vtkSetMacro(Radius, double);
  vtkGetMacro(Radius, double);

  // Description:
  // DeepCopy the properties of the other envelop.
  void DeepCopy(vtkProp* other);

  // Description:
  // Set the world to bone rotation so the capsule is rotated to its
  // correct orientation. (Just like the bone).
  void SetWorldToBoneRotation(vtkTransform* worldToBoneRotation);

protected:
  vtkBoneEnvelopeRepresentation();
  ~vtkBoneEnvelopeRepresentation();

  // Governing variables.
  double Head[3];
  double Tail[3];
  double Radius;
  vtkTransform* Rotation;

  // Representation
  vtkActor* EnvelopeActor;
  vtkPolyDataMapper*  EnvelopeMapper;
  vtkCapsuleSource* CapsuleSource;

  // Properties
  vtkProperty* Property;
  void CreateDefaultProperties();

  void RebuildEnvelope();

private:
  vtkBoneEnvelopeRepresentation(const vtkBoneEnvelopeRepresentation&);  //Not implemented
  void operator=(const vtkBoneEnvelopeRepresentation&);  //Not implemented
};

#endif
