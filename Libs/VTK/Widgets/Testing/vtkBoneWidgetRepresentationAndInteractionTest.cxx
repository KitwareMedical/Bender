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

#include "vtkCallbackCommand.h"
#include <vtkAxes.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkObjectFactory.h>
#include <vtkPolyData.h>
#include <vtkProperty.h>
#include <vtkSmartPointer.h>
#include <vtkSphereSource.h>
#include <vtkBiDimensionalRepresentation2D.h>
#include <vtkCommand.h>
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkOrientationMarkerWidget.h>
#include <vtkAppendPolyData.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkTransform.h>
#include <vtkAxesActor.h>
#include <vtkPointHandleRepresentation3D.h>
#include <vtkLineWidget2.h>

#include <map>

#include <vtkInteractorStyleTrackballCamera.h>

#include "vtkBoneWidget.h"
#include "vtkCylinderBoneRepresentation.h"
#include "vtkDoubleConeBoneRepresentation.h"
 
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
        /*else if ( widgetState == vtkBoneWidget::Rest )
          {
          this->Widget->SetWidgetStateToPose();
          }
        else if ( widgetState == vtkBoneWidget::Pose )
          {
          this->Widget->SetWidgetStateToRest();
          }*/
        }
      else if (key == "Tab")
        {
        vtkWidgetRepresentation* rep = Widget->GetRepresentation();

        if (vtkCylinderBoneRepresentation::SafeDownCast(rep)) // switch to double cone
            {
            vtkSmartPointer<vtkDoubleConeBoneRepresentation> simsIconRep = 
              vtkSmartPointer<vtkDoubleConeBoneRepresentation>::New();
            simsIconRep->SetNumberOfSides(10);
            simsIconRep->SetRatio(0.2);
            simsIconRep->SetCapping(1);
            simsIconRep->GetConesProperty()->SetOpacity(0.3);
            Widget->SetRepresentation(simsIconRep);
            }
        else if (vtkDoubleConeBoneRepresentation::SafeDownCast(rep)) // switch to line
          {
          vtkSmartPointer<vtkBoneRepresentation> lineRep = 
              vtkSmartPointer<vtkBoneRepresentation>::New();
         
          Widget->SetRepresentation(lineRep);
          }
        else if (vtkBoneRepresentation::SafeDownCast(rep))
          {
          vtkSmartPointer<vtkCylinderBoneRepresentation> cylinderRep = 
            vtkSmartPointer<vtkCylinderBoneRepresentation>::New();
          cylinderRep->SetNumberOfSides(10);
          cylinderRep->GetCylinderProperty()->SetOpacity(0.3);
          Widget->SetRepresentation(cylinderRep);
          }

        Widget->GetBoneRepresentation()->GetLineProperty()->SetOpacity(0.3);
        }
      else if (key == "u")
        {
  //      std::cout<<"here"<<std::endl;
 //       double axis[3];
 //       vtkSmartPointer<vtkTransform> transform = vtkSmartPointer<vtkTransform>::New();
//        transform->Translate( Widget->GetTailRestWorldPosition() );

  //      double angle = vtkBoneWidget::QuaternionToAxisAngle(Widget->GetRestTransform(), axis);
//        transform->RotateWXYZ(vtkMath::DegreesFromRadians(angle), axis[0], axis[1], axis[2]);

   //     Axes->SetUserTransform(transform);
        }
      else if (key == "h")
        {
        Axes->SetVisibility( !Axes->GetVisibility() );
        }
      else if (key == "minus")
        {
        double op = Widget->GetBoneRepresentation()->GetLineProperty()->GetOpacity() - 0.1;
        Widget->GetBoneRepresentation()->SetOpacity(op);
        }
      else if (key == "plus")
        {
        double op = Widget->GetBoneRepresentation()->GetLineProperty()->GetOpacity() + 0.1;
        Widget->GetBoneRepresentation()->SetOpacity(op);
        }

      }

  vtkAxesActor*  Axes;
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

  vtkSmartPointer<vtkBoneWidget> BoneWidget = 
    vtkSmartPointer<vtkBoneWidget>::New();
  BoneWidget->SetInteractor(renderWindowInteractor);
  //Test Line
  BoneWidget->CreateDefaultRepresentation();
  BoneWidget->SetWidgetStateToRest();
  BoneWidget->AddObserver(vtkCommand::AnyEvent, spy.GetPointer());

  //Setup callbacks
  vtkSmartPointer<KeyPressInteractorStyle> style = 
    vtkSmartPointer<KeyPressInteractorStyle>::New();
  renderWindowInteractor->SetInteractorStyle(style);
  style->Widget = BoneWidget;
  style->SetCurrentRenderer(renderer);

  // Render
  renderWindow->Render();
  BoneWidget->On();
  renderWindow->Render();

  // Begin mouse interaction
  renderWindowInteractor->Start();
 
  return EXIT_SUCCESS;
}

