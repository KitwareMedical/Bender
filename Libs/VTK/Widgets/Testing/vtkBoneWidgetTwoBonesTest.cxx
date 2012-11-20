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
class TwoBonesTestKeyPressInteractorStyle : public vtkInteractorStyleTrackballCamera
{
  public:
    static TwoBonesTestKeyPressInteractorStyle* New();
    vtkTypeMacro(TwoBonesTestKeyPressInteractorStyle, vtkInteractorStyleTrackballCamera);
 
    virtual void OnKeyPress() 
      {
      vtkRenderWindowInteractor *rwi = this->Interactor;
      std::string key = rwi->GetKeySym();
      std::cout<<"Key Pressed: "<<key<<std::endl;

      if (key == "Control_L")
        {
        //int widgetState = this->Widget->GetWidgetState();
        //std::cout<<"WidgetState: "<<widgetState <<" (Rest = "
        //  <<vtkBoneWidget::Rest<<", Pose = "
        //  <<vtkBoneWidget::Pose<<")"<<std::endl;

        /*if ( widgetState == vtkBoneWidget::Rest )
          {
          this->Widget->SetWidgetStateToPose();
          this->SonWidget->SetWidgetStateToPose();
          }
        else if ( widgetState == vtkBoneWidget::Pose )
          {
          this->Widget->SetWidgetStateToRest();
          this->SonWidget->SetWidgetStateToRest();
          }*/

        }
      else if (key == "Tab")
        {
        //std::cout<<"Tab"<<std::endl;
        //int state = Widget->GetAxesVisibility() + 1;
        //if (state > vtkBoneWidget::ShowPoseTransformAndRestTransform)
        //  {
        //  state = 0;
        //  }
        //Widget->SetAxesVisibility(state);
        //SonWidget->SetAxesVisibility(state);
        }
      else if (key == "l")
        {
        //SonWidget->SetHeadLinkedToParent(
        // ! SonWidget->GetHeadLinkedToParent() );
        }
      else if (key == "quoteleft")
        {
        //SonWidget->SetShowParentage(
        //  ! SonWidget->GetShowParentage());
        }
      else if (key == "x")
        {
        //Widget->RotateTailX(vtkMath::RadiansFromDegrees(1.0));
        }
      }

    vtkBoneWidget* Widget;
    vtkBoneWidget* SonWidget;
};

vtkStandardNewMacro(TwoBonesTestKeyPressInteractorStyle);

int vtkBoneWidgetTwoBonesTest(int, char *[])
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

  fatherBoneWidget->GetBoneRepresentation()->GetLineProperty()->SetColor(0.5, 0.5, 0.5);
  fatherBoneWidget->GetBoneRepresentation()->GetHeadRepresentation()->GetProperty()->SetColor(0.0, 1.0, 1.0);
  fatherBoneWidget->GetBoneRepresentation()->GetTailRepresentation()->GetProperty()->SetColor(0.0, 0.0, 1.0);
  fatherBoneWidget->SetWidgetStateToRest();

  //Test RestTransform matrix
  double axis[3], expectedAngle, expectedAxis[3], angle;

  //Test Z Axis
//  fatherBoneWidget->SetHeadRestWorldPosition(0.0, 0.0, 0.0);
//  fatherBoneWidget->SetTailRestWorldPosition(0.0, 0.0, 0.1);
  expectedAngle = vtkMath::Pi() / 2.0;
  expectedAxis[0] = 1.0; expectedAxis[1] = 0.0; expectedAxis[2] = 0.0;

//  angle = vtkBoneWidget::QuaternionToAxisAngle(fatherBoneWidget->GetRestTransform(), axis);
  /*if (fabs(angle - expectedAngle) > 0.0001)
    {
    std::cout<<"Angle different !"<<std::endl
      <<"Expected "<< expectedAngle <<" - Got "<< angle <<std::endl;
    //return EXIT_FAILURE;
    }
  for (int i =0; i < 3; ++i)
    {
    if (fabs(expectedAxis[i] - axis[i]) > 0.0001)
      {
      std::cout<<"Axis Different different !"<<std::endl
        <<"Expected:  "<<expectedAxis[0]<<" "
                      <<expectedAxis[1]<<" "
                      <<expectedAxis[2]<<std::endl
        <<" - Got:    "<<axis[0]<<" "
                      <<axis[1]<<" "
                      <<axis[2]<<std::endl;
      //return EXIT_FAILURE;
      }
    }*/

  //Test Y Axis
  //std::cout<<"Father, along Y"<<std::endl;
//  fatherBoneWidget->SetHeadRestWorldPosition(0.0, 0.0, 0.0);
//  fatherBoneWidget->SetTailRestWorldPosition(0.0, 0.1, 0.0);
  expectedAngle = 0.0;
  expectedAxis[0] = 1.0; expectedAxis[1] = 0.0; expectedAxis[2] = 0.0;

//  angle = vtkBoneWidget::QuaternionToAxisAngle(fatherBoneWidget->GetRestTransform(), axis);
  if (fabs(angle - expectedAngle) > 0.0001)
    {
    std::cout<<"Angle different !"<<std::endl
      <<"Expected "<< expectedAngle <<" - Got "<< angle <<std::endl;
    //return EXIT_FAILURE;
    }
  for (int i =0; i < 3; ++i)
    {
    if (fabs(expectedAxis[i] - axis[i]) > 0.0001)
      {
      std::cout<<"Axis Different different !"<<std::endl
        <<"Expected:  "<<expectedAxis[0]<<" "
                      <<expectedAxis[1]<<" "
                      <<expectedAxis[2]<<std::endl
        <<" - Got:    "<<axis[0]<<" "
                      <<axis[1]<<" "
                      <<axis[2]<<std::endl;
      //return EXIT_FAILURE;
      }
    }
    
  //Test X Axis
  //std::cout<<"Father, along X"<<std::endl;
//  fatherBoneWidget->SetTailRestWorldPosition(0.1, 0.0, 0.0);
  expectedAngle = vtkMath::Pi() / 2.0;
  expectedAxis[0] = 0.0; expectedAxis[1] = 0.0; expectedAxis[2] = -1.0;

//  angle = vtkBoneWidget::QuaternionToAxisAngle(fatherBoneWidget->GetRestTransform(), axis);
  if (fabs(angle - expectedAngle) > 0.0001)
    {
    std::cout<<"Angle different !"<<std::endl
      <<"Expected "<< expectedAngle <<" - Got "<< angle <<std::endl;
    //return EXIT_FAILURE;
    }
  for (int i =0; i < 3; ++i)
    {
    if (fabs(expectedAxis[i] - axis[i]) > 0.0001)
      {
      std::cout<<"Axis Different different !"<<std::endl
        <<"Expected:  "<<expectedAxis[0]<<" "
                      <<expectedAxis[1]<<" "
                      <<expectedAxis[2]<<std::endl
        <<" - Got:    "<<axis[0]<<" "
                      <<axis[1]<<" "
                      <<axis[2]<<std::endl;
      //return EXIT_FAILURE;
      }
    }

  //Test Weirder Axis
  //std::cout<<"Father, along weirder axis"<<std::endl;
//  fatherBoneWidget->SetTailRestWorldPosition(0.1, 0.1, 0.1);
  expectedAngle = 0.955317;
  expectedAxis[0] = sqrt(2.0)/2.0; expectedAxis[1] = 0.0; expectedAxis[2] = -sqrt(2.0)/2.0;

//  angle = vtkBoneWidget::QuaternionToAxisAngle(fatherBoneWidget->GetRestTransform(), axis);
  if (fabs(angle - expectedAngle) > 0.0001)
    {
    std::cout<<"Angle different !"<<std::endl
      <<"Expected "<< expectedAngle <<" - Got "<< angle <<std::endl;
    //return EXIT_FAILURE;
    }
  for (int i =0; i < 3; ++i)
    {
    if (fabs(expectedAxis[i] - axis[i]) > 0.0001)
      {
      std::cout<<"Axis Different different !"<<std::endl
        <<"Expected:  "<<expectedAxis[0]<<" "
                      <<expectedAxis[1]<<" "
                      <<expectedAxis[2]<<std::endl
        <<" - Got:    "<<axis[0]<<" "
                      <<axis[1]<<" "
                      <<axis[2]<<std::endl;
      //return EXIT_FAILURE;
      }
    }

  //Reset Father position
//  fatherBoneWidget->SetTailRestWorldPosition(0.1, 0.0, 0.0);

  vtkSmartPointer<vtkBoneWidget> sonBoneWidget = 
    vtkSmartPointer<vtkBoneWidget>::New();
  sonBoneWidget->SetInteractor(renderWindowInteractor);
  sonBoneWidget->SetCurrentRenderer(renderer);
  sonBoneWidget->CreateDefaultRepresentation();

  sonBoneWidget->SetWidgetStateToRest();
//  sonBoneWidget->SetHeadRestWorldPosition(0.2, 0.0, -0.1);
//  sonBoneWidget->SetTailRestWorldPosition(0.2, 0.0, -0.2);
//  sonBoneWidget->SetBoneParent(fatherBoneWidget);
  sonBoneWidget->GetBoneRepresentation()->GetHeadRepresentation()->GetProperty()->SetColor(0.0, 1.0, 1.0);
  sonBoneWidget->GetBoneRepresentation()->GetTailRepresentation()->GetProperty()->SetColor(0.0, 0.0, 1.0);

  //Test son
  //RestTransform
  expectedAngle = vtkMath::Pi() / 2.0;
  expectedAxis[0] = 1.0; expectedAxis[1] = 0.0; expectedAxis[2] = 0.0;
//  angle = vtkBoneWidget::QuaternionToAxisAngle(sonBoneWidget->GetRestTransform(), axis);
  if (fabs(angle - expectedAngle) > 0.0001)
    {
    std::cout<<"Angle different !"<<std::endl
      <<"Expected "<< expectedAngle <<" - Got "<< angle <<std::endl;
    //return EXIT_FAILURE;
    }
  for (int i =0; i < 3; ++i)
    {
    if (fabs(expectedAxis[i] - axis[i]) > 0.0001)
      {
      std::cout<<"Axis Different different !"<<std::endl
        <<"Expected:  "<<expectedAxis[0]<<" "
                      <<expectedAxis[1]<<" "
                      <<expectedAxis[2]<<std::endl
        <<" - Got:    "<<axis[0]<<" "
                      <<axis[1]<<" "
                      <<axis[2]<<std::endl;
      //return EXIT_FAILURE;
      }
    }

  //Setup callbacks
  vtkSmartPointer<TwoBonesTestKeyPressInteractorStyle> fatherStyle = 
    vtkSmartPointer<TwoBonesTestKeyPressInteractorStyle>::New();
  renderWindowInteractor->SetInteractorStyle(fatherStyle);
  fatherStyle->Widget = fatherBoneWidget;
  fatherStyle->SonWidget = sonBoneWidget;
  //fatherStyle->X = X;
  //fatherStyle->Y = Y;
  //fatherStyle->Z = Z;
  //fatherStyle->TestLine = TestLine;
  fatherStyle->SetCurrentRenderer(renderer);

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
  sonBoneWidget->On();
  //X->On();
 // Y->On();
  //Z->On();
  //TestLine->On();

  box->On();

  // Begin mouse interaction
  renderWindowInteractor->Start();
 
  return EXIT_SUCCESS;
}
