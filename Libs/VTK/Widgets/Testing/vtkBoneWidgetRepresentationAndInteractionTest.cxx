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

#include "vtkBoneEnvelopeRepresentation.h"

// VTK includes
#include <vtkCallbackCommand.h>
#include <vtkCommand.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkObjectFactory.h>

#include <vtkProperty.h>

#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkSmartPointer.h>

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
      if (key == "h")
        {
        this->Widget->SetWidgetState(vtkBoneWidget::PlaceHead );
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
      else if (key == "1")
        {
        Widget->GetBoneRepresentation()->SetShowEnvelope(
          ! Widget->GetBoneRepresentation()->GetShowEnvelope());
        }
      }

  vtkBoneWidget* Widget;
};

vtkStandardNewMacro(KeyPressInteractorStyle);

int vtkBoneWidgetRepresentationAndInteractionTest(int argc, char* argv[])
{
  bool interactive = false;
  for (int i = 0; i < argc; ++i)
    {
    if (strcmp(argv[i], "-I") == 0)
      {
      interactive = true;
      }
    }

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

  boneWidget->SetInteractor(renderWindowInteractor);
  //Test Line
  boneWidget->CreateDefaultRepresentation();
  boneWidget->GetBoneRepresentation()->GetEnvelope()->GetProperty()->SetOpacity(0.4);

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

  if (interactive)
    {
    // Begin mouse interaction
    renderWindowInteractor->Start();
    }

  return EXIT_SUCCESS;
}

