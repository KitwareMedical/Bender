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

#include <vtkCommand.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkSmartPointer.h>

#include <vtkInteractorStyleTrackballCamera.h>

#include "vtkArmatureWidget.h"
#include "vtkBoneWidget.h"
#include "vtkCylinderBoneRepresentation.h"
#include "vtkDoubleConeBoneRepresentation.h"


class Test1KeyPressInteractorStyle : public vtkInteractorStyleTrackballCamera
{
  public:
    static Test1KeyPressInteractorStyle* New();
    vtkTypeMacro(Test1KeyPressInteractorStyle, vtkInteractorStyleTrackballCamera);

    virtual void OnKeyPress()
      {
      vtkRenderWindowInteractor *rwi = this->Interactor;
      std::string key = rwi->GetKeySym();

      if (key == "Control_L")
        {
        int widgetState = this->Armature->GetWidgetState();
        if ( widgetState == vtkArmatureWidget::Rest )
          {
          widgetState = vtkArmatureWidget::Pose;
          }
        else
          {
          widgetState = vtkArmatureWidget::Rest;
          }

        this->Armature->SetWidgetState(widgetState);
        }
      else if (key == "a")
        {
        int state = this->Armature->GetShowAxes() + 1;
        if (state > vtkBoneWidget::ShowPoseTransform)
          {
          state = 0;
          }
        this->Armature->SetShowAxes(state);
        }
      else if (key == "space")
        {
        this->Armature->GetBonesRepresentation()->SetAlwaysOnTop(
          !this-> Armature->GetBonesRepresentation()->GetAlwaysOnTop());
        }
      }

  vtkArmatureWidget* Armature;
};

vtkStandardNewMacro(Test1KeyPressInteractorStyle);

int vtkArmatureWidgetArmTest(int argc, char* argv[])
{
  bool interactive = false;
  for (int i = 0; i < argc; ++i)
    {
    if (strcmp(argv[i], "-I") == 0)
      {
      interactive = true;
      }
    }

  vtkSmartPointer<vtkRenderer> renderer =
    vtkSmartPointer<vtkRenderer>::New();
  vtkSmartPointer<vtkRenderWindow> renderWindow =
    vtkSmartPointer<vtkRenderWindow>::New();
  renderWindow->AddRenderer(renderer);

  // An interactor
  vtkSmartPointer<vtkRenderWindowInteractor> renderWindowInteractor =
    vtkSmartPointer<vtkRenderWindowInteractor>::New();
  renderWindowInteractor->SetRenderWindow(renderWindow);

  vtkSmartPointer<vtkArmatureWidget> armature =
    vtkSmartPointer<vtkArmatureWidget>::New();
  armature->SetInteractor(renderWindowInteractor);
  armature->SetCurrentRenderer(renderer);
  armature->CreateDefaultRepresentation();
  vtkBoneRepresentation* defaultRep = vtkBoneRepresentation::New();
  armature->SetBonesRepresentation(defaultRep);
  defaultRep->Delete();

  vtkBoneWidget* biceps = armature->CreateBone(NULL, "Biceps");
  armature->AddBone(biceps, NULL);
  biceps->SetWorldHeadRest(0.0, 0.0, 0.0);
  biceps->SetWorldTailRest(10.0, 0.0, 0.0);
  vtkSmartPointer<vtkCylinderBoneRepresentation> bicepsRep =
    vtkSmartPointer<vtkCylinderBoneRepresentation>::New();
  biceps->SetRepresentation(bicepsRep);

  vtkBoneWidget* fore = armature->CreateBone(biceps, 20.0, 10.0, 0.0, "fore");
  armature->AddBone(fore, biceps);

  vtkBoneWidget* arm = armature->CreateBone(fore, 20.0, 0.0, 0.0, "arm");
  armature->AddBone(arm, NULL);

  vtkBoneWidget* thumb = armature->CreateBone(arm, 20.0, 4.0, 0.0, "thumb");
  armature->AddBone(thumb, arm);
  //thumb->SetShowAxes(vtkBoneWidget::ShowPoseTransform);

  vtkBoneWidget* indexFinger =
    armature->CreateBone(arm, 22.0, 2.0, 0.0, "index finger");
  armature->AddBone(indexFinger, arm);
  //indexFinger->SetShowAxes(vtkBoneWidget::ShowPoseTransform);

  armature->ReparentBone(arm, fore);
  if (! armature->IsBoneDirectParent(arm, fore))
    {
    std::cout<<"Arm parent should be Fore !"<<std::endl;
    return EXIT_FAILURE;
    }

  vtkBoneWidget* forearm = armature->MergeBones(fore, arm);
  vtkSmartPointer<vtkDoubleConeBoneRepresentation> forearmRep =
    vtkSmartPointer<vtkDoubleConeBoneRepresentation>::New();
  forearm->SetRepresentation(forearmRep);
  fore->Delete();
  arm->Delete();

  vtkBoneWidget* middleFinger =
    armature->CreateBone(forearm, 22.0, 1.0, 0.0, "middle finger");
  armature->AddBone(middleFinger, forearm);
  //middleFinger->SetShowAxes(vtkBoneWidget::ShowPoseTransform);

  vtkBoneWidget* ringFinger =
    armature->CreateBone(forearm, 22.0, -1.0, 0.0, "ring finger");
  armature->AddBone(ringFinger, forearm);
  //ringFinger->SetShowAxes(vtkBoneWidget::ShowPoseTransform);

  vtkBoneWidget* littleFinger =
    armature->CreateBone(forearm, 22.0, -2.0, 0.0, "little finger");
  armature->AddBone(littleFinger, forearm);
  //littleFinger->SetShowAxes(vtkBoneWidget::ShowPoseTransform);

  if (! armature->IsBoneParent(littleFinger, biceps))
    {
    std::cout<<"Little finger should be inderectly "
      <<"related to the biceps"<<std::endl;
    return EXIT_FAILURE;
    }

  biceps->Delete();
  thumb->Delete();
  indexFinger->Delete();
  middleFinger->Delete();
  ringFinger->Delete();
  littleFinger->Delete();

  vtkSmartPointer<Test1KeyPressInteractorStyle> style =
    vtkSmartPointer<Test1KeyPressInteractorStyle>::New();
  renderWindowInteractor->SetInteractorStyle(style);
  style->Armature = armature;

  // Render
  renderWindow->Render();
  renderWindowInteractor->Initialize();
  renderWindow->Render();
  armature->On();

  if (interactive)
    {
    // Begin mouse interaction
    renderWindowInteractor->Start();
    }

  return EXIT_SUCCESS;
}

