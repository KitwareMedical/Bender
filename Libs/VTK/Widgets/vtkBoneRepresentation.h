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

class vtkPointHandleRepresentation3D;

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
  void GetDisplayHeadPosition(double pos[2]);
  double* GetDisplayHeadPosition();
  virtual void SetWorldHeadPosition(double pos[3]);
  virtual void SetDisplayHeadPosition(double pos[2]);
  void GetDisplayTailPosition(double pos[2]);
  double* GetDisplayTailPosition();
  void GetWorldTailPosition(double pos[3]);
  double* GetWorldTailPosition();
  virtual void SetWorldTailPosition(double pos[3]);
  virtual void SetDisplayTailPosition(double pos[2]);

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
  // always visible.
  vtkSetMacro(AlwaysOnTop, int);
  vtkGetMacro(AlwaysOnTop, int);

  // Description:
  // Rendering methods.
  virtual int RenderTranslucentPolygonalGeometry(vtkViewport*);
  virtual int RenderOpaqueGeometry(vtkViewport*);
  virtual int RenderOverlay(vtkViewport*);

protected:
  vtkBoneRepresentation();
  ~vtkBoneRepresentation();

  int AlwaysOnTop;

  // Protected rendring classes. They do the the regular job of rendering and
  // are called depeding if the rendering is overlayed or not.
  virtual int RenderOpaqueGeometryInternal(vtkViewport *v);
  virtual int RenderTranslucentPolygonalGeometryInternal(vtkViewport* v);

private:
  vtkBoneRepresentation(const vtkBoneRepresentation&);  //Not implemented
  void operator=(const vtkBoneRepresentation&);  //Not implemented
};

#endif
