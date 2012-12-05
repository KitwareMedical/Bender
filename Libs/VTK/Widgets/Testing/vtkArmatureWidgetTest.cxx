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

#include <vtkCollection.h>
#include <vtkCommand.h>
#include <vtkObjectFactory.h>
#include <vtkSmartPointer.h>

#include "vtkArmatureWidget.h"
#include "vtkBenderWidgetTestHelper.h"
#include "vtkBoneWidget.h"
#include "vtkCylinderBoneRepresentation.h"
#include "vtkDoubleConeBoneRepresentation.h"

int vtkArmatureWidgetTest(int, char *[])
{
  int errors = 0;
  int sectionErrors = 0;

  // Create armature
  vtkSmartPointer<vtkArmatureWidget> arm =
    vtkSmartPointer<vtkArmatureWidget>::New();

  // Create spy
  vtkSmartPointer<vtkSpy> spy = vtkSmartPointer<vtkSpy>::New();
  //spy->Verbose = true;
  arm->AddObserver(vtkCommand::AnyEvent, spy);

  //
  // Add/Create bone root
  //

  spy->ClearEvents();
  vtkBoneWidget* root = arm->CreateBone(NULL, "Root");
  sectionErrors += spy->CalledEvents.size() != 0;
  sectionErrors += root->GetName() != "Root";

  spy->ClearEvents();
  arm->AddBone(root, NULL);
  sectionErrors += root->GetReferenceCount() != 2;
  sectionErrors += arm->HasBone(root) != true;
  sectionErrors += arm->GetBoneParent(root) != NULL;
  sectionErrors += arm->GetBoneByName("Root") != root;
  sectionErrors += spy->CalledEvents[0] != vtkArmatureWidget::BoneAdded;
  sectionErrors += spy->CalledEvents[1] != vtkCommand::ModifiedEvent;
  sectionErrors += spy->CalledEvents.size() != 2;

  double tail[3] = {0.5, 0.0, 0.0};
  root->SetWorldTailRest(tail);

  if (sectionErrors > 0)
    {
    std::cout<<"There were "<<sectionErrors
      <<" while testing the addition/creation of the root."<<std::endl;
    }
  errors += sectionErrors;

  //
  // Add/Create normal bone
  //
  sectionErrors = 0;

  spy->ClearEvents();
  vtkBoneWidget* child = arm->CreateBone(root, "first Child");
  sectionErrors += spy->CalledEvents.size() != 0;

  spy->ClearEvents();
  arm->AddBone(child, root);
  sectionErrors += child->GetReferenceCount() != 2;
  sectionErrors += arm->HasBone(child) != true;
  sectionErrors += arm->GetBoneParent(child) != root;
  sectionErrors += arm->GetBoneByName("first Child") != child;
  sectionErrors += arm->GetBoneLinkedWithParent(child) != true;
  sectionErrors += arm->IsBoneDirectParent(child, root) != true;
  sectionErrors += arm->IsBoneParent(child, root) != true;
  sectionErrors += spy->CalledEvents[0] != vtkArmatureWidget::BoneAdded;
  sectionErrors += spy->CalledEvents[1] != vtkCommand::ModifiedEvent;
  sectionErrors += spy->CalledEvents.size() != 2;
  sectionErrors += CompareVector3(child->GetWorldHeadRest(),
    root->GetWorldTailRest()) != true;

  double head[3] = {2.0, 0.0, 0.0};
  child->SetWorldHeadRest(head);
  sectionErrors += CompareVector3(child->GetWorldHeadRest(), tail) != true;
  sectionErrors += CompareVector3(root->GetWorldTailRest(), tail) != true;

  root->SetWorldTailRest(head);
  sectionErrors += CompareVector3(child->GetWorldHeadRest(), head) != true;
  sectionErrors += CompareVector3(root->GetWorldTailRest(), head) != true;

  child->SetWorldTailRest(2.0, 1.0, 0.0);

  if (sectionErrors > 0)
    {
    std::cout<<"There were "<<sectionErrors
      <<" while testing the addition/creation of the first child."<<std::endl;
    }
  errors += sectionErrors;

  //
  // Add/Create second bone
  //
  sectionErrors = 0;

  spy->ClearEvents();
  tail[0] = 3.0; tail[0] = 1.0; tail[0] = 0.0;
  vtkBoneWidget* secondChild = arm->CreateBone(child, tail, "Second child");
  sectionErrors += spy->CalledEvents.size() != 0;
  sectionErrors += CompareVector3(secondChild->GetWorldHeadRest(),
    child->GetWorldTailRest()) != true;
  sectionErrors += CompareVector3(secondChild->GetWorldTailRest(), tail) != true;

  spy->ClearEvents();
  arm->AddBone(secondChild, child);
  sectionErrors += secondChild->GetReferenceCount() != 2;
  sectionErrors += arm->HasBone(secondChild) != true;
  sectionErrors += arm->GetBoneParent(secondChild) != child;
  sectionErrors += arm->GetBoneByName("Second child") != secondChild;
  sectionErrors += arm->GetBoneLinkedWithParent(secondChild) != true;
  sectionErrors += arm->IsBoneDirectParent(secondChild, root) != false;
  sectionErrors += arm->IsBoneParent(secondChild, root) != true;
  sectionErrors += spy->CalledEvents[0] != vtkArmatureWidget::BoneAdded;
  sectionErrors += spy->CalledEvents[1] != vtkCommand::ModifiedEvent;
  sectionErrors += spy->CalledEvents.size() != 2;

  if (sectionErrors > 0)
    {
    std::cout<<"There were "<<sectionErrors
      <<" while testing the addition/creation of the first child."<<std::endl;
    }
  errors += sectionErrors;

  //
  // Remove parent bone
  //
  sectionErrors = 0;

  // Add bones to be removed
  vtkBoneWidget* toBeRemovedParent = arm->CreateBone(secondChild, "toBeRemovedParent");
  arm->AddBone(toBeRemovedParent, child);

  tail[0] = 12.0; tail[0] = -38.0; tail[0] = 0.001;
  toBeRemovedParent->SetWorldTailRest(tail);

  vtkBoneWidget* toBeRemovedLeaf = arm->CreateBone(secondChild, "toBeRemovedLeaf");
  arm->AddBone(toBeRemovedLeaf, toBeRemovedParent);
  toBeRemovedLeaf->GetWorldTailRest(tail);

  // Test
  spy->ClearEvents();
  sectionErrors += arm->RemoveBone(toBeRemovedParent) != true;
  sectionErrors += toBeRemovedParent->GetReferenceCount() != 1;
  sectionErrors += arm->HasBone(toBeRemovedParent) != false;
  sectionErrors += arm->GetBoneParent(toBeRemovedParent) != NULL;
  sectionErrors += arm->GetBoneByName("toBeRemovedParent") != NULL;
  sectionErrors += arm->GetBoneLinkedWithParent(toBeRemovedParent) != false;
  sectionErrors += arm->IsBoneDirectParent(toBeRemovedParent, root) != false;
  sectionErrors += arm->IsBoneParent(toBeRemovedParent, root) != false;
  sectionErrors += spy->CalledEvents[0] != vtkArmatureWidget::BoneRemoved;
  sectionErrors += spy->CalledEvents[1] != vtkCommand::ModifiedEvent;
  sectionErrors += spy->CalledEvents.size() != 2;

  sectionErrors += arm->HasBone(toBeRemovedLeaf) != true;
  sectionErrors += arm->GetBoneParent(toBeRemovedLeaf) != child;
  sectionErrors += arm->GetBoneLinkedWithParent(toBeRemovedLeaf) != true;
  sectionErrors += CompareVector3(toBeRemovedLeaf->GetWorldHeadRest(),
    child->GetWorldTailRest()) != true;
  sectionErrors += CompareVector3(toBeRemovedLeaf->GetWorldTailRest(), tail) != true;

  if (sectionErrors > 0)
    {
    std::cout<<"There were "<<sectionErrors
      <<" while testing the removal of a parent bone."<<std::endl;
    }
  errors += sectionErrors;

  //
  // Remove leaf bone
  //
  sectionErrors = 0;

  spy->ClearEvents();
  sectionErrors += arm->RemoveBone(toBeRemovedLeaf) != true;
  sectionErrors += toBeRemovedLeaf->GetReferenceCount() != 1;
  sectionErrors += arm->HasBone(toBeRemovedLeaf) != false;
  sectionErrors += spy->CalledEvents[0] != vtkArmatureWidget::BoneRemoved;
  sectionErrors += spy->CalledEvents[1] != vtkCommand::ModifiedEvent;
  sectionErrors += spy->CalledEvents.size() != 2;

  if (sectionErrors > 0)
    {
    std::cout<<"There were "<<sectionErrors
      <<" while testing the removal of a leaf bone."<<std::endl;
    }
  errors += sectionErrors;

  spy->Verbose = false;
  root->Delete();
  child->Delete();
  secondChild->Delete();
  toBeRemovedLeaf->Delete();
  toBeRemovedParent->Delete();
  if (errors > 0)
    {
    std::cout<<"Test failed with "<<errors<<" errors."<<std::endl;
    return EXIT_FAILURE;
    }
  else
    {
    std::cout<<"Basic Armature test passed !"<<std::endl;
    }

  return EXIT_SUCCESS;
}
