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

// Armatures includes
#include "vtkMRMLArmatureNode.h"
#include "vtkMRMLBoneDisplayNode.h"
#include "vtkMRMLBoneNode.h"

// MRML includes
#include <vtkMRMLScene.h>

// VTK includes
#include <vtkArmatureWidget.h>
#include <vtkObjectFactory.h>
#include <vtkWidgetRepresentation.h>

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLArmatureNode);

//----------------------------------------------------------------------------
vtkMRMLArmatureNode::vtkMRMLArmatureNode()
{
  this->ArmatureProperties = vtkArmatureWidget::New();
  this->ArmatureProperties->CreateDefaultRepresentation();
  this->ArmatureProperties->SetBonesRepresentation(
    vtkArmatureWidget::DoubleCone);

  this->WidgetState = vtkArmatureWidget::Rest;

  this->SetHideFromEditors(0);
}

//----------------------------------------------------------------------------
vtkMRMLArmatureNode::~vtkMRMLArmatureNode()
{
  this->ArmatureProperties->Delete();
}

//----------------------------------------------------------------------------
void vtkMRMLArmatureNode::WriteXML(ostream& of, int nIndent)
{
  this->Superclass::WriteXML(of, nIndent);
  // of << indent << " ctrlPtsNumberingScheme=\"" << this->NumberingScheme << "\"";
}

//----------------------------------------------------------------------------
void vtkMRMLArmatureNode::ReadXMLAttributes(const char** atts)
{
  int disabledModify = this->StartModify();

  this->Superclass::ReadXMLAttributes(atts);

  while (*atts != NULL)
    {
    //const char* attName = *(atts++);
    //std::string attValue(*(atts++));

    // if  (!strcmp(attName, "ctrlPtsNumberingScheme"))
    //   {
    //   std::stringstream ss;
    //   ss << attValue;
    //   ss >> this->NumberingScheme;
    //   }
    }
  this->EndModify(disabledModify);
}

//-----------------------------------------------------------
void vtkMRMLArmatureNode::UpdateScene(vtkMRMLScene *scene)
{
  this->Superclass::UpdateScene(scene);
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureNode::ProcessMRMLEvents(vtkObject* caller,
                                        unsigned long event,
                                        void* callData)
{
  this->Superclass::ProcessMRMLEvents(caller, event, callData);
}

//----------------------------------------------------------------------------
void vtkMRMLArmatureNode::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}

//----------------------------------------------------------------------------
void vtkMRMLArmatureNode::Copy(vtkMRMLNode* anode)
{
  int wasModifying = this->StartModify();
  this->Superclass::Copy(anode);
  this->EndModify(wasModifying);
}

//----------------------------------------------------------------------------
const char* vtkMRMLArmatureNode::GetIcon()
{
  return this->Superclass::GetIcon();
}

//----------------------------------------------------------------------------
vtkMRMLBoneNode* vtkMRMLArmatureNode::GetParentBone(vtkMRMLBoneNode* bone)
{
  vtkMRMLDisplayableHierarchyNode* boneHierarchyNode =
    vtkMRMLDisplayableHierarchyNode::GetDisplayableHierarchyNode(
      bone->GetScene(), bone->GetID());
  vtkMRMLDisplayableHierarchyNode* parentHierarchyNode =
    vtkMRMLDisplayableHierarchyNode::SafeDownCast(
      boneHierarchyNode->GetParentNode());
  return vtkMRMLBoneNode::SafeDownCast(
    parentHierarchyNode->GetDisplayableNode());
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureNode::SetBonesRepresentation(int representationType)
{
  this->ArmatureProperties->SetBonesRepresentation(representationType);
}

//---------------------------------------------------------------------------
int vtkMRMLArmatureNode::GetBonesRepresentation()
{
  return this->ArmatureProperties->GetBonesRepresentationType();
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureNode::SetWidgetState(int state)
{
  if (state == this->WidgetState)
    {
    return;
    }

  this->WidgetState = state;
  this->Modified();
}

//---------------------------------------------------------------------------
int vtkMRMLArmatureNode::GetWidgetState()
{
  return this->ArmatureProperties->GetWidgetState();
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureNode::SetShowAxes(int axesVisibility)
{
  this->ArmatureProperties->SetShowAxes(axesVisibility);
}

//---------------------------------------------------------------------------
int vtkMRMLArmatureNode::GetShowAxes()
{
  return this->ArmatureProperties->GetShowAxes();
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureNode::SetShowParenthood(int parenthood)
{
  this->ArmatureProperties->SetShowParenthood(parenthood);
}

//---------------------------------------------------------------------------
int vtkMRMLArmatureNode::GetShowParenthood()
{
  return this->ArmatureProperties->GetShowParenthood();
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureNode::SetVisibility(bool visible)
{
  this->ArmatureProperties->GetRepresentation()->SetVisibility(visible);
}

//---------------------------------------------------------------------------
bool vtkMRMLArmatureNode::GetVisibility()
{
  return this->ArmatureProperties->GetRepresentation()->GetVisibility();
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureNode
::CopyArmatureWidgetProperties(vtkArmatureWidget* armatureWidget)
{
  //int wasModifying = this->StartModify(); // ? Probably not
  this->ArmatureProperties->SetBonesRepresentation(
    armatureWidget->GetBonesRepresentationType());
  this->WidgetState = armatureWidget->GetWidgetState();
  this->ArmatureProperties->SetShowAxes(
    armatureWidget->GetShowAxes());
  this->ArmatureProperties->SetShowParenthood(
    armatureWidget->GetShowParenthood());
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureNode
::PasteArmatureNodeProperties(vtkArmatureWidget* armatureWidget)
{
  //int wasModifying = this->StartModify(); // ? Probably not
  armatureWidget->SetBonesRepresentation(
    this->ArmatureProperties->GetBonesRepresentationType());
  armatureWidget->SetWidgetState(this->WidgetState);
  armatureWidget->SetShowAxes(
    this->ArmatureProperties->GetShowAxes());
  armatureWidget->SetShowParenthood(
    this->ArmatureProperties->GetShowParenthood());
}
