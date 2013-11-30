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

// Models includes
#include "vtkSlicerAnnotationModuleLogic.h"
#include "vtkSlicerModelsLogic.h"

// Armatures includes
#include "vtkMRMLArmatureNode.h"
#include "vtkMRMLBoneDisplayNode.h"
#include "vtkMRMLBoneNode.h"
#include "vtkMRMLSelectionNode.h"
#include "vtkSlicerArmaturesLogic.h"

// MRML includes
#include <vtkEventBroker.h>
#include <vtkMRMLInteractionNode.h>
#include <vtkMRMLSelectionNode.h>

// VTK includes
#include <vtkCellData.h>
#include <vtkDoubleArray.h>
#include <vtkIdTypeArray.h>
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataReader.h>
#include <vtkStringArray.h>
#include <vtksys/SystemTools.hxx>
#include <vtkXMLDataElement.h>
#include <vtkXMLDataParser.h>

// STD includes
#include <cassert>

vtkCxxSetObjectMacro(vtkSlicerArmaturesLogic, ModelsLogic, vtkSlicerModelsLogic);

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerArmaturesLogic);

//----------------------------------------------------------------------------
vtkSlicerArmaturesLogic::vtkSlicerArmaturesLogic()
{
  this->ModelsLogic = 0;
  this->AnnotationsLogic = 0;
}

//----------------------------------------------------------------------------
vtkSlicerArmaturesLogic::~vtkSlicerArmaturesLogic()
{
  if (this->ModelsLogic)
    {
    this->ModelsLogic->Delete();
    }
  if (this->AnnotationsLogic)
    {
    this->AnnotationsLogic->Delete();
    }
}

//----------------------------------------------------------------------------
void vtkSlicerArmaturesLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
void vtkSlicerArmaturesLogic::SetMRMLSceneInternal(vtkMRMLScene * newScene)
{
  vtkIntArray *events = vtkIntArray::New();
  events->InsertNextValue(vtkMRMLScene::NodeAddedEvent);
  events->InsertNextValue(vtkMRMLScene::NodeAboutToBeRemovedEvent);
  this->SetAndObserveMRMLSceneEventsInternal(newScene, events);
  events->Delete();
}

//----------------------------------------------------------------------------
void vtkSlicerArmaturesLogic::ObserveMRMLScene()
{
  vtkMRMLSelectionNode* selectionNode = vtkMRMLSelectionNode::SafeDownCast(
    this->GetMRMLScene()->GetNthNodeByClass(0, "vtkMRMLSelectionNode"));
  if (selectionNode)
    {
    selectionNode->AddNewPlaceNodeClassNameToList(
      "vtkMRMLBoneNode", ":/Icons/BoneWithArrow.png");
    selectionNode->SetReferenceActivePlaceNodeClassName("vtkMRMLBoneNode");
    }
  vtkMRMLInteractionNode* interactionNode =
    vtkMRMLInteractionNode::SafeDownCast(
      this->GetMRMLScene()->GetNodeByID("vtkMRMLInteractionNodeSingleton"));
  if (interactionNode)
    {
    interactionNode->SetPlaceModePersistence(1);
    interactionNode->SetCurrentInteractionMode(
      vtkMRMLInteractionNode::ViewTransform);
    }

  this->Superclass::ObserveMRMLScene();
}

//----------------------------------------------------------------------------
void vtkSlicerArmaturesLogic::RegisterNodes()
{
  assert(this->GetMRMLScene());

  vtkNew<vtkMRMLArmatureNode> armatureNode;
  this->GetMRMLScene()->RegisterNodeClass( armatureNode.GetPointer() );

  vtkNew<vtkMRMLBoneNode> boneNode;
  this->GetMRMLScene()->RegisterNodeClass( boneNode.GetPointer() );

  vtkNew<vtkMRMLBoneDisplayNode> boneDisplayNode;
  this->GetMRMLScene()->RegisterNodeClass( boneDisplayNode.GetPointer() );
}

//----------------------------------------------------------------------------
void vtkSlicerArmaturesLogic::ProcessMRMLSceneEvents(vtkObject* caller,
                                    unsigned long event,
                                    void * callData)
{
  this->Superclass::ProcessMRMLSceneEvents(caller, event, callData);
  if (event == vtkMRMLScene::NodeAboutToBeRemovedEvent)
    {
    vtkMRMLNode* node = reinterpret_cast<vtkMRMLNode*>(callData);
    this->OnMRMLSceneNodeAboutToBeRemoved(node);
    }
}

//-----------------------------------------------------------------------------
void vtkSlicerArmaturesLogic::OnMRMLSceneNodeAdded(vtkMRMLNode* node)
{
  this->Superclass::OnMRMLSceneNodeAdded(node);
  vtkMRMLArmatureNode* armatureNode = vtkMRMLArmatureNode::SafeDownCast(node);
  if (armatureNode)
    {
    vtkNew<vtkMRMLModelNode> armatureModel;
    this->GetMRMLScene()->AddNode(armatureModel.GetPointer());
    armatureNode->SetArmatureModel(armatureModel.GetPointer());

    this->SetActiveArmature(armatureNode);
    }
  vtkMRMLBoneNode* boneNode = vtkMRMLBoneNode::SafeDownCast(node);
  if (boneNode)
    {
    this->SetActiveBone(boneNode);
    }
}

//-----------------------------------------------------------------------------
void vtkSlicerArmaturesLogic::OnMRMLSceneNodeAboutToBeRemoved(vtkMRMLNode* node)
{
  this->Superclass::OnMRMLSceneNodeRemoved(node);
  vtkMRMLArmatureNode* armatureNode = vtkMRMLArmatureNode::SafeDownCast(node);
  if (armatureNode)
    {
    vtkMRMLModelNode* model = armatureNode->GetArmatureModel();
    if (model)
      {
      this->GetMRMLScene()->RemoveNode(model);
      }
    if (this->GetActiveArmature() == armatureNode)
      {
      this->SetActiveArmature(0);
      }
    }
  vtkMRMLBoneNode* boneNode = vtkMRMLBoneNode::SafeDownCast(node);
  if (boneNode && this->GetActiveBone())
    {
    vtkMRMLBoneNode* parentBone = this->GetBoneParent(boneNode);
    if (parentBone)
      {
      this->SetActiveBone(parentBone);
      }
    else
      {
      this->SetActiveArmature(this->GetBoneArmature(boneNode));
      }
    }
}

//----------------------------------------------------------------------------
void vtkSlicerArmaturesLogic
::SetAnnotationsLogic(vtkSlicerAnnotationModuleLogic* annotationLogic)
{
  if (this->AnnotationsLogic == annotationLogic)
    {
    return;
    }

  if (this->AnnotationsLogic)
    {
    this->AnnotationsLogic->Delete();
    vtkEventBroker::GetInstance()->RemoveObservations(
      this->AnnotationsLogic, vtkCommand::ModifiedEvent,
      this, this->GetMRMLLogicsCallbackCommand());
    }
  this->AnnotationsLogic = annotationLogic;
  if (this->AnnotationsLogic)
    {
    this->AnnotationsLogic->Register(this);
    vtkEventBroker::GetInstance()->AddObservation(
      this->AnnotationsLogic, vtkCommand::ModifiedEvent,
      this, this->GetMRMLLogicsCallbackCommand());
    }
}

//----------------------------------------------------------------------------
void vtkSlicerArmaturesLogic
::ProcessMRMLLogicsEvents(vtkObject* caller, unsigned long event,
                          void* callData)
{
  this->Superclass::ProcessMRMLLogicsEvents(caller, event, callData);
  if (vtkSlicerAnnotationModuleLogic::SafeDownCast(caller))
    {
    assert(event == vtkCommand::ModifiedEvent);
    this->Modified();
    }
}

//----------------------------------------------------------------------------
void vtkSlicerArmaturesLogic::SetActiveArmature(vtkMRMLArmatureNode* armature)
{
  assert(this->AnnotationsLogic != 0);
  vtkMRMLArmatureNode* currentArmature = this->GetActiveArmature();
  if (currentArmature == armature)
    {
    return;
    }

  if (currentArmature)
    {
    currentArmature->SetSelected(0);
    }
  if (armature)
    {
    armature->SetSelected(1);
    }

  this->GetAnnotationsLogic()->SetActiveHierarchyNodeID(
    armature ? armature->GetID() : 0);
}

//----------------------------------------------------------------------------
vtkMRMLArmatureNode* vtkSlicerArmaturesLogic::GetActiveArmature()
{
  assert(this->AnnotationsLogic != 0);
  if (this->GetActiveBone())
    {
    return this->GetBoneArmature(this->GetActiveBone());
    }
  return vtkMRMLArmatureNode::SafeDownCast(
    this->GetAnnotationsLogic()->GetActiveHierarchyNode());
}

//----------------------------------------------------------------------------
void vtkSlicerArmaturesLogic::SetActiveArmatureWidgetState(int mode)
{
  assert(this->AnnotationsLogic != 0);
  vtkMRMLArmatureNode* currentArmature = this->GetActiveArmature();

  if (!currentArmature || currentArmature->GetWidgetState() == mode)
    {
    return;
    }

  currentArmature->SetWidgetState(mode);
}

//----------------------------------------------------------------------------
int vtkSlicerArmaturesLogic::GetActiveArmatureWidgetState()
{
  vtkMRMLArmatureNode* currentArmature = this->GetActiveArmature();

  if (!currentArmature)
    {
    return -1;
    }

  return currentArmature->GetWidgetState();
}

//----------------------------------------------------------------------------
void vtkSlicerArmaturesLogic::SetActiveBone(vtkMRMLBoneNode* bone)
{
  assert(this->AnnotationsLogic != 0);
  vtkMRMLAnnotationHierarchyNode* hierarchyNode = 0;
  if (bone != 0)
    {
    hierarchyNode = vtkMRMLAnnotationHierarchyNode::SafeDownCast(
      vtkMRMLHierarchyNode::GetAssociatedHierarchyNode(
        bone->GetScene(), bone->GetID()));
    }
  if (hierarchyNode == 0)
    {
    hierarchyNode = this->GetActiveArmature();
    }
  if (hierarchyNode == 0)
    {
    hierarchyNode = this->GetAnnotationsLogic()->GetActiveHierarchyNode();
    }
  this->GetAnnotationsLogic()->SetActiveHierarchyNodeID(
    hierarchyNode ? hierarchyNode->GetID() : 0);
}

//----------------------------------------------------------------------------
vtkMRMLBoneNode* vtkSlicerArmaturesLogic::GetActiveBone()
{
  assert(this->AnnotationsLogic != 0);
  vtkMRMLAnnotationHierarchyNode* hierarchyNode =
    this->GetAnnotationsLogic()->GetActiveHierarchyNode();
  return vtkMRMLBoneNode::SafeDownCast(
    hierarchyNode? hierarchyNode->GetDisplayableNode() : 0);
}

//----------------------------------------------------------------------------
void vtkSlicerArmaturesLogic
::SetArmatureVisibility(vtkMRMLArmatureNode* armatureNode, bool visible)
{
  if (!armatureNode)
    {
    return;
    }
  armatureNode->SetVisibility(visible);

  vtkNew<vtkCollection> bones;
  armatureNode->GetAllBones(bones.GetPointer());
  for (int i = 0; i < bones->GetNumberOfItems(); ++i)
    {
    vtkMRMLBoneNode* boneNode
      = vtkMRMLBoneNode::SafeDownCast(bones->GetItemAsObject(i));

    if (boneNode)
      {
      boneNode->SetDisplayVisibility(visible);
      }
    }
}

//----------------------------------------------------------------------------
bool vtkSlicerArmaturesLogic
::GetArmatureVisibility(vtkMRMLArmatureNode* armature)
{
  if (armature)
    {
    return armature->GetVisibility();
    }
  return false;
}

//----------------------------------------------------------------------------
vtkMRMLArmatureNode* vtkSlicerArmaturesLogic
::GetBoneArmature(vtkMRMLBoneNode* bone)
{
  if (bone == 0)
    {
    return 0;
    }
  vtkMRMLAnnotationHierarchyNode* hierarchyNode =
    vtkMRMLAnnotationHierarchyNode::SafeDownCast(
      vtkMRMLHierarchyNode::GetAssociatedHierarchyNode(
        bone->GetScene(), bone->GetID()));

  vtkMRMLArmatureNode* armatureNode = 0;
  if (hierarchyNode)
    {
    vtkMRMLAnnotationHierarchyNode* parentHierarchyNode =
      vtkMRMLAnnotationHierarchyNode::SafeDownCast(
        hierarchyNode->GetParentNode());

    armatureNode = vtkMRMLArmatureNode::SafeDownCast(parentHierarchyNode);
    if (armatureNode == 0)
      {
      armatureNode = this->GetBoneArmature(
        vtkMRMLBoneNode::SafeDownCast(
          parentHierarchyNode->GetDisplayableNode()));
      }
    }
  return armatureNode;
}

//----------------------------------------------------------------------------
vtkMRMLBoneNode* vtkSlicerArmaturesLogic::GetBoneParent(vtkMRMLBoneNode* bone)
{
  if (bone == 0)
    {
    return 0;
    }

  vtkMRMLAnnotationHierarchyNode* hierarchyNode =
    vtkMRMLAnnotationHierarchyNode::SafeDownCast(
      vtkMRMLHierarchyNode::GetAssociatedHierarchyNode(
        bone->GetScene(), bone->GetID()));

  vtkMRMLBoneNode* boneNode = 0;
  if (hierarchyNode)
    {
    vtkMRMLAnnotationHierarchyNode* parentHierarchyNode =
      vtkMRMLAnnotationHierarchyNode::SafeDownCast(
        hierarchyNode->GetParentNode());

    boneNode = vtkMRMLBoneNode::SafeDownCast(
      parentHierarchyNode? parentHierarchyNode->GetDisplayableNode() : 0);
    }

  return boneNode;
}

//----------------------------------------------------------------------------
vtkMRMLModelNode* vtkSlicerArmaturesLogic
::GetArmatureModel(vtkMRMLArmatureNode* armature)
{
  if (!armature)
    {
    return 0;
    }
  return armature->GetArmatureModel();
}

//----------------------------------------------------------------------------
vtkMRMLModelNode* vtkSlicerArmaturesLogic::AddArmatureFile(const char* fileName)
{
  vtkNew<vtkXMLDataParser> armatureParser;
  armatureParser->SetFileName(fileName);
  int res = armatureParser->Parse();
  if (res == 0)
    {
    vtkErrorMacro(<<"Fail to read" << fileName << ". Not a valid armature file");
    return 0;
    }
  vtkNew<vtkPolyData> armature;
  vtkNew<vtkPoints> points;
  points->SetDataTypeToDouble();
  armature->SetPoints(points.GetPointer());
  armature->Allocate(100);
  vtkXMLDataElement* armatureElement = armatureParser->GetRootElement();
  double orientationXYZW[4];
  armatureElement->GetVectorAttribute("orientation", 4, orientationXYZW);
  double orientationWXYZ[4] = {orientationXYZW[3], orientationXYZW[0],
                               orientationXYZW[1], orientationXYZW[2]};

  double origin[3] = {0., 0., 0.};
  for (int child = 0; child < armatureElement->GetNumberOfNestedElements(); ++child)
    {
    this->ReadBone(armatureElement->GetNestedElement(child), armature.GetPointer(), origin, orientationWXYZ);
    }
  return this->ModelsLogic->AddModel(armature.GetPointer());
}

//----------------------------------------------------------------------------
vtkMRMLArmatureNode* vtkSlicerArmaturesLogic
::ReadArmatureFromModel(const char* fileName)
{
  vtkNew<vtkPolyDataReader> reader;
  reader->SetFileName(fileName);
  reader->Update();
  vtkStdString baseName =
    vtksys::SystemTools::GetFilenameWithoutExtension(fileName);
  return this->CreateArmatureFromModel(reader->GetOutput(), baseName.c_str());
}

//----------------------------------------------------------------------------
vtkMRMLArmatureNode* vtkSlicerArmaturesLogic
::CreateArmatureFromModel(vtkPolyData* model, const char* baseName)
{
  if (!model)
    {
    std::cerr<<"Cannot create armature from model, model is null"<<std::endl;
    return NULL;
    }

  // First, create an armature node
  vtkMRMLArmatureNode* armatureNode = vtkMRMLArmatureNode::New();
  armatureNode->SetName(baseName);
  this->GetMRMLScene()->AddNode(armatureNode);
  armatureNode->Delete();

  vtkPoints* points = model->GetPoints();
  if (!points)
    {
    std::cerr<<"Cannot create armature from model,"
      <<" No points !"<<std::endl;
    return NULL;
    }

  vtkCellData* cellData = model->GetCellData();
  if (!cellData)
    {
    std::cerr<<"Cannot create armature from model, No cell data"<<std::endl;
    return NULL;
    }

  vtkIdTypeArray* parenthood =
    vtkIdTypeArray::SafeDownCast(cellData->GetArray("Parenthood"));
  if (!parenthood
    || parenthood->GetNumberOfTuples()*2 != points->GetNumberOfPoints())
    {
    std::cerr<<"Cannot create armature from model,"
      <<" parenthood array invalid"<<std::endl;
    return NULL;
    }

  vtkStringArray* names =
    vtkStringArray::SafeDownCast(cellData->GetAbstractArray("Names"));
  if (!names
    || names->GetNumberOfTuples()*2 != points->GetNumberOfPoints())
    {
    std::cout<<"Warning: No names found in the armature file. \n"
      << "-> Using default naming !" <<std::endl;
    }

  vtkDoubleArray* restToPose=
    vtkDoubleArray::SafeDownCast(cellData->GetArray("RestToPoseRotation"));
  // 1 quaternion per bone
  if (!restToPose
    || restToPose->GetNumberOfTuples()*2 != points->GetNumberOfPoints())
    {
    std::cout<<"Warning: No Pose found in the armature file. \n"
      << "-> No pose imported !" <<std::endl;
    }

  vtkNew<vtkCollection> addedBones;
  for (vtkIdType id = 0, pointId = 0;
    id < parenthood->GetNumberOfTuples(); ++id, pointId += 2)
    {
    vtkIdType parentId = parenthood->GetValue(id);
    if (parentId > id)
      {
      std::cerr<<"There most likely were reparenting."
        << " Not supported yet."<<std::endl;
      return armatureNode;
      }

    vtkMRMLBoneNode* boneParentNode = 0;
    if (parentId > -1)
      {
      boneParentNode =
        vtkMRMLBoneNode::SafeDownCast(addedBones->GetItemAsObject(parentId));
      if (! boneParentNode)
        {
        std::cerr<<"Could not find bone parent ! Stopping"<<std::endl;
        return armatureNode;
        }

      vtkMRMLAnnotationHierarchyNode* hierarchyNode =
        vtkMRMLAnnotationHierarchyNode::SafeDownCast(
          vtkMRMLHierarchyNode::GetAssociatedHierarchyNode(
            boneParentNode->GetScene(), boneParentNode->GetID()));
      this->GetAnnotationsLogic()->SetActiveHierarchyNodeID(
        hierarchyNode != 0 ? hierarchyNode->GetID() : 0);
      }
    else // Root
      {
      this->GetAnnotationsLogic()->SetActiveHierarchyNodeID(
        armatureNode->GetID());
      }

    vtkMRMLBoneNode* boneNode = vtkMRMLBoneNode::New();

    if (names)
      {
      boneNode->SetName(names->GetValue(id));
      }

    double p[3];
    points->GetPoint(pointId, p);
    boneNode->SetWorldHeadRest(p);

    points->GetPoint(pointId + 1, p);
    boneNode->SetWorldTailRest(p);

    if (restToPose)
      {
      double quad[4];
      restToPose->GetTupleValue(id, quad);
      boneNode->SetRestToPoseRotation(quad);
      }

    if (boneParentNode)
      {
      double diff[3];
      vtkMath::Subtract(
        boneParentNode->GetWorldTailRest(),
        boneNode->GetWorldHeadRest(),
        diff);
      if (vtkMath::Dot(diff, diff) > 1e-6)
        {
        boneNode->SetBoneLinkedWithParent(false);
        }
      }

    boneNode->Initialize(this->GetMRMLScene());
    addedBones->AddItem(boneNode);
    boneNode->Delete();
    }

  return armatureNode;
}

//----------------------------------------------------------------------------
void vtkSlicerArmaturesLogic::ReadBone(vtkXMLDataElement* boneElement,
                                       vtkPolyData* polyData,
                                       const double origin[3],
                                       const double parentOrientation[4])
{
  double parentMatrix[3][3];
  vtkMath::QuaternionToMatrix3x3(parentOrientation, parentMatrix);

  double localHead[3] = {0., 0., 0.};
  boneElement->GetVectorAttribute("head", 3, localHead);
  double head[3] = {0., 0., 0.};
  vtkMath::Multiply3x3(parentMatrix, localHead, head);

  double parentPosedOrientation[4] =
    {parentOrientation[0], parentOrientation[1],
     parentOrientation[2], parentOrientation[3]};
  bool applyPose = true;
  if (applyPose)
    {
    vtkXMLDataElement * poseElement =
      boneElement->FindNestedElementWithName("pose");
    if (poseElement)
      {
      this->ReadPose(poseElement, parentPosedOrientation, true);
      }
    }

#ifndef BEFORE
  double parentPosedMatrix[3][3];
  vtkMath::QuaternionToMatrix3x3(parentPosedOrientation, parentPosedMatrix);

  double localTail[3] = {0., 0. ,0};
  boneElement->GetVectorAttribute("tail", 3, localTail);
  double tail[3] = {0., 0. ,0};
  vtkMath::Subtract(localTail, localHead, tail);
  vtkMath::Multiply3x3(parentPosedMatrix, tail, tail);
  vtkMath::Add(localHead, tail, tail);

  vtkMath::Add(origin, head, head);
  vtkMath::Add(origin, tail, tail);

  vtkIdType indexes[2];
  indexes[0] = polyData->GetPoints()->InsertNextPoint(head);
  indexes[1] = polyData->GetPoints()->InsertNextPoint(tail);

  polyData->InsertNextCell(VTK_LINE, 2, indexes);
#endif

  double localOrientationXYZW[4] = {0., 0., 0., 1.};
  boneElement->GetVectorAttribute("orientation", 4, localOrientationXYZW);
  double localOrientationWXYZ[4] = {localOrientationXYZW[3], localOrientationXYZW[0],
                                    localOrientationXYZW[1], localOrientationXYZW[2]};
  double worldOrientation[4];
  //vtkMath::MultiplyQuaternion(parentOrientation, localOrientationWXYZ, worldOrientation);
  vtkMath::MultiplyQuaternion(parentPosedOrientation, localOrientationWXYZ, worldOrientation);
  //vtkMath::MultiplyQuaternion(localOrientationWXYZ, parentOrientation, worldOrientation);
  //vtkMath::MultiplyQuaternion(localOrientationWXYZ, parentPosedOrientation, worldOrientation);
/*
  vtkXMLDataElement * poseElement =
    boneElement->FindNestedElementWithName("pose");
  if (applyPose)
    {
    if (poseElement)
      {
      this->ReadPose(poseElement, worldOrientation, false);
      }
    }
*/
#ifdef BEFORE
  double parentPosedMatrix[3][3];
  vtkMath::QuaternionToMatrix3x3(worldOrientation, parentPosedMatrix);

  double localTail[3] = {0., 0. ,0};
  boneElement->GetVectorAttribute("tail", 3, localTail);
  double tail[3] = {0., 0. ,0};
  vtkMath::Subtract(localTail, localHead, tail);
  vtkMath::Multiply3x3(parentPosedMatrix, tail, tail);
  vtkMath::Add(localHead, tail, tail);

  vtkMath::Add(origin, head, head);
  vtkMath::Add(origin, tail, tail);

  vtkIdType indexes[2];
  indexes[0] = polyData->GetPoints()->InsertNextPoint(head);
  indexes[1] = polyData->GetPoints()->InsertNextPoint(tail);

  polyData->InsertNextCell(VTK_LINE, 2, indexes);
#endif

  //std::cout << "local quat: w=" << localOrientationWXYZ[0] << " x=" << localOrientationWXYZ[1] << " y=" << localOrientationWXYZ[2] << " z=" << localOrientationWXYZ[3] << std::endl;
  //std::cout << "New quat: w=" << worldOrientation[0] << " x=" << worldOrientation[1] << " y=" << worldOrientation[2] << " z=" << worldOrientation[3] << std::endl;
  for (int child = 0; child < boneElement->GetNumberOfNestedElements(); ++child)
    {
    vtkXMLDataElement* childElement = boneElement->GetNestedElement(child);
    if (strcmp(childElement->GetName(),"bone") == 0)
      {
      this->ReadBone(childElement, polyData, tail, worldOrientation);
      }
    else if (strcmp(childElement->GetName(),"pose") == 0)
      {
      // already parsed
      }
    else
      {
      vtkWarningMacro( << "XML element " << childElement->GetName()
                       << "is not supported");
      }
    }
}

//----------------------------------------------------------------------------
void vtkSlicerArmaturesLogic::ReadPose(vtkXMLDataElement* poseElement,
                                       double parentOrientation[4], bool invert)
{
  double poseRotationXYZW[4] = {0., 0., 0., 0.};
  poseElement->GetVectorAttribute("rotation", 4, poseRotationXYZW);
  double poseRotationWXYZ[4] = {poseRotationXYZW[3], poseRotationXYZW[0],
                                poseRotationXYZW[1], poseRotationXYZW[2]};
  if (invert)
    {
    vtkMath::MultiplyQuaternion(poseRotationWXYZ, parentOrientation, parentOrientation);
    }
  else
    {
    vtkMath::MultiplyQuaternion(parentOrientation, poseRotationWXYZ, parentOrientation);
    }
}
