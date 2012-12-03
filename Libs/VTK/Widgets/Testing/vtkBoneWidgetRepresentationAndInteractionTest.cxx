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

// Widgets
#include "vtkBoneWidget.h"
#include "vtkCylinderBoneRepresentation.h"
#include "vtkDoubleConeBoneRepresentation.h"

// VTK includes
#include <vtkAxes.h>
#include <vtkAxesActor.h>
#include <vtkActor.h>
#include <vtkAppendPolyData.h>
#include <vtkBiDimensionalRepresentation2D.h>
#include <vtkCallbackCommand.h>
#include <vtkCommand.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkLineWidget2.h>
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkOrientationMarkerWidget.h>
#include <vtkPointHandleRepresentation3D.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkSmartPointer.h>
#include <vtkSphereSource.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkTransform.h>

// STD includes
#include <map>

// Define interaction style
class KeyPressInteractorStyle : public vtkInteractorStyleTrackballCamera
{
  public:
    static KeyPressInteractorStyle* New();
    vtkTypeMacro(KeyPressInteractorStyle, vtkInteractorStyleTrackballCamera);
 
    virtual void OnKeyPress() 
      {
      vtkRenderWindowInteractor *rwi = this->Interactor;
      std::string key = rwi->GetKeySym();
      std::cout<<"Key pressed: "<<key<<std::endl; 

      if (key == "Control_L")
        {
        int widgetState = this->Widget->GetWidgetState();
   
        if ( widgetState == vtkBoneWidget::PlaceHead )
          {
          //do nothing
          //I wanted this if to be explicit !
          }
        else if ( widgetState == vtkBoneWidget::PlaceTail )
          {
          //do nothing
          //I wanted this if to be explicit !
          }
        else if ( widgetState == vtkBoneWidget::Rest )
          {
          this->Widget->SetWidgetStateToPose();
          }
        else if ( widgetState == vtkBoneWidget::Pose )
          {
          this->Widget->SetWidgetStateToRest();
          }
        }
      else if (key == "Tab")
        {
        vtkWidgetRepresentation* rep = Widget->GetRepresentation();

        if (vtkCylinderBoneRepresentation::SafeDownCast(rep)) // switch to double cone
            {
            vtkDoubleConeBoneRepresentation* simsIconRep =
              vtkDoubleConeBoneRepresentation::New();
            Widget->SetRepresentation(simsIconRep);
            simsIconRep->Delete();
            }
        else if (vtkDoubleConeBoneRepresentation::SafeDownCast(rep)) // switch to line
          {
          vtkBoneRepresentation* lineRep =
            vtkBoneRepresentation::New();
          Widget->SetRepresentation(lineRep);
          lineRep->Delete();
          }
        else if (vtkBoneRepresentation::SafeDownCast(rep))
          {
          vtkCylinderBoneRepresentation* cylinderRep =
            vtkCylinderBoneRepresentation::New();
          Widget->SetRepresentation(cylinderRep);
          cylinderRep->Delete();
          }
        }
      else if (key == "a")
        {
        int show = Widget->GetShowAxes() + 1;
        if (show > vtkBoneWidget::ShowPoseTransform)
          {
          show = vtkBoneWidget::Hidden;
          }
        Widget->SetShowAxes(show);
        }
      }

  vtkBoneWidget* Widget;
};

class vtkSpy : public vtkCommand
{
public:
  vtkTypeMacro(vtkSpy, vtkCommand);
  static vtkSpy *New(){return new vtkSpy;}
  virtual void Execute(vtkObject *caller, unsigned long eventId, 
                       void *callData);
  // List of node that should be updated when NodeAddedEvent is catched
  std::map<unsigned long, unsigned int> CalledEvents;
  std::map<unsigned long, unsigned long> LastEventMTime;

  bool Verbose;
protected:
  vtkSpy():Verbose(false){}
  virtual ~vtkSpy(){}
};

//---------------------------------------------------------------------------
void vtkSpy::Execute(
  vtkObject *vtkcaller, unsigned long eid, void *vtkNotUsed(calldata))
{
  ++this->CalledEvents[eid];
  this->LastEventMTime[eid] = vtkTimeStamp();
  if (this->Verbose)
    {
    std::cout << "vtkSpy: event:" << eid
              << " (" << vtkCommand::GetStringFromEventId(eid) << ")"
              << " count: " << this->CalledEvents[eid]
              << " time: " << this->LastEventMTime[eid]
              << std::endl;
    }
}


vtkStandardNewMacro(KeyPressInteractorStyle);

int vtkBoneWidgetRepresentationAndInteractionTest(int, char *[])
{
  vtkNew<vtkSpy> spy;
  spy->Verbose = true;

  // A renderer and render window
  vtkSmartPointer<vtkRenderer> renderer =
    vtkSmartPointer<vtkRenderer>::New();
  vtkSmartPointer<vtkRenderWindow> renderWindow = 
    vtkSmartPointer<vtkRenderWindow>::New();
  renderWindow->AddRenderer(renderer);

  // An interactor
  vtkSmartPointer<vtkRenderWindowInteractor> renderWindowInteractor =
    vtkSmartPointer<vtkRenderWindowInteractor>::New();
  renderWindowInteractor->SetRenderWindow(renderWindow);

  vtkSmartPointer<vtkBoneWidget> boneWidget =
    vtkSmartPointer<vtkBoneWidget>::New();
  boneWidget->AddObserver(vtkCommand::AnyEvent, spy.GetPointer());

  boneWidget->SetInteractor(renderWindowInteractor);
  //Test Line
  boneWidget->CreateDefaultRepresentation();

  //Setup callbacks
  vtkSmartPointer<KeyPressInteractorStyle> style = 
    vtkSmartPointer<KeyPressInteractorStyle>::New();
  renderWindowInteractor->SetInteractorStyle(style);
  style->Widget = boneWidget;
  style->SetCurrentRenderer(renderer);

  // Render
  renderWindow->Render();
  boneWidget->On();
  renderWindow->Render();

  // Begin mouse interaction
  renderWindowInteractor->Start();
 
  return EXIT_SUCCESS;
}

