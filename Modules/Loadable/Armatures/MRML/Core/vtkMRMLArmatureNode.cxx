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
#include "vtkMRMLNodeHelper.h"

// MRML includes
#include <vtkMRMLScene.h>

// VTK includes
#include <vtkArmatureRepresentation.h>
#include <vtkBoneRepresentation.h>
#include <vtkBoneWidget.h>
#include <vtkCallbackCommand.h>
#include <vtkCollection.h>
#include <vtkCylinderBoneRepresentation.h>
#include <vtkDoubleConeBoneRepresentation.h>
#include <vtkMathUtilities.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkProperty.h>
#include <vtkWidgetRepresentation.h>

//----------------------------------------------------------------------------
namespace
{

int FindBonesRepresentationType(vtkBoneRepresentation* r)
{
  if (vtkDoubleConeBoneRepresentation::SafeDownCast(r))
    {
    return vtkMRMLArmatureNode::Octohedron;
    }
  else if (vtkCylinderBoneRepresentation::SafeDownCast(r))
    {
    return vtkMRMLArmatureNode::Cylinder;
    }

  return vtkMRMLArmatureNode::Bone;
}

} // end namespace


//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLArmatureNode);

//----------------------------------------------------------------------------
static void MRMLArmatureNodeCallback(vtkObject* vtkNotUsed(caller),
                                     long unsigned int eventId,
                                     void* clientData,
                                     void* vtkNotUsed(callData))
{
  if (eventId == vtkCommand::ModifiedEvent)
    {
    vtkMRMLArmatureNode* node =
      reinterpret_cast<vtkMRMLArmatureNode*>(clientData);
    if (node)
      {
      node->Modified();
      }
    }
}

//----------------------------------------------------------------------------
vtkMRMLArmatureNode::vtkMRMLArmatureNode()
{
  this->ArmatureProperties = vtkArmatureWidget::New();
  this->ArmatureProperties->CreateDefaultRepresentation();
  vtkNew<vtkDoubleConeBoneRepresentation> newRep;
  this->ArmatureProperties->SetBonesRepresentation(newRep.GetPointer());
  this->BonesRepresentationType = FindBonesRepresentationType(
    this->ArmatureProperties->GetBonesRepresentation());

  this->WidgetState = vtkMRMLArmatureNode::Rest;
  this->SetHideFromEditors(0);
  this->Callback = vtkCallbackCommand::New();

  this->ArmatureProperties->GetArmatureRepresentation()->GetProperty()
    ->SetColor(67.0 / 255.0, 75.0/255.0, 89.0/255.0); //Slicer's bone color.

  this->ArmatureProperties->GetArmatureRepresentation()->GetProperty()
    ->SetOpacity(1.0);

  this->SetBonesAlwaysOnTop(1);

  this->ShouldResetPoseMode = 0;

  this->Callback->SetClientData(this);
  this->Callback->SetCallback(MRMLArmatureNodeCallback);
  this->ArmatureProperties->AddObserver(vtkCommand::ModifiedEvent,
    this->Callback);
}

//----------------------------------------------------------------------------
vtkMRMLArmatureNode::~vtkMRMLArmatureNode()
{
  this->Callback->Delete();
  this->ArmatureProperties->Delete();
}

//----------------------------------------------------------------------------
void vtkMRMLArmatureNode::WriteXML(ostream& of, int nIndent)
{
  this->Superclass::WriteXML(of, nIndent);

  vtkIndent indent(nIndent);
  of << indent << " BonesRepresentationType=\""
    << this->BonesRepresentationType << "\"";
  of << indent << " ShowAxes=\""
    << this->ArmatureProperties->GetShowAxes() << "\"";
  of << indent << " ShowParenthood=\""
    << this->ArmatureProperties->GetShowParenthood() << "\"";

  of << indent << " Visibility=\"" << this->GetVisibility() << "\"";
  of << indent << " Opacity=\"" << this->GetOpacity() << "\"";
  of << indent << " Color=";
  double rgb[3];
  this->GetColor(rgb);
  vtkMRMLNodeHelper::PrintQuotedVector3(of, rgb);
  of << indent << " BonesAlwaysOnTop=\""
    << this->GetBonesAlwaysOnTop() << "\"";
}

//----------------------------------------------------------------------------
void vtkMRMLArmatureNode::ReadXMLAttributes(const char** atts)
{
  int disabledModify = this->StartModify();

  this->Superclass::ReadXMLAttributes(atts);

  this->SetWidgetState(vtkMRMLArmatureNode::Rest);
  while (*atts != NULL)
    {
    const char* attName = *(atts++);
    std::string attValue(*(atts++));

    if (!strcmp(attName, "BonesRepresentationType"))
      {
      this->SetBonesRepresentationType(
        vtkMRMLNodeHelper::StringToInt(attValue));
      }
    else if (!strcmp(attName, "ShowAxes"))
      {
      this->SetShowAxes(
        vtkMRMLNodeHelper::StringToInt(attValue));
      }
    else if (!strcmp(attName, "ShowParenthood"))
      {
      this->SetShowParenthood(
        vtkMRMLNodeHelper::StringToInt(attValue));
      }
    else if (!strcmp(attName, "Visibility"))
      {
      this->SetVisibility(
        vtkMRMLNodeHelper::StringToInt(attValue));
      }
    else if (!strcmp(attName, "Opacity"))
      {
      this->SetOpacity(
        vtkMRMLNodeHelper::StringToInt(attValue));
      }
    else if (!strcmp(attName, "BonesAlwaysOnTop"))
      {
      this->SetBonesAlwaysOnTop(
        vtkMRMLNodeHelper::StringToInt(attValue));
      }
    else if (!strcmp(attName, "Color"))
      {
      double rgb[3];
      vtkMRMLNodeHelper::StringToVector3(attValue, rgb);
      this->SetColor(rgb);
      }
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
void vtkMRMLArmatureNode::SetName(const char* name)
{
  vtkMRMLModelNode* armatureModel = this->GetArmatureModel();
  if (armatureModel)
    {
    this->GetArmatureModel()->SetName(name);
    }
  this->Superclass::SetName(name);
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureNode::SetBonesRepresentation(vtkBoneRepresentation* rep)
{
  this->SetBonesRepresentationType(FindBonesRepresentationType(rep));
}

//---------------------------------------------------------------------------
vtkBoneRepresentation* vtkMRMLArmatureNode::GetBonesRepresentation()
{
  return this->ArmatureProperties->GetBonesRepresentation();
}

//---------------------------------------------------------------------------
int vtkMRMLArmatureNode::GetBonesRepresentationType()
{
  return this->BonesRepresentationType;
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureNode::SetBonesRepresentationType(int type)
{
  if (type == this->BonesRepresentationType)
    {
    return;
    }

  this->BonesRepresentationType = type;
  vtkBoneRepresentation* r = 0;
  if (type == vtkMRMLArmatureNode::Octohedron)
    {
    r = vtkDoubleConeBoneRepresentation::New();
    }
  else if (type == vtkMRMLArmatureNode::Cylinder)
    {
    r =  vtkCylinderBoneRepresentation::New();
    }
  else
    {
    r = vtkBoneRepresentation::New();
    }

  r->DeepCopy(this->GetBonesRepresentation());
  this->ArmatureProperties->SetBonesRepresentation(r);
  r->Delete();
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
  if (onTop == this->GetBonesAlwaysOnTop())
    {
    return;
    }

  this->ArmatureProperties->GetBonesRepresentation()
    ->SetAlwaysOnTop(onTop);
  this->Modified();
}

//---------------------------------------------------------------------------
int vtkMRMLArmatureNode::GetBonesAlwaysOnTop()
{
  return this->ArmatureProperties->GetBonesRepresentation()->GetAlwaysOnTop();
}

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
  this->SetBonesRepresentation(armatureWidget->GetBonesRepresentation());
  this->WidgetState = armatureWidget->GetWidgetState();
  this->ArmatureProperties->SetShowAxes(
    armatureWidget->GetShowAxes());
  this->ArmatureProperties->SetShowParenthood(
    armatureWidget->GetShowParenthood());
  this->SetBonesAlwaysOnTop(
    armatureWidget->GetBonesRepresentation()->GetAlwaysOnTop());

  this->SetOpacity(
    armatureWidget->GetBonesRepresentation()->GetLineProperty()->GetOpacity());
  this->SetColor(
    armatureWidget->GetBonesRepresentation()->GetLineProperty()->GetColor());
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
  if (FindBonesRepresentationType(armatureWidget->GetBonesRepresentation())
      != this->BonesRepresentationType)
    {
    if (this->BonesRepresentationType == 1)
      {
      vtkNew<vtkCylinderBoneRepresentation> rep;
      this->UpdateBoneRepresentation(rep.GetPointer());
      armatureWidget->SetBonesRepresentation(rep.GetPointer());
      }
    else if (this->BonesRepresentationType == 2)
      {
      vtkNew<vtkDoubleConeBoneRepresentation> rep;
      this->UpdateBoneRepresentation(rep.GetPointer());
      armatureWidget->SetBonesRepresentation(rep.GetPointer());
      }
    else
      {
      vtkNew<vtkBoneRepresentation> rep;
      this->UpdateBoneRepresentation(rep.GetPointer());
      armatureWidget->SetBonesRepresentation(rep.GetPointer());
      }

    }

  armatureWidget->SetWidgetState(this->WidgetState);
  armatureWidget->SetShowAxes(
    this->ArmatureProperties->GetShowAxes());
  armatureWidget->GetBonesRepresentation()->SetAlwaysOnTop(
    this->GetBonesAlwaysOnTop());

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

      // Color and opacity are tricky.
      // Each display node needs to be updated as well as the
      // armature bones representation
      vtkMRMLBoneDisplayNode* boneDisplayNode = boneNode->GetBoneDisplayNode();
      if (boneDisplayNode)
        {
        boneDisplayNode->SetColor(color);
        boneDisplayNode->SetOpacity(this->GetOpacity());
        }
      }
    }

  this->UpdateBoneRepresentation(armatureWidget->GetBonesRepresentation());

  if (this->ShouldResetPoseMode)
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

//---------------------------------------------------------------------------
void vtkMRMLArmatureNode::UpdateBoneRepresentation(vtkBoneRepresentation* rep)
{
  double color[3];
  this->GetColor(color);

  rep->SetAlwaysOnTop(this->GetBonesAlwaysOnTop());
  rep->SetOpacity(this->GetOpacity());
  rep->GetLineProperty()->SetColor(color);
}
