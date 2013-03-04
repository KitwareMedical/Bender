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
// .NAME vtkCylinderRepresentation - A class defining the representation
// for a vtkBoneWidget
// .SECTION Description
// This class is used to represents a vtkBoneWidget. It derives from
// vtkBoneRepresentation and add a cylinder around the bone's line.
// The cylinder radius is automatically adjusted depending on the line's
// lenght.
// .SECTION See Also
// vtkBoneWidgetRepresentation vtkBoneWidget vtkDoubleConeRepresentation
// vtkLineRepresentation

#ifndef __vtkCylinderBoneRepresentation_h
#define __vtkCylinderBoneRepresentation_h

// Bender includes
#include "vtkBenderWidgetsExport.h"

// VTK includes
#include "vtkBoneRepresentation.h"

class vtkActor;
class vtkCellPicker;
class vtkPolyDataMapper;
class vtkPolyData;
class vtkProperty;
class vtkTubeFilter;

class VTK_BENDER_WIDGETS_EXPORT vtkCylinderBoneRepresentation
  : public vtkBoneRepresentation
{
public:

  // Description:
  // Instantiate this class.
  static vtkCylinderBoneRepresentation *New();

  // Description:
  // Standard methods for the class.
  vtkTypeMacro(vtkCylinderBoneRepresentation, vtkBoneRepresentation);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // These are methods that satisfy vtkWidgetRepresentation's API.
  virtual void BuildRepresentation();
  virtual double *GetBounds();

  // Description:
  // Retrieve the polydata that defines the cylinder.
  // To use this method, the user provides the vtkPolyData as an
  // input argument, and the representation polydata is copied into it.
  void GetPolyData(vtkPolyData *pd);

  // Description:
  // Get the radius of the cylinder. The radius is automatically computed
  // from the distance between the two endpoints.
  vtkGetMacro(Radius,double);

  // Description:
  // Set/Get if the cylinder is capped or not. Default is true.
  vtkSetMacro(Capping, int);
  vtkGetMacro(Capping, int);

  // Description:
  // Set/Get the number of sides of the cylinder. The minimum is 3 and
  // the default is 5.
  void SetNumberOfSides(int numberOfSides);
  vtkGetMacro(NumberOfSides, int);

  // Description:
  // Get the cylinder properties. The properties of the cylinder
  // can be manipulated.
  vtkGetObjectMacro(CylinderProperty,vtkProperty);
  vtkGetObjectMacro(SelectedCylinderProperty,vtkProperty);

  // Description:
  // Methods supporting the rendering process.
  virtual void GetActors(vtkPropCollection *pc);
  virtual void ReleaseGraphicsResources(vtkWindow*);
  virtual int HasTranslucentPolygonalGeometry();
  virtual int HasOnlyTranslucentPolygonalGeometry();

  // Description:
  // Helper function to set the opacity of all the cylinder
  // representation actors (normal and selected).
  virtual void SetOpacity(double opacity);

  // Description:
  // Set/Get if the bones are represented in X-Ray mode or not. In this
  // mode, the bone are overlayed any element of the scene, which makes them
  // always visible. The backface culling is automatically activated.
  // False by default.
  virtual void SetAlwaysOnTop (int onTop);

  // Description:
  // Helper method to highlight the line, the cylinder and the endpoints.
  virtual void Highlight(int highlight);

  // Description:
  // Reimplemented to translate the bone when clicking on the cylinder surface.
  virtual int ComputeInteractionState(int X, int Y, int modifier);

protected:
  vtkCylinderBoneRepresentation();
  ~vtkCylinderBoneRepresentation();

  // The cylinder
  vtkActor*          CylinderActor;
  vtkPolyDataMapper* CylinderMapper;
  vtkTubeFilter*     CylinderGenerator;
  vtkCellPicker*     CylinderPicker;

  // Properties used to control the appearance of selected objects and
  // the manipulator in general.
  vtkProperty* CylinderProperty;
  vtkProperty* SelectedCylinderProperty;
  void         CreateDefaultProperties();

  // Cylinder properties
  double Radius;
  int    Capping;
  int    NumberOfSides;

  void RebuildCylinder();
  virtual int RenderOpaqueGeometryInternal(vtkViewport*);
  virtual int RenderTranslucentPolygonalGeometryInternal(vtkViewport*);
  virtual int RenderOverlayInternal(vtkViewport*);

private:
  vtkCylinderBoneRepresentation(const vtkCylinderBoneRepresentation&);  // Not implemented
  void operator=(const vtkCylinderBoneRepresentation&);  // Not implemented
};

#endif
