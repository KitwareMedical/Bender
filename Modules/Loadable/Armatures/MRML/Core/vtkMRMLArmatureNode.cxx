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
#include <vtkArmatureRepresentation.h>
#include <vtkBoneRepresentation.h>
#include <vtkBoneWidget.h>
#include <vtkCollection.h>
#include <vtkCylinderBoneRepresentation.h>
#include <vtkDoubleConeBoneRepresentation.h>
#include <vtkMathUtilities.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkProperty.h>
#include <vtkWidgetRepresentation.h>

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLArmatureNode);

//----------------------------------------------------------------------------
class vtkMRMLArmatureNodeCallback : public vtkCommand
{
public:
  static vtkMRMLArmatureNodeCallback *New()
    { return new vtkMRMLArmatureNodeCallback; }

  vtkMRMLArmatureNodeCallback()
    { this->Node = 0; }

  virtual void Execute(vtkObject* caller, unsigned long eventId, void* data)
    {
    vtkNotUsed(data);
    switch (eventId)
      {
      case vtkCommand::ModifiedEvent:
        {
        this->Node->Modified();
        break;
        }
      }
    }

  vtkMRMLArmatureNode* Node;
};

//----------------------------------------------------------------------------
vtkMRMLArmatureNode::vtkMRMLArmatureNode()
{
  this->ArmatureProperties = vtkArmatureWidget::New();
  this->ArmatureProperties->CreateDefaultRepresentation();
  this->ArmatureProperties->SetBonesRepresentation(
    vtkArmatureWidget::DoubleCone);
  this->WidgetState = vtkArmatureWidget::Rest;
  this->SetHideFromEditors(0);
  this->Callback = vtkMRMLArmatureNodeCallback::New();

  this->ArmatureProperties->GetArmatureRepresentation()->GetProperty()
    ->SetColor(67.0 / 255.0, 75.0/255.0, 89.0/255.0); //Slicer's bone color.

  this->ArmatureProperties->GetArmatureRepresentation()->GetProperty()
    ->SetOpacity(1.0);

  this->ArmatureProperties->SetBonesAlwaysOnTop(1);

  this->ShouldResetPoseMode = 0;

  this->Callback->Node = this;
  this->ArmatureProperties->AddObserver(vtkCommand::ModifiedEvent,
    this->Callback);
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
  return this->WidgetState;
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
void vtkMRMLArmatureNode::SetOpacity(double opacity)
{
  this->ArmatureProperties->GetArmatureRepresentation()->GetProperty()
    ->SetOpacity(opacity);
  this->Modified();
}

//---------------------------------------------------------------------------
double vtkMRMLArmatureNode::GetOpacity()
{
  return this->ArmatureProperties->GetArmatureRepresentation()->GetProperty()
    ->GetOpacity();
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureNode::SetColor(double rgb[3])
{
  this->ArmatureProperties->GetArmatureRepresentation()->GetProperty()
    ->SetColor(rgb);
  this->Modified();
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureNode::GetColor(double rgb[3])
{
  this->ArmatureProperties->GetArmatureRepresentation()->GetProperty()
    ->GetColor(rgb);
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureNode::SetBonesAlwaysOnTop(int onTop)
{
  this->ArmatureProperties->SetBonesAlwaysOnTop(onTop);
}

//---------------------------------------------------------------------------
int vtkMRMLArmatureNode::GetBonesAlwaysOnTop()
{
  return this->ArmatureProperties->GetBonesAlwaysOnTop();
}

/*
//---------------------------------------------------------------------------
void vtkMRMLArmatureNode
::SetBoneLinkedWithParent(vtkBoneWidget* bone, bool linked)
{
  if (CompareColor(this->Color, rgb))
    {
    return;
    }

  for (int i=0; i<3; ++i)
    {
    this->Color[i] = rgb[i];
    }

  double doubleRGB[3];
  doubleRGB[0] = rgb[0]/255.0;
  doubleRGB[1] = rgb[1]/255.0;
  doubleRGB[2] = rgb[2]/255.0;

  // vv This should go to logic vv
  for (int i = 0; i < bones->GetNumberOfItems();++i)
    {
    vtkBoneWidget* bone
      = vtkBoneWidget::Safedowncast(bones->GetItemAsObject(i));
    if (bone && bone->GetBoneRepresentation())
      {
      bone->GetBoneRepresenation()->GetLineProperty()->SetColor(doubleRGB);

      if (vtkCylinderBoneRepresentation::SafeDownCast(
          bone->GetBoneRepresentation()))
        {
        vtkCylinderBoneRepresentation::SafeDownCast(
          bone->GetBoneRepresentation())->GetCylinderProperty()
            ->SetColor(doubleRGB);
        }
      else if (vtkDoubleConeBoneRepresentation::SafeDownCast(
          bone->GetBoneRepresentation()))
        {
        vtkDoubleConeBoneRepresentation::SafeDownCast(
          bone->GetBoneRepresentation())->GetConesProperty()
            ->SetColor(doubleRGB);
        }
      }
    }
  // ^^ This should go to logic ^^

  this->Modified();
}

//---------------------------------------------------------------------------
bool vtkMRMLArmatureNode::GetBoneLinkedWithParent(vtkBoneWidget* bone)
{
  this->ArmatureProperties->
}*/

//---------------------------------------------------------------------------
void vtkMRMLArmatureNode::ResetPoseMode()
{
  this->ShouldResetPoseMode = 1;
  this->Modified();
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureNode
::CopyArmatureWidgetProperties(vtkArmatureWidget* armatureWidget)
{
  if (!armatureWidget)
    {
    return;
    }

  //int wasModifying = this->StartModify(); // ? Probably not
  this->ArmatureProperties->SetBonesRepresentation(
    armatureWidget->GetBonesRepresentationType());
  this->WidgetState = armatureWidget->GetWidgetState();
  this->ArmatureProperties->SetShowAxes(
    armatureWidget->GetShowAxes());
  this->ArmatureProperties->SetShowParenthood(
    armatureWidget->GetShowParenthood());
  this->ArmatureProperties->SetBonesAlwaysOnTop(
    armatureWidget->GetBonesAlwaysOnTop());

  this->ArmatureProperties->GetArmatureRepresentation()->GetProperty()
    ->SetOpacity(
    armatureWidget->GetArmatureRepresentation()->GetProperty()->GetOpacity());
  this->ArmatureProperties->GetArmatureRepresentation()->GetProperty()
    ->SetColor(
    armatureWidget->GetArmatureRepresentation()->GetProperty()->GetColor());
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureNode
::PasteArmatureNodeProperties(vtkArmatureWidget* armatureWidget)
{
  if (!armatureWidget)
    {
    return;
    }

  //int wasModifying = this->StartModify(); // ? Probably not
  armatureWidget->SetBonesRepresentation(
    this->ArmatureProperties->GetBonesRepresentationType());
  armatureWidget->SetWidgetState(this->WidgetState);
  armatureWidget->SetShowAxes(
    this->ArmatureProperties->GetShowAxes());
  armatureWidget->SetBonesAlwaysOnTop(
    this->ArmatureProperties->GetBonesAlwaysOnTop());

  double color[3];
  this->GetColor(color);

  // Update it now because the display node does not listens to
  // widget representation change.
  vtkNew<vtkCollection> bones;
  this->GetAllBones(bones.GetPointer());
  for (int i = 0; i < bones->GetNumberOfItems();++i)
    {
    vtkMRMLBoneNode* boneNode =
      vtkMRMLBoneNode::SafeDownCast(bones->GetItemAsObject(i));
    if (boneNode)
      {
      boneNode->SetShowParenthood(
        this->ArmatureProperties->GetShowParenthood()
        && boneNode->GetHasParent());

      vtkMRMLBoneDisplayNode* boneDisplayNode = boneNode->GetBoneDisplayNode();
      if (boneDisplayNode)
        {
        boneDisplayNode->SetColor(color);
        boneDisplayNode->SetOpacity(this->GetOpacity());
        }
      }
    }

  if (ShouldResetPoseMode)
    {
    this->ShouldResetPoseMode = 0;
    armatureWidget->ResetPoseToRest();
    }

}

//---------------------------------------------------------------------------
void vtkMRMLArmatureNode::SetArmatureModel(vtkMRMLModelNode* model)
{
  vtkPolyData* polyData = this->GetPolyData();
  if (model)
    {
    model->SetName(this->GetName());
    model->SetAndObservePolyData(polyData);
    }
  // Prevent ModifiedEvents from being fired as the order of calls is wrong.
  int wasModifying = this->StartModify();
  this->SetAssociatedNodeID(model ? model->GetID() : 0);
  this->EndModify(wasModifying);
}

//---------------------------------------------------------------------------
vtkMRMLModelNode* vtkMRMLArmatureNode::GetArmatureModel()
{
  return vtkMRMLModelNode::SafeDownCast(this->GetAssociatedNode());
}

//---------------------------------------------------------------------------
vtkPolyData* vtkMRMLArmatureNode::GetPolyData()
{
  vtkMRMLModelNode* model = this->GetArmatureModel();
  return model ? model->GetPolyData() : 0;
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureNode::SetArmaturePolyData(vtkPolyData* polyData)
{
  vtkMRMLModelNode* model = this->GetArmatureModel();
  if (!model)
    {
    return;
    }
  model->SetAndObservePolyData(polyData);
}
