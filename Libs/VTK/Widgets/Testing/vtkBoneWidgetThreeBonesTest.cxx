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
#include <vtkBoxWidget.h>
#include <vtkBiDimensionalRepresentation2D.h>
#include <vtkCommand.h>
#include <vtkMath.h>
#include <vtkMatrix3x3.h>
#include <vtkTransform.h>
#include <vtkPointHandleRepresentation3D.h>
#include <vtkAxesActor.h>
#include <vtkCaptionActor2D.h>
#include <vtkOrientationMarkerWidget.h>
#include <vtkLineWidget2.h>
#include <vtkLineRepresentation.h>

#include <vtkInteractorStyleTrackballCamera.h>

#include "vtkBoneWidget.h"
#include "vtkCylinderBoneRepresentation.h"
#include "vtkDoubleConeBoneRepresentation.h"
 
// Define interaction style
class ThreeBonesTestKeyPressInteractorStyle : public vtkInteractorStyleTrackballCamera
{
  public:
    static ThreeBonesTestKeyPressInteractorStyle* New();
    vtkTypeMacro(ThreeBonesTestKeyPressInteractorStyle, vtkInteractorStyleTrackballCamera);
 
    virtual void OnKeyPress() 
      {
      vtkRenderWindowInteractor *rwi = this->Interactor;
      std::string key = rwi->GetKeySym();
      std::cout<<"Key Pressed: "<<key<<std::endl;

      if (key == "Control_L")
        {
        int widgetState = this->Widget->GetWidgetState();
        //std::cout<<"WidgetState: "<<widgetState <<" (Rest = "
        //  <<vtkBoneWidget::Rest<<", Pose = "
        //  <<vtkBoneWidget::Pose<<")"<<std::endl;

        if ( widgetState == vtkBoneWidget::Rest )
          {
          std::cout<<"Set Widget State to Pose"<<std::endl;
          this->Widget->SetWidgetStateToPose();
          this->MiddleSonWidget->SetWidgetStateToPose();
          this->SonWidget->SetWidgetStateToPose();
          }
        else if ( widgetState == vtkBoneWidget::Pose )
          {
          std::cout<<"Set Widget State to Rest"<<std::endl;
          this->Widget->SetWidgetStateToRest();
          this->MiddleSonWidget->SetWidgetStateToRest();
          this->SonWidget->SetWidgetStateToRest();
          }

        }
      /*else if (key == "Tab"
        {
        vtkWidgetRepresentation* rep = Widget->GetRepresentation();

        if (vtkCylinderBoneRepresentation::SafeDownCast(rep)) // switch to double cone
            {
            vtkSmartPointer<vtkDoubleConeBoneRepresentation> simsIconRep = 
              vtkSmartPointer<vtkDoubleConeBoneRepresentation>::New();
            simsIconRep->SetNumberOfSides(10);
            simsIconRep->SetRatio(0.2);
            simsIconRep->SetCapping(1);
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
          Widget->SetRepresentation(cylinderRep);
          }
        }*/
      else if (key == "p")
        {
        double axis[3];
        double angle = vtkBoneWidget::QuaternionToAxisAngle(
           this->MiddleSonWidget->GetRestTransform(), axis);

        std::cout<<"MiddleSonWidget RestTransform:"<<std::endl;
        std::cout<<"  Theta:          "<<vtkMath::DegreesFromRadians(angle)<<std::endl;
        std::cout<<"  Rotation Axis:  "<<axis[0]<<" "
          <<axis[1]<<" "<<axis[2]<<std::endl;

        angle = vtkBoneWidget::QuaternionToAxisAngle(
           this->MiddleSonWidget->GetPoseTransform(), axis);
        std::cout<<"MiddleSonWidget Pose Transfotm:"<<std::endl;
        std::cout<<"  Theta:          "<<vtkMath::DegreesFromRadians(angle)<<std::endl;
        std::cout<<"  Rotation Axis:  "<<axis[0]<<" "
          <<axis[1]<<" "<<axis[2]<<std::endl;
        }
      else if (key == "Tab")
        {
        //std::cout<<"Tab"<<std::endl;
        int state = Widget->GetAxesVisibility() + 1;
        if (state > vtkBoneWidget::ShowPoseTransformAndRestTransform)
          {
          state = 0;
          }
        Widget->SetAxesVisibility(state);
        SonWidget->SetAxesVisibility(state);
        MiddleSonWidget->SetAxesVisibility(state);
        }
      else if (key == "l")
        {
        MiddleSonWidget->SetHeadLinkedToParent(
         ! MiddleSonWidget->GetHeadLinkedToParent() );
        SonWidget->SetHeadLinkedToParent(
         ! SonWidget->GetHeadLinkedToParent() );
        }
      }

    vtkBoneWidget* Widget;
    vtkBoneWidget* MiddleSonWidget;
    vtkBoneWidget* SonWidget;
};

vtkStandardNewMacro(ThreeBonesTestKeyPressInteractorStyle);

int vtkBoneWidgetThreeBonesTest(int, char *[])
{
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

  // A box to visualize things better
  vtkSmartPointer<vtkBoxWidget> box = vtkSmartPointer<vtkBoxWidget>::New();
  box->SetInteractor(renderWindowInteractor);
  box->SetScalingEnabled(false);
  box->SetRotationEnabled(false);
  box->SetHandleSize(0.01);
  box->SetOutlineCursorWires(false);
  box->GetHandleProperty()->SetLineWidth(0.001);
  box->GetFaceProperty()->SetRepresentationToSurface();
  box->SetTranslationEnabled(false);
  box->PlaceWidget(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);

  vtkSmartPointer<vtkBoneWidget> fatherBoneWidget = 
    vtkSmartPointer<vtkBoneWidget>::New();
  fatherBoneWidget->SetInteractor(renderWindowInteractor);
  fatherBoneWidget->SetCurrentRenderer(renderer);
  fatherBoneWidget->CreateDefaultRepresentation();

  fatherBoneWidget->GetBoneRepresentation()->GetLineProperty()->SetColor(1.0, 0.0, 0.0);
  fatherBoneWidget->GetBoneRepresentation()->GetHeadRepresentation()->GetProperty()->SetColor(0.0, 1.0, 1.0);
  fatherBoneWidget->GetBoneRepresentation()->GetTailRepresentation()->GetProperty()->SetColor(0.0, 0.0, 1.0);
  fatherBoneWidget->SetWidgetStateToRest();

  //Reset Father position
  fatherBoneWidget->SetHeadRestWorldPosition(0.0, 0.0, 0.0);
  fatherBoneWidget->SetTailRestWorldPosition(0.0, 0.1, 0.0);

  vtkSmartPointer<vtkBoneWidget> middleSonBoneWidget = 
    vtkSmartPointer<vtkBoneWidget>::New();
  middleSonBoneWidget->SetInteractor(renderWindowInteractor);
  middleSonBoneWidget->CreateDefaultRepresentation();

  middleSonBoneWidget->SetWidgetStateToRest();
  middleSonBoneWidget->SetHeadRestWorldPosition(0.0, 0.2, 0.0);
  middleSonBoneWidget->SetTailRestWorldPosition(0.1, 0.2, 0.0);
  middleSonBoneWidget->SetBoneParent(fatherBoneWidget);
  middleSonBoneWidget->GetBoneRepresentation()->GetHeadRepresentation()->GetProperty()->SetColor(0.0, 1.0, 1.0);
  middleSonBoneWidget->GetBoneRepresentation()->GetTailRepresentation()->GetProperty()->SetColor(0.0, 0.0, 1.0);

  vtkSmartPointer<vtkBoneWidget> sonBoneWidget = 
    vtkSmartPointer<vtkBoneWidget>::New();
  sonBoneWidget->SetInteractor(renderWindowInteractor);
  sonBoneWidget->CreateDefaultRepresentation();

  sonBoneWidget->SetWidgetStateToRest();
  sonBoneWidget->SetHeadRestWorldPosition(0.1, 0.3, 0.0);
  sonBoneWidget->SetTailRestWorldPosition(0.1, 0.4, 0.0);
  sonBoneWidget->SetBoneParent(middleSonBoneWidget);
  sonBoneWidget->GetBoneRepresentation()->GetHeadRepresentation()->GetProperty()->SetColor(0.0, 1.0, 1.0);
  sonBoneWidget->GetBoneRepresentation()->GetTailRepresentation()->GetProperty()->SetColor(0.0, 0.0, 1.0);

  //Setup callbacks
  vtkSmartPointer<ThreeBonesTestKeyPressInteractorStyle> style = 
    vtkSmartPointer<ThreeBonesTestKeyPressInteractorStyle>::New();
  renderWindowInteractor->SetInteractorStyle(style);
  style->Widget = fatherBoneWidget;
  style->MiddleSonWidget = middleSonBoneWidget;
  style->SonWidget = sonBoneWidget;
  style->SetCurrentRenderer(renderer);

  vtkSmartPointer<vtkAxesActor> axes = 
    vtkSmartPointer<vtkAxesActor>::New();
 
  vtkSmartPointer<vtkOrientationMarkerWidget> axesWidget = 
    vtkSmartPointer<vtkOrientationMarkerWidget>::New();
  axesWidget->SetOrientationMarker( axes );
  axesWidget->SetInteractor( renderWindowInteractor );
  axesWidget->On();


  // Render
  renderWindow->Render();
  renderWindowInteractor->Initialize();
  renderWindow->Render();
  fatherBoneWidget->On();
  middleSonBoneWidget->On();
  sonBoneWidget->On();


  box->On();

  // Begin mouse interaction
  renderWindowInteractor->Start();
 
  return EXIT_SUCCESS;
}