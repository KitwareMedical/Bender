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
#include <vtkCallbackCommand.h>

#include <vtkInteractorStyleTrackballCamera.h>

#include "vtkArmatureWidget.h"
#include "vtkBoneWidget.h"
#include "vtkCylinderBoneRepresentation.h"
#include "vtkDoubleConeBoneRepresentation.h"

// Define interaction style
class ArmatureTestKeyPressInteractorStyle : public vtkInteractorStyleTrackballCamera
{
  public:
    static ArmatureTestKeyPressInteractorStyle* New();
    vtkTypeMacro(ArmatureTestKeyPressInteractorStyle, vtkInteractorStyleTrackballCamera);

    virtual void OnKeyPress()
      {
      vtkRenderWindowInteractor *rwi = this->Interactor;
      std::string key = rwi->GetKeySym();
      std::cout<<"Key Pressed: "<<key<<std::endl;

      if (key == "Shift_L")
        {
        std::cout<<"Changing representation !"<<std::endl;

        // Cycling through the representations
        int newRepresentationType = Armature->GetBonesRepresentationType() + 1;
        if (newRepresentationType > vtkArmatureWidget::DoubleCone
            || newRepresentationType < vtkArmatureWidget::Bone) //last case is case no representation
          {
          newRepresentationType = 0;
          }
        Armature->SetBonesRepresentation(newRepresentationType);
        }

      //if (key == "a")
        //{
        //vtkSmartPointer<vtkTransform> transform =
        //  vtkSmartPointer<vtkTransform>::New();
        //transform->Translate(Armature->GetBone(1)->GetParentToBoneRestTranslation());

        //transform->Concatenate(Armature->GetBone(1)->CreateParentToBoneRestRotation());

        //Axes->SetUserTransform(transform);
        //}
      if (key == "Control_L")
        {
          std::cout<<"Setting widget state to "<<! Armature->GetWidgetState()<<std::endl;
        Armature->SetWidgetState( ! Armature->GetWidgetState() );
        }
      else if (key == "Tab")
        {
        int state = Armature->GetShowAxes() + 1;
        if (state > vtkBoneWidget::ShowPoseTransform)
          {
          state = 0;
          }
        Armature->SetShowAxes(state);
        }
      }

  vtkArmatureWidget* Armature;
  //vtkAxesActor* Axes;
};

void PrintInteractionEvent(vtkObject*, unsigned long eid, void* clientdata, void *calldata)
{
  std::cout<<"Interaction Event !"<<std::endl;
}

vtkStandardNewMacro(ArmatureTestKeyPressInteractorStyle);

int vtkArmatureWidgetTest(int, char *[])
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

  vtkSmartPointer<vtkArmatureWidget> arm =
    vtkSmartPointer<vtkArmatureWidget>::New();
  arm->SetInteractor(renderWindowInteractor);
  arm->SetCurrentRenderer(renderer);
  arm->CreateDefaultRepresentation();

  // Add root bone
  vtkBoneWidget* root = arm->CreateBone(NULL, "Root");
  arm->AddBone(root, NULL);
  root->Delete();
  root->SetWorldHeadRest(0.0, 0.0, 0.0);
  root->SetWorldTailRest(0.5, 0.0, 0.0);

  arm->SetBonesRepresentation(vtkArmatureWidget::Bone);

  vtkBoneWidget* child = arm->CreateBone(root, "first Child (Red)");
  arm->AddBone(child, root);
  child->Delete();
  child->SetWorldHeadRest(2.0, 0.0, 0.0);
  child->SetWorldTailRest(2.0, 1.0, 0.0);

  if ( !child->GetBoneRepresentation() || !root->GetBoneRepresentation() )
    {
    std::cerr<<"There should be representation for each bone"<<std::endl;
    return EXIT_FAILURE;
    }
  child->GetBoneRepresentation()->GetLineProperty()->SetColor(1.0, 0.0, 0.0);

  vtkBoneWidget* finalChild = arm->CreateBone(child, 3.0, 1.0, 0.0, "Grand child (Green)");
  arm->AddBone(finalChild, child);
  finalChild->Delete();
  if (finalChild == NULL)
    {
    std::cerr<<"There should be a bone with id 2"<<std::endl;
    return EXIT_FAILURE;
    }
  finalChild->GetBoneRepresentation()->GetLineProperty()->SetColor(0.0, 1.0, 0.0);

  vtkBoneWidget* toBeRemovedChild = arm->CreateBone(finalChild, "toBeRemovedChild");
  if (toBeRemovedChild == NULL)
    {
    std::cerr<<"There should be a bone with id 3"<<std::endl;
    return EXIT_FAILURE;
    }
  arm->AddBone(toBeRemovedChild);
  toBeRemovedChild->Delete();
  if (arm->GetBoneLinkedWithParent(toBeRemovedChild) != false)
    {
    std::cerr<<"The bones should be unlinked !"<<std::endl;
    return EXIT_FAILURE;
    }
  arm->SetBoneLinkedWithParent(toBeRemovedChild, true);
  if (arm->GetBoneLinkedWithParent(toBeRemovedChild) != true)
    {
    std::cerr<<"The bones should be linked !"<<std::endl;
    return EXIT_FAILURE;
    }

  // Delete bone
  vtkSmartPointer<vtkBoneWidget> fake = vtkSmartPointer<vtkBoneWidget>::New();
  if (arm->RemoveBone(fake))
    {
    std::cerr<<"Deleting a fake bone should have failed"<<std::endl;
    return EXIT_FAILURE;
    }
  if (! arm->RemoveBone(toBeRemovedChild))
    {
    std::cerr<<"Deleting a bone child shouldn't have failed"<<std::endl;
    return EXIT_FAILURE;
    }

  arm->SetShowParenthood(false);
  if (finalChild->GetShowParenthood())
    {
    std::cerr<<"Parenthood for bone with shouldn't be active"<<std::endl;
    return EXIT_FAILURE;
    }
  finalChild->SetShowParenthood(true);
  arm->SetShowParenthood(true);
  if (! finalChild->GetShowParenthood())
    {
    std::cerr<<"Parenthood for bone should be active"<<std::endl;
    return EXIT_FAILURE;
    }

  // Setup callbacks
  vtkSmartPointer<ArmatureTestKeyPressInteractorStyle> style =
    vtkSmartPointer<ArmatureTestKeyPressInteractorStyle>::New();
  renderWindowInteractor->SetInteractorStyle(style);
  style->Armature = arm;

  vtkSmartPointer<vtkAxesActor> axes =
    vtkSmartPointer<vtkAxesActor>::New();

  vtkSmartPointer<vtkOrientationMarkerWidget> axesWidget =
    vtkSmartPointer<vtkOrientationMarkerWidget>::New();
  axesWidget->SetOrientationMarker( axes );
  axesWidget->SetInteractor( renderWindowInteractor );
  axesWidget->On();

  /*vtkSmartPointer<vtkCallbackCommand> callback = 
    vtkSmartPointer<vtkCallbackCommand>::New();
  callback->SetCallback ( PrintInteractionEvent );
  root->AddObserver(vtkCommand::InteractionEvent, callback);*/

  // Render
  renderWindow->Render();
  renderWindowInteractor->Initialize();
  renderWindow->Render();
  arm->On();

  // Begin mouse interaction
  renderWindowInteractor->Start();

  return EXIT_SUCCESS;
}
