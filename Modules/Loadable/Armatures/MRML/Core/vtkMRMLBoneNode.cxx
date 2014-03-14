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
#include "vtkMRMLBoneNode.h"
#include "vtkMRMLBoneDisplayNode.h"
//#include "vtkMRMLBoneStorageNode.h"

// Bender includes
#include <vtkBoneRepresentation.h>
#include <vtkCylinderBoneRepresentation.h>
#include <vtkDoubleConeBoneRepresentation.h>
#include <vtkMRMLNodeHelper.h>
#include <vtkQuaternion.h>

// MRML includes
#include <vtkMRMLAnnotationHierarchyNode.h>
#include <vtkMRMLScene.h>

// VTK includes
#include <vtkCallbackCommand.h>
#include <vtkFloatArray.h>
#include <vtkIntArray.h>
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkObserverManager.h>

#include <vtkProperty.h>

// STD includes
//#include <algorithm>
//#include <cassert>
//#include <sstream>

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLBoneNode);

//----------------------------------------------------------------------------
static void MRMLBoneNodeCallback(vtkObject* vtkNotUsed(caller),
                                 long unsigned int eventId,
                                 void* clientData,
                                 void* vtkNotUsed(callData))
{
  if (eventId == vtkCommand::ModifiedEvent)
    {
    vtkMRMLBoneNode* node = reinterpret_cast<vtkMRMLBoneNode*>(clientData);
    if (node)
      {
      node->Modified();
      }
    }
}

//----------------------------------------------------------------------------
vtkMRMLBoneNode::vtkMRMLBoneNode()
{
  this->CurrentHierarchyNode = 0;

  this->SceneObserverManager = vtkObserverManager::New();
  this->SceneObserverManager->AssignOwner(this);
  this->SceneObserverManager->GetCallbackCommand()->SetClientData(
    reinterpret_cast<void *>(this));
  this->SceneObserverManager->GetCallbackCommand()->SetCallback(
    vtkMRMLBoneNode::MRMLSceneCallback);

  this->Callback = vtkCallbackCommand::New();
  this->BoneProperties = vtkBoneWidget::New();
  this->BoneRepresentationType = 0;
  this->LinkedWithParent = true;
  this->HasParent = false;

  this->Callback->SetCallback(MRMLBoneNodeCallback);
  this->Callback->SetClientData(this);
  this->BoneProperties->AddObserver(vtkCommand::ModifiedEvent, this->Callback);

  this->BoneProperties->CreateDefaultRepresentation();
  this->BoneProperties->SetWidgetStateToRest();

  this->SetHideFromEditors(true);
}

//----------------------------------------------------------------------------
vtkMRMLBoneNode::~vtkMRMLBoneNode()
{
  this->SceneObserverManager->Delete();
  this->Callback->Delete();
  this->BoneProperties->Delete();
}

//----------------------------------------------------------------------------
void vtkMRMLBoneNode::WriteXML(ostream& of, int nIndent)
{
  this->Superclass::WriteXML(of, nIndent);

  vtkIndent indent(nIndent);
  of << indent << " Roll=\"" << this->BoneProperties->GetRoll() << "\"";
  of << indent << " RepresentationType=\""
    << this->BoneRepresentationType << "\"";
  of << indent << " ShowAxes=\""
    << this->BoneProperties->GetShowAxes() << "\"";
  of << indent << " ShowParenthood=\""
    << this->BoneProperties->GetShowParenthood() << "\"";

  of << indent << " WorldHeadRest=";
  vtkMRMLNodeHelper::PrintQuotedVector3(of,
    this->BoneProperties->GetWorldHeadRest());
  of << indent << " WorldTailRest=";
  vtkMRMLNodeHelper::PrintQuotedVector3(of,
    this->BoneProperties->GetWorldTailRest());
  of << indent << " WorldToParentRestRotation=";
  vtkMRMLNodeHelper::PrintQuotedVector(of,
    this->BoneProperties->GetWorldToParentRestRotation().GetData(), 4);
  of << indent << " WorldToParentRestTranslation=";
  vtkMRMLNodeHelper::PrintQuotedVector3(of,
    this->BoneProperties->GetWorldToParentRestTranslation());
  of << indent << " RestToPoseRotation=";
  vtkMRMLNodeHelper::PrintQuotedVector(of,
    this->BoneProperties->GetRestToPoseRotation().GetData(), 4);

  of << indent << " BoneLinkedWithParent=\""
    << this->GetBoneLinkedWithParent() << "\"";
}

//----------------------------------------------------------------------------
void vtkMRMLBoneNode::Copy(vtkMRMLNode* node)
{
  int wasModifying = this->StartModify();
  this->Superclass::Copy(node);

  vtkMRMLBoneNode* boneNode = vtkMRMLBoneNode::SafeDownCast(node);
  if (!boneNode)
    {
    return;
    }

  boneNode->PasteBoneNodeProperties(this->BoneProperties);

  this->EndModify(wasModifying);
}

//----------------------------------------------------------------------------
void vtkMRMLBoneNode::ReadXMLAttributes(const char** atts)
{
  int disabledModify = this->StartModify();

  this->Superclass::ReadXMLAttributes(atts);

  this->SetWidgetState(vtkBoneWidget::Rest);
  while (*atts != NULL)
    {
    const char* attName = *(atts++);
    std::string attValue(*(atts++));

    if (!strcmp(attName, "State"))
      {
      this->BoneProperties->SetWidgetState(
        vtkMRMLNodeHelper::StringToInt(attValue));
      }
    else if (!strcmp(attName, "Roll"))
      {
      this->BoneProperties->SetRoll(
        vtkMRMLNodeHelper::StringToDouble(attValue));
      }
    else if (!strcmp(attName, "RepresentationType"))
      {
      this->SetBoneRepresentationType(
        vtkMRMLNodeHelper::StringToInt(attValue));
      }
    else if (!strcmp(attName, "ShowAxes"))
      {
      this->BoneProperties->SetShowAxes(
        vtkMRMLNodeHelper::StringToInt(attValue));
      }
    else if (!strcmp(attName, "ShowParenthood"))
      {
      this->BoneProperties->SetShowParenthood(
        vtkMRMLNodeHelper::StringToInt(attValue));
      }
    else if (!strcmp(attName, "WorldHeadRest"))
      {
      double head[3];
      vtkMRMLNodeHelper::StringToVector3(attValue, head);
      this->BoneProperties->SetWorldHeadRest(head);
      }
    else if (!strcmp(attName, "WorldTailRest"))
      {
      double tail[3];
      vtkMRMLNodeHelper::StringToVector3(attValue, tail);
      this->BoneProperties->SetWorldTailRest(tail);
      }
    else if (!strcmp(attName, "WorldToParentRestRotation"))
      {
      double rotation[4];
      vtkMRMLNodeHelper::StringToVector(attValue, rotation, 4);
      this->BoneProperties->SetWorldToParentRestRotation(rotation);
      }
    else if (!strcmp(attName, "WorldToParentRestTranslation"))
      {
      double translation[3];
      vtkMRMLNodeHelper::StringToVector3(attValue, translation);
      this->BoneProperties->SetWorldToParentRestTranslation(translation);
      }
    else if (!strcmp(attName, "RestToPoseRotation"))
      {
      double rotation[4];
      vtkMRMLNodeHelper::StringToVector(attValue, rotation, 4);
      this->BoneProperties->SetRestToPoseRotation(rotation);
      }
    else if (!strcmp(attName, "BoneLinkedWithParent"))
      {
      this->SetBoneLinkedWithParent(
        vtkMRMLNodeHelper::StringToInt(attValue));
      }
    }

  this->EndModify(disabledModify);
}

//-----------------------------------------------------------
void vtkMRMLBoneNode::UpdateScene(vtkMRMLScene *scene)
{
  this->Superclass::UpdateScene(scene);
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::ProcessMRMLEvents(vtkObject* caller,
                                        unsigned long event,
                                        void* callData)
{
  this->Superclass::ProcessMRMLEvents(caller, event, callData);
}

//----------------------------------------------------------------------------
void vtkMRMLBoneNode
::ProcessMRMLSceneEvents(vtkObject *caller, unsigned long eid, void *callData)
{
  vtkMRMLNode* node = reinterpret_cast<vtkMRMLNode*>(callData);
  if (eid == vtkMRMLScene::NodeAddedEvent && node == this)
    {
    this->AddBoneHierarchyNode();
    }
}

//----------------------------------------------------------------------------
void vtkMRMLBoneNode::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}

//----------------------------------------------------------------------------
void vtkMRMLBoneNode
::Initialize(vtkMRMLScene* mrmlScene, vtkMRMLAnnotationHierarchyNode* parent)
{
  if (!mrmlScene)
    {
    vtkErrorMacro(<< __FUNCTION__ << ": No scene");
    return;
    }
  // \tbd remove this SetScene call as it shouldn't be mandatory.
  this->SetScene(mrmlScene);
  this->CreateBoneDisplayNode();

  // HACK:
  // The vtkSlicerAnnotationModuleLogic adds the hierarchy nodes with no
  // respect for the bone parent. To make this right, we add
  // an observer that needs to be called before the annotation logic to
  // make sure that the hierarchy node is added properly.
  // The CurrentlyAdded*Node are used to know what is the current armature
  // and the current bones.
  vtkNew<vtkIntArray> events;
  events->InsertNextValue(vtkMRMLScene::NodeAddedEvent);
  vtkNew<vtkFloatArray> priorities;
  // HACK: it should even be done before the scene models think the bones
  // have no parent.
  priorities->InsertNextValue(10000.0);
  this->SceneObserverManager->AddObjectEvents(
    this->GetScene(), events.GetPointer(), priorities.GetPointer());

  this->CurrentHierarchyNode = parent;
  this->Superclass::Initialize(mrmlScene);

  // Reset observer and CurrentHierarchyNode variables
  this->SceneObserverManager->RemoveObjectEvents(this->GetScene());
  this->CurrentHierarchyNode = 0;
}

//----------------------------------------------------------------------------
vtkMRMLBoneDisplayNode* vtkMRMLBoneNode::GetBoneDisplayNode()
{
  for (int i = 0; i < this->GetNumberOfDisplayNodes(); ++i)
    {
    vtkMRMLBoneDisplayNode* bdn = vtkMRMLBoneDisplayNode::SafeDownCast(
      this->GetNthDisplayNode(i));
    if (bdn)
      {
      return bdn;
      }
    }
  return 0;
}

//----------------------------------------------------------------------------
vtkMRMLAnnotationHierarchyNode* vtkMRMLBoneNode::GetHierarchyNode()
{
  return vtkMRMLAnnotationHierarchyNode::SafeDownCast(
    vtkMRMLHierarchyNode::GetAssociatedHierarchyNode(
      this->GetScene(), this->GetID()));
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::CreateBoneDisplayNode()
{
  if (this->GetBoneDisplayNode() != 0)
    {
    return;
    }
  if (!this->GetScene())
    {
    vtkErrorMacro( << __FUNCTION__ << ": No scene" ) ;
    return;
    }

  vtkNew<vtkMRMLBoneDisplayNode> boneDisplayNode;
  this->GetScene()->AddNode(boneDisplayNode.GetPointer());

  this->AddAndObserveDisplayNodeID(boneDisplayNode->GetID());
}

//---------------------------------------------------------------------------
double vtkMRMLBoneNode::GetLength()
{
  return this->BoneProperties->GetLength();
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::SetName(const char* name)
{
  this->BoneProperties->SetName(name);
  this->Superclass::SetName(name);
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::SetWidgetState(int state)
{
  this->BoneProperties->SetWidgetState(state);
}

//---------------------------------------------------------------------------
int vtkMRMLBoneNode::GetWidgetState()
{
  return this->BoneProperties->GetWidgetState();
}

namespace
{
int FindBoneRepresentationType(vtkBoneRepresentation* rep)
{
  if (vtkCylinderBoneRepresentation::SafeDownCast(rep))
    {
    return 1;
    }
  else if (vtkDoubleConeBoneRepresentation::SafeDownCast(rep))
    {
    return 2;
    }
  else
    {
    return 0;
    }
}
}// end namespace

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::SetBoneRepresentation(vtkBoneRepresentation* r)
{
  this->SetBoneRepresentationType( FindBoneRepresentationType(r) );
}

//---------------------------------------------------------------------------
vtkBoneRepresentation* vtkMRMLBoneNode::GetBoneRepresentation()
{
  return this->BoneProperties->GetBoneRepresentation();
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::SetBoneRepresentationType(int type)
{
  if (type == this->BoneRepresentationType)
    {
    return;
    }

  if (type == 1)
    {
    vtkNew<vtkCylinderBoneRepresentation> rep;
    this->BoneProperties->SetRepresentation(rep.GetPointer());
    this->BoneRepresentationType = 1;
    }
  else if (type == 2)
    {
    vtkNew<vtkDoubleConeBoneRepresentation> rep;
    this->BoneProperties->SetRepresentation(rep.GetPointer());
    this->BoneRepresentationType = 2;
    }
  else
    {
    vtkNew<vtkBoneRepresentation> rep;
    this->BoneProperties->SetRepresentation(rep.GetPointer());
    this->BoneRepresentationType = 0;
    }

  this->Modified();
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::SetRoll(double roll)
{
  this->BoneProperties->SetRoll(roll);
}

//---------------------------------------------------------------------------
double vtkMRMLBoneNode::GetRoll()
{
  return this->BoneProperties->GetRoll();
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::SetWorldHeadRest(const double* headPoint)
{
  double head[3];
  head[0] = headPoint[0];
  head[1] = headPoint[1];
  head[2] = headPoint[2];
  this->BoneProperties->SetWorldHeadRest(head);
}

//---------------------------------------------------------------------------
double* vtkMRMLBoneNode::GetWorldHeadRest()
{
  return this->BoneProperties->GetWorldHeadRest();
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::GetWorldHeadRest(double head[3])
{
  this->BoneProperties->GetWorldHeadRest(head);
}

//---------------------------------------------------------------------------
double* vtkMRMLBoneNode::GetWorldHeadPose()
{
  return this->BoneProperties->GetWorldHeadPose();
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::GetWorldHeadPose(double head[3])
{
  this->BoneProperties->GetWorldHeadPose(head);
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::SetWorldTailRest(const double* tailPoint)
{
  double tail[3];
  tail[0] = tailPoint[0];
  tail[1] = tailPoint[1];
  tail[2] = tailPoint[2];
  this->BoneProperties->SetWorldTailRest(tail);
}

//---------------------------------------------------------------------------
double* vtkMRMLBoneNode::GetWorldTailRest()
{
  return this->BoneProperties->GetWorldTailRest();
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::GetWorldTailRest(double tail[3])
{
  this->BoneProperties->GetWorldTailRest(tail);
}

//---------------------------------------------------------------------------
double* vtkMRMLBoneNode::GetWorldTailPose()
{
  return this->BoneProperties->GetWorldTailPose();
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::GetWorldTailPose(double tail[3])
{
  this->BoneProperties->GetWorldTailPose(tail);
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::SetLocalHeadRest(const double* headPoint)
{
  double head[3];
  head[0] = headPoint[0];
  head[1] = headPoint[1];
  head[2] = headPoint[2];
  this->BoneProperties->SetLocalHeadRest(head);
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::SetLocalTailRest(const double* tailPoint)
{
  double tail[3];
  tail[0] = tailPoint[0];
  tail[1] = tailPoint[1];
  tail[2] = tailPoint[2];
  this->BoneProperties->SetLocalTailRest(tail);
}

//---------------------------------------------------------------------------
double* vtkMRMLBoneNode::GetLocalHeadRest()
{
  return this->BoneProperties->GetLocalHeadRest();
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::GetLocalHeadRest(double head[3])
{
  this->BoneProperties->GetLocalHeadRest(head);
}

//---------------------------------------------------------------------------
double* vtkMRMLBoneNode::GetLocalHeadPose()
{
  return this->BoneProperties->GetLocalHeadPose();
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::GetLocalHeadPose(double head[3])
{
  this->BoneProperties->GetLocalHeadPose(head);
}

//---------------------------------------------------------------------------
double* vtkMRMLBoneNode::GetLocalTailRest()
{
  return this->BoneProperties->GetLocalTailRest();
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::GetLocalTailRest(double tail[3])
{
  this->BoneProperties->GetLocalTailRest(tail);
}

//---------------------------------------------------------------------------
double* vtkMRMLBoneNode::GetLocalTailPose()
{
  return this->BoneProperties->GetLocalTailPose();
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::GetLocalTailPose(double tail[3])
{
  this->BoneProperties->GetLocalTailPose(tail);
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::SetShowAxes(int axesVisibility)
{
  this->BoneProperties->SetShowAxes(axesVisibility);
}

//---------------------------------------------------------------------------
int vtkMRMLBoneNode::GetShowAxes()
{
  return this->BoneProperties->GetShowAxes();
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::SetRestToPoseRotation(double quad[4])
{
  this->BoneProperties->SetRestToPoseRotation(quad);
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::GetRestToPoseRotation(double quad[4])
{
  this->BoneProperties->GetRestToPoseRotation(quad);
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::SetWorldToParentRestRotation(const double* rotation)
{
  double rot[4];
  rot[0] = rotation[0];
  rot[1] = rotation[1];
  rot[2] = rotation[2];
  rot[3] = rotation[3];
  this->BoneProperties->SetWorldToParentRestRotation(rot);
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::SetWorldToParentPoseRotation(const double* rotation)
{
  double rot[4];
  rot[0] = rotation[0];
  rot[1] = rotation[1];
  rot[2] = rotation[2];
  rot[3] = rotation[3];
  this->BoneProperties->SetWorldToParentPoseRotation(rot);
}

//---------------------------------------------------------------------------
vtkQuaterniond vtkMRMLBoneNode::GetWorldToParentRestRotation()
{
  return this->BoneProperties->GetWorldToParentRestRotation();
}

//---------------------------------------------------------------------------
vtkQuaterniond vtkMRMLBoneNode::GetWorldToParentPoseRotation()
{
  return this->BoneProperties->GetWorldToParentPoseRotation();
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::SetWorldToParentRestTranslation(const double* translation)
{
  double trans[3];
  trans[0] = translation[0];
  trans[1] = translation[1];
  trans[2] = translation[2];
  this->BoneProperties->SetWorldToParentRestTranslation(trans);
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::SetWorldToParentPoseTranslation(const double* translation)
{
  double trans[3];
  trans[0] = translation[0];
  trans[1] = translation[1];
  trans[2] = translation[2];
  this->BoneProperties->SetWorldToParentPoseTranslation(trans);
}

//---------------------------------------------------------------------------
double* vtkMRMLBoneNode::GetWorldToParentRestTranslation()
{
  return this->BoneProperties->GetWorldToParentRestTranslation();
}

//---------------------------------------------------------------------------
double* vtkMRMLBoneNode::GetWorldToParentPoseTranslation()
{
  return this->BoneProperties->GetWorldToParentPoseTranslation();
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::GetWorldToParentRestTranslation(double pos[3])
{
  this->BoneProperties->GetWorldToParentRestTranslation(pos);
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::GetWorldToParentPoseTranslation(double pos[3])
{
  this->BoneProperties->GetWorldToParentPoseTranslation(pos);
}

//---------------------------------------------------------------------------
vtkQuaterniond vtkMRMLBoneNode::GetParentToBoneRestRotation()
{
  return this->BoneProperties->GetParentToBoneRestRotation();
}

//---------------------------------------------------------------------------
vtkQuaterniond vtkMRMLBoneNode::GetParentToBonePoseRotation()
{
  return this->BoneProperties->GetParentToBonePoseRotation();
}

//---------------------------------------------------------------------------
double* vtkMRMLBoneNode::GetParentToBoneRestTranslation()
{
  return this->BoneProperties->GetParentToBoneRestTranslation();
}

//---------------------------------------------------------------------------
double* vtkMRMLBoneNode::GetParentToBonePoseTranslation()
{
  return this->BoneProperties->GetParentToBonePoseTranslation();
}

//---------------------------------------------------------------------------
vtkQuaterniond vtkMRMLBoneNode::GetWorldToBoneRestRotation()
{
  return this->BoneProperties->GetWorldToBoneRestRotation();
}

//---------------------------------------------------------------------------
vtkQuaterniond vtkMRMLBoneNode::GetWorldToBonePoseRotation()
{
  return this->BoneProperties->GetWorldToBonePoseRotation();
}

//---------------------------------------------------------------------------
double* vtkMRMLBoneNode::GetWorldToBoneHeadRestTranslation()
{
  return this->BoneProperties->GetWorldToBoneHeadRestTranslation();
}

//---------------------------------------------------------------------------
double* vtkMRMLBoneNode::GetWorldToBoneHeadPoseTranslation()
{
  return this->BoneProperties->GetWorldToBoneHeadPoseTranslation();
}

//---------------------------------------------------------------------------
double* vtkMRMLBoneNode::GetWorldToBoneTailRestTranslation()
{
  return this->BoneProperties->GetWorldToBoneTailRestTranslation();
}

//---------------------------------------------------------------------------
double* vtkMRMLBoneNode::GetWorldToBoneTailPoseTranslation()
{
  return this->BoneProperties->GetWorldToBoneTailPoseTranslation();
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::SetShowParenthood(int parenthood)
{
  this->BoneProperties->SetShowParenthood(parenthood);
}

//---------------------------------------------------------------------------
int vtkMRMLBoneNode::GetShowParenthood()
{
  return this->BoneProperties->GetShowParenthood();
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::SetBoneLinkedWithParent(bool linked)
{
  if (linked == this->LinkedWithParent)
    {
    return;
    }

  this->LinkedWithParent = linked;
  this->Modified();
}

//---------------------------------------------------------------------------
bool vtkMRMLBoneNode::GetBoneLinkedWithParent()
{
  return this->LinkedWithParent;
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::RotateTailWithParentX(double angle)
{
  this->RotateTailWithParentWXYZ(angle, 1.0, 0.0, 0.0);
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::RotateTailWithParentY(double angle)
{
  this->RotateTailWithParentWXYZ(angle, 0.0, 1.0, 0.0);
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::RotateTailWithParentZ(double angle)
{
  this->RotateTailWithParentWXYZ(angle, 0.0, 0.0, 1.0);
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode
::RotateTailWithParentWXYZ(double angle, double x, double y, double z)
{
  double axis[3];
  axis[0] = x;
  axis[1] = y;
  axis[2] = z;
  this->RotateTailWithParentWXYZ(angle, axis);
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::RotateTailWithParentWXYZ(double angle, double axis[3])
{
  this->BoneProperties->RotateTailWithParentWXYZ(angle, axis);
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::RotateTailWithWorldX(double angle)
{
  this->RotateTailWithWorldWXYZ(angle, 1.0, 0.0, 0.0);
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::RotateTailWithWorldY(double angle)
{
  this->RotateTailWithWorldWXYZ(angle, 0.0, 1.0, 0.0);
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::RotateTailWithWorldZ(double angle)
{
  this->RotateTailWithWorldWXYZ(angle, 0.0, 0.0, 1.0);
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode
::RotateTailWithWorldWXYZ(double angle, double x, double y, double z)
{
  double axis[3];
  axis[0] = x;
  axis[1] = y;
  axis[2] = z;
  this->RotateTailWithWorldWXYZ(angle, axis);
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::RotateTailWithWorldWXYZ(double angle, double axis[3])
{
  this->BoneProperties->RotateTailWithWorldWXYZ(angle, axis);
}

//----------------------------------------------------------------------------
void vtkMRMLBoneNode::Scale(double factor)
{
  this->Scale(factor, factor, factor);
}

//----------------------------------------------------------------------------
void vtkMRMLBoneNode::Scale(double factorX, double factorY, double factorZ)
{
  double factors[3];
  factors[0] = factorX;
  factors[1] = factorX;
  factors[2] = factorX;
  this->Scale(factors);
}

//----------------------------------------------------------------------------
void vtkMRMLBoneNode::Scale(double factors[3])
{
  this->BoneProperties->Scale(factors);
}

//----------------------------------------------------------------------------
void vtkMRMLBoneNode::Translate(double x, double y, double z)
{
  double translation[3];
  translation[0] = x;
  translation[1] = y;
  translation[2] = z;
  this->Translate(translation);
}

//----------------------------------------------------------------------------
void vtkMRMLBoneNode::Translate(double rootHead[3])
{
  this->BoneProperties->Translate(rootHead);
}

//----------------------------------------------------------------------------
void vtkMRMLBoneNode::RotateX(double angle)
{
  this->RotateWXYZ(angle, 1.0, 0.0, 0.0);
}

//----------------------------------------------------------------------------
void vtkMRMLBoneNode::RotateY(double angle)
{
  this->RotateWXYZ(angle, 0.0, 1.0, 0.0);
}

//----------------------------------------------------------------------------
void vtkMRMLBoneNode::RotateZ(double angle)
{
  this->RotateWXYZ(angle, 0.0, 0.0, 1.0);
}

//----------------------------------------------------------------------------
void vtkMRMLBoneNode::RotateWXYZ(double angle, double x, double y, double z)
{
  double axis[3];
  axis[0] = x;
  axis[1] = y;
  axis[2] = z;
  this->RotateWXYZ(angle, axis);
}

//----------------------------------------------------------------------------
void vtkMRMLBoneNode::RotateWXYZ(double angle, double axis[3])
{
  this->BoneProperties->RotateWXYZ(angle, axis);
}

//----------------------------------------------------------------------------
void vtkMRMLBoneNode::Transform(vtkTransform* transform)
{
  this->BoneProperties->Transform(transform);
}

//----------------------------------------------------------------------------
void vtkMRMLBoneNode::SetLength(double size)
{
  this->BoneProperties->SetLength(size);
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::CopyBoneWidgetProperties(vtkBoneWidget* boneWidget)
{
  if (!boneWidget)
    {
    return;
    }

  // -- Representation --
  this->SetBoneRepresentation(boneWidget->GetBoneRepresentation());
  this->BoneProperties->GetBoneRepresentation()->GetLineProperty()->SetColor(
    boneWidget->GetBoneRepresentation()->GetLineProperty()->GetColor());
  this->BoneProperties->GetBoneRepresentation()->GetLineProperty()->SetOpacity(
    boneWidget->GetBoneRepresentation()->GetLineProperty()->GetOpacity());

  // -- Name --
  this->SetName(boneWidget->GetName());

  // -- Selected --
  this->SetSelected(boneWidget->GetBoneSelected());

  // -- All the other properties --
  this->BoneProperties->DeepCopy(boneWidget);
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::PasteBoneNodeProperties(vtkBoneWidget* boneWidget)
{
  if (!boneWidget)
    {
    return;
    }

  // -- Representation, part 1 --
  if (FindBoneRepresentationType(boneWidget->GetBoneRepresentation()) != 
      this->BoneRepresentationType)
    {
    if (this->BoneRepresentationType == 1)
      {
      vtkNew<vtkCylinderBoneRepresentation> rep;
      boneWidget->SetRepresentation(rep.GetPointer());
      }
    else if (this->BoneRepresentationType == 2)
      {
      vtkNew<vtkDoubleConeBoneRepresentation> rep;
      boneWidget->SetRepresentation(rep.GetPointer());
      }
    else
      {
      vtkNew<vtkBoneRepresentation> rep;
      boneWidget->SetRepresentation(rep.GetPointer());
      }
    }

  // -- All the other properties --
  boneWidget->DeepCopy(this->BoneProperties);
}

//----------------------------------------------------------------------------
void vtkMRMLBoneNode
::MRMLSceneCallback(vtkObject *caller, unsigned long eid,
                    void *clientData, void *callData)
{
  vtkMRMLBoneNode *self = reinterpret_cast<vtkMRMLBoneNode *>(clientData);
  assert(vtkMRMLScene::SafeDownCast(caller));
  assert(caller == self->GetScene());

  self->ProcessMRMLSceneEvents(caller, eid, callData);
}

//----------------------------------------------------------------------------
void vtkMRMLBoneNode::AddBoneHierarchyNode()
{
  if (!this->CurrentHierarchyNode || !this->GetID())
    {
    return;
    }

  vtkNew<vtkMRMLAnnotationHierarchyNode> hierarchyNode;
  hierarchyNode->AllowMultipleChildrenOff();
  hierarchyNode->SetHideFromEditors(1);
  hierarchyNode->SetName(
    this->GetScene()->GetUniqueNameByString("BoneAnnotationHierarchy"));

  hierarchyNode->SetParentNodeID(this->CurrentHierarchyNode->GetID());

  this->GetScene()->AddNode(hierarchyNode.GetPointer());

  hierarchyNode->SetDisplayableNodeID(this->GetID());
}
