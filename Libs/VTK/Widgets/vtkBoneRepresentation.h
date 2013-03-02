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
// .NAME vtkBoneRepresentation - A class defining the representation
// for a vtkBoneWidget
// .SECTION Description
// This class is used to represents a vtkBoneWidget. It derives from
// vtkLineRepresentation and simply wraps the vtkLineRepresentation with
// more appropriate names.
// .SECTION See Also
// vtkBoneWidget vtkCylinderRepresentation  vtkDoubleConeRepresentation
// vtkLineRepresentation

#ifndef __vtkBoneRepresentation_h
#define __vtkBoneRepresentation_h

// VTK includes
#include <vtkLineRepresentation.h>

// Bender includes
#include "vtkBenderWidgetsExport.h"

class vtkBoneEnvelopeRepresentation;
class vtkPointHandleRepresentation3D;
class vtkProp;
class vtkTransform;

class VTK_BENDER_WIDGETS_EXPORT vtkBoneRepresentation
  : public vtkLineRepresentation
{
public:
  // Description:
  // Instantiate the class.
  static vtkBoneRepresentation *New();

  // Description:
  // Standard methods for the class.
  vtkTypeMacro(vtkBoneRepresentation, vtkLineRepresentation);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Methods to Set/Get the coordinates of the two points defining
  // this representation. Note that methods are available for both
  // display and world coordinates.
  void GetWorldHeadPosition(double pos[3]);
  double* GetWorldHeadPosition();
  void GetDisplayHeadPosition(double pos[3]);
  double* GetDisplayHeadPosition();
  virtual void SetWorldHeadPosition(double pos[3]);
  virtual void SetDisplayHeadPosition(double pos[3]);
  void GetDisplayTailPosition(double pos[3]);
  double* GetDisplayTailPosition();
  void GetWorldTailPosition(double pos[3]);
  double* GetWorldTailPosition();
  virtual void SetWorldTailPosition(double pos[3]);
  virtual void SetDisplayTailPosition(double pos[3]);

  // Description:
  // Methods to return the distance between the head and the tail.
  virtual double GetLength();

  // Description:
  // Enum that mirrors the enums in vtkLineRepresentation with appropriate
  // names for the bone animation. They manage the state of the widget.
  //BTX
  enum {Outside=0,
        OnHead,
        OnTail,
        TranslatingHead,
        TranslatingTail,
        OnLine,
        Scaling};
  //ETX

  // Description:
  // Standard methods to get the handle representations.
  // This allows the user to change the properties of the bone's
  // endpoints.
  vtkPointHandleRepresentation3D* GetHeadRepresentation();
  vtkPointHandleRepresentation3D* GetTailRepresentation();

  // Description:
  // Helper method to highlight the line and the endpoints.
  virtual void Highlight(int highlight);

  // Description:
  // Set/Get if the bones are represented in X-Ray mode or not. In this
  // mode, the bone are overlayed any element of the scene, which makes them
  // always visible. The backface culling is automatically activated.
  // False by default.
  virtual void SetAlwaysOnTop (int onTop);
  vtkGetMacro(AlwaysOnTop, int);

  // Description:
  // These are methods that satisfy vtkWidgetRepresentation's API.
  virtual void SetRenderer(vtkRenderer *ren);
  virtual void BuildRepresentation();

  // Description:
  // Rendering methods.
  virtual void GetActors(vtkPropCollection *pc);

  virtual void ReleaseGraphicsResources(vtkWindow*);

  virtual int HasTranslucentPolygonalGeometry();
  virtual int HasOnlyTranslucentPolygonalGeometry();

  virtual int RenderTranslucentPolygonalGeometry(vtkViewport*);
  virtual int RenderOpaqueGeometry(vtkViewport*);
  virtual int RenderOverlay(vtkViewport*);

  // Description:
  // Deep copy all the properties of the given prop to the bone representation.
  // \sa DeepCopyRepresentationOnly()
  virtual void DeepCopy(vtkProp* prop);

  // Description:
  // Deep copy all the representation properties of the given bone
  // representation. This is useful for having the representation with the same
  // graphical properties and yet associated with different bones.
  // For example, the AlwaysOnTop (visual only) property will be copied
  // whereas the Head property will not as it would change the displayed bone
  // position.
  // \sa DeepCopy()
  virtual void DeepCopyRepresentationOnly(
    vtkBoneRepresentation* boneRepresentation);

  // Description:
  // Helper function to set the opacity of all the bone representation
  // actors (normal and selected).
  virtual void SetOpacity(double opacity);

  // Description:
  // This property controls the behavior when interacting with the
  // line/tail.
  // @sa vtkBoneWidget::WidgetStateType
  vtkSetMacro(Pose, bool);
  vtkGetMacro(Pose, bool);

  // Description:
  // Reimplemented to rotate the bone in pose mode
  virtual void WidgetInteraction(double e[2]);

  // Description:
  // Reimplemented to prevent head selection in pose mode
  virtual int ComputeInteractionState(int X, int Y, int modifier = 0);

  // Description:
  // This property controls the visibility of the envelope.
  vtkSetMacro(ShowEnvelope, bool);
  vtkGetMacro(ShowEnvelope, bool);

  // Description:
  // Retrieve the envelope. Exist even of not visible.
  vtkGetObjectMacro(Envelope, vtkBoneEnvelopeRepresentation);

  // Description:
  // For updating the envelope actor's rotation.
  void SetWorldToBoneRotation(vtkTransform* worldToBoneRotation);

protected:
  vtkBoneRepresentation();
  ~vtkBoneRepresentation();

  int AlwaysOnTop;
  bool Pose;
  bool ShowEnvelope;
  vtkBoneEnvelopeRepresentation* Envelope;

  // Protected rendring classes. They do the the regular job of rendering and
  // are called depeding if the rendering is overlayed or not.
  virtual int RenderOpaqueGeometryInternal(vtkViewport *v);
  virtual int RenderTranslucentPolygonalGeometryInternal(vtkViewport* v);
  virtual int RenderOverlayInternal(vtkViewport*);

  void Rotate(double angle, double axis[3], double center[3], double pos[3], double res[3]);

private:
  vtkBoneRepresentation(const vtkBoneRepresentation&);  //Not implemented
  void operator=(const vtkBoneRepresentation&);  //Not implemented
};

#endif
