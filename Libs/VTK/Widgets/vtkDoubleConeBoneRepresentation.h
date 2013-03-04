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
// .NAME vtkDoubleConeBoneRepresentation - A class defining a representation
// for a vtkBoneWidget
// .SECTION Description
// This class is used to represents a vtkBoneWidget. It derives from
// vtkBoneRepresentation and add a two cones around the bone's line. Each cone
// tip is pointing to one of the line's endpoint.
// The cones base radius is automatically adjusted depending on the line's
// lenght.
// .SECTION See Also
// vtkBoneWidgetRepresentation vtkBoneWidget vtkCylinderRepresentation
// vtkLineRepresentation

#ifndef __vtkDoubleConeBoneRepresentation_h
#define __vtkDoubleConeBoneRepresentation_h

// Bender includes
#include "vtkBenderWidgetsExport.h"

// VTK includes
#include "vtkBoneRepresentation.h"

class vtkActor;
class vtkAppendPolyData;
class vtkCellPicker;
class vtkConeSource;
class vtkPolyDataMapper;
class vtkPolyData;
class vtkProperty;
class vtkTubeFilter;

class VTK_BENDER_WIDGETS_EXPORT vtkDoubleConeBoneRepresentation
  : public vtkBoneRepresentation
{
public:

  // Description:
  // Instantiate this class.
  static vtkDoubleConeBoneRepresentation *New();

  // Description:
  // Standard methods for the class.
  vtkTypeMacro(vtkDoubleConeBoneRepresentation, vtkBoneRepresentation);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // These are methods that satisfy vtkWidgetRepresentation's API.
  virtual void BuildRepresentation();
  virtual double *GetBounds();

  // Description:
  // Retrieve the polydata that defines the cones.
  // To use this method, the user provides the vtkPolyData as an
  // input argument, and the representation polydata is copied into it.
  void GetPolyData(vtkPolyData *pd);

  // Description:
  // Get the radius of the cylinder. The radius is automatically computed
  // from the distance between the two endpoints.
  vtkGetMacro(Radius,double);

  // Description:
  // Set/Get if the cones are capped or not. Default is true.
  vtkSetMacro(Capping ,int);
  vtkGetMacro(Capping, int);

  // Description:
  // Set the sharing ratio between the 2 cones.
  // The minimum value is 0.0001 and the maximum value is 0.9999.
  // Default value is 0.25.
  void SetRatio (double ratio);
  vtkGetMacro(Ratio,double);

  // Description:
  // Set/Get the number of sides of the cylinder. The minimum is 3 and
  // the default is 5.
  void SetNumberOfSides(int numberOfSides);
  vtkGetMacro(NumberOfSides, int);

  // Description:
  // Get the cylinder properties. The properties of the cones when selected
  // and unselected can be manipulated.
  vtkGetObjectMacro(ConesProperty, vtkProperty);
  vtkGetObjectMacro(SelectedConesProperty, vtkProperty);

  // Description:
  // Methods supporting the rendering process.
  virtual void GetActors(vtkPropCollection *pc);
  virtual void ReleaseGraphicsResources(vtkWindow*);
  virtual int HasTranslucentPolygonalGeometry();
  virtual int HasOnlyTranslucentPolygonalGeometry();

  // Description:
  // Helper function to set the opacity of all the cones
  // representation actors (normal and selected).
  virtual void SetOpacity(double opacity);

  // Description:
  // Set/Get if the bones are represented in X-Ray mode or not. In this
  // mode, the bone are overlayed any element of the scene, which makes them
  // always visible. The backface culling is automatically activated.
  // False by default.
  virtual void SetAlwaysOnTop (int onTop);

  // Description:
  // Helper method to highlight the line, the cones and the endpoints.
  virtual void Highlight(int highlight);

  // Description:
  // Reimplemented to translate the bone when clicking on the cones surface.
  virtual int ComputeInteractionState(int X, int Y, int modifier);

protected:
  vtkDoubleConeBoneRepresentation();
  ~vtkDoubleConeBoneRepresentation();

  // The cones
  vtkActor*          ConesActor;
  vtkPolyDataMapper* ConesMapper;
  vtkConeSource*     Cone1;
  vtkConeSource*     Cone2;
  vtkAppendPolyData* GlueFilter;
  vtkCellPicker*     ConesPicker;

  // Properties used to control the appearance of selected objects and
  // the manipulator in general.
  vtkProperty* ConesProperty;
  vtkProperty* SelectedConesProperty;
  void         CreateDefaultProperties();

  //Cone properties
  double Radius;
  int    NumberOfSides;
  double Ratio;
  int    Capping;

  void RebuildCones();

  virtual int RenderOpaqueGeometryInternal(vtkViewport*);
  virtual int RenderTranslucentPolygonalGeometryInternal(vtkViewport*);
  virtual int RenderOverlayInternal(vtkViewport*);

private:
  vtkDoubleConeBoneRepresentation(const vtkDoubleConeBoneRepresentation&);  //Not implemented
  void operator=(const vtkDoubleConeBoneRepresentation&);  //Not implemented
};

#endif
