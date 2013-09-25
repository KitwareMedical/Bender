/*=auto=========================================================================

  Portions (c) Copyright 2005 Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Program:   3D Slicer
  Module:    $RCSfile: vtkMRMLModelStorageNode.cxx,v $
  Date:      $Date: 2006/03/17 15:10:09 $
  Version:   $Revision: 1.2 $

=========================================================================auto=*/

#include "vtkMRMLArmatureStorageNode.h"

// Bender includes
#include "vtkMRMLArmatureNode.h"
#include "vtkMRMLBoneDisplayNode.h"
#include "vtkMRMLBoneNode.h"
#include <vtkBVHReader.h>

// VTK includes
#include <vtkCallbackCommand.h>
#include <vtkCellData.h>
#include <vtkCollection.h>
#include <vtkDoubleArray.h>
#include <vtkFloatArray.h>
#include <vtkIdTypeArray.h>
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkObserverManager.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataReader.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
#include <vtksys/SystemTools.hxx>

// Slicer includes
#include <vtkMRMLHierarchyNode.h>
#include <vtkMRMLScene.h>

// STD includes
#include <sstream>
#include <locale>

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLArmatureStorageNode);

//----------------------------------------------------------------------------
vtkMRMLArmatureStorageNode::vtkMRMLArmatureStorageNode()
{
  this->CurrentlyAddedBoneNode = 0;
  this->CurrentlyAddedBoneNodeParent = 0;
  this->CurrentlyAddedArmatureNode = 0;

  this->SceneObserverManager = vtkObserverManager::New();
  this->SceneObserverManager->AssignOwner(this);
  this->SceneObserverManager->GetCallbackCommand()->SetClientData(
    reinterpret_cast<void *>(this));
  this->SceneObserverManager->GetCallbackCommand()->SetCallback(
    vtkMRMLArmatureStorageNode::MRMLSceneCallback);
}

//----------------------------------------------------------------------------
vtkMRMLArmatureStorageNode::~vtkMRMLArmatureStorageNode()
{
  this->SceneObserverManager->Delete();
}

//----------------------------------------------------------------------------
void vtkMRMLArmatureStorageNode::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os,indent);
}

//----------------------------------------------------------------------------
bool vtkMRMLArmatureStorageNode::CanReadInReferenceNode(vtkMRMLNode *refNode)
{
  return refNode->IsA("vtkMRMLArmatureNode");
}

//----------------------------------------------------------------------------
void vtkMRMLArmatureStorageNode
::MRMLSceneCallback(vtkObject *caller, unsigned long eid,
                    void *clientData, void *callData)
{
  vtkMRMLArmatureStorageNode *self =
    reinterpret_cast<vtkMRMLArmatureStorageNode *>(clientData);
  assert(vtkMRMLScene::SafeDownCast(caller));
  assert(caller == self->GetScene());

  self->ProcessMRMLSceneEvents(caller, eid, callData);
}

//----------------------------------------------------------------------------
void vtkMRMLArmatureStorageNode
::ProcessMRMLSceneEvents(vtkObject *caller, unsigned long eid, void *callData)
{
  vtkMRMLNode* node = reinterpret_cast<vtkMRMLNode*>(callData);
  vtkMRMLBoneNode* boneNode = vtkMRMLBoneNode::SafeDownCast(node);
  if (!boneNode || boneNode != this->CurrentlyAddedBoneNode)
    {
    return;
    }

  vtkNew<vtkMRMLAnnotationHierarchyNode> hierarchyNode;
  hierarchyNode->AllowMultipleChildrenOff();
  hierarchyNode->SetName(
    this->GetScene()->GetUniqueNameByString("AnnotationHierarchy"));

  vtkMRMLHierarchyNode* parentHierarchyNode;
  if (this->CurrentlyAddedBoneNodeParent)
    {
    parentHierarchyNode =
      vtkMRMLHierarchyNode::GetAssociatedHierarchyNode(
        this->GetScene(), this->CurrentlyAddedBoneNodeParent->GetID());
    }
  else
    {
    parentHierarchyNode = this->CurrentlyAddedArmatureNode;
    }
  hierarchyNode->SetParentNodeID(parentHierarchyNode->GetID());

  this->GetScene()->AddNode(hierarchyNode.GetPointer());

  boneNode->SetDisableModifiedEvent(1);
  hierarchyNode->SetDisplayableNodeID(boneNode->GetID());
  boneNode->SetDisableModifiedEvent(0);
}

//----------------------------------------------------------------------------
int vtkMRMLArmatureStorageNode::CreateArmatureFromModel(
  vtkMRMLArmatureNode* armatureNode, vtkPolyData* model)
{
  if (!model || !armatureNode)
    {
    std::cerr<<"Cannot create armature, model or armature is null"<<std::endl;
    return 0;
    }

  vtkPoints* points = model->GetPoints();
  if (!points)
    {
    std::cerr<<"Cannot create armature from model, No points !"<<std::endl;
    return 0;
    }

  vtkCellData* cellData = model->GetCellData();
  if (!cellData)
    {
    std::cerr<<"Cannot create armature from model, No cell data"<<std::endl;
    return 0;
    }

  vtkIdTypeArray* parenthood =
    vtkIdTypeArray::SafeDownCast(cellData->GetArray("Parenthood"));
  if (!parenthood
    || parenthood->GetNumberOfTuples()*2 != points->GetNumberOfPoints())
    {
    std::cerr<<"Cannot create armature from model,"
      <<" parenthood array invalid"<<std::endl;
    if (parenthood)
      {
      std::cerr<<parenthood->GetNumberOfTuples()<<std::endl;
      }
    else
      {
      std::cerr<<"No parenthood array"<<std::endl;
      }

    return 0;
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

  // HACK:
  // The vtkSlicerAnnotationModuleLogic adds the hierarchy nodes with no
  // respect for the bone parent. To make this right, we add
  // an observer that needs to be call before the annotation logic to
  // make sure that the hierarchy node is added properly.
  // The CurrentlyAdded*Node are used to know what is the current armature
  // and the current bones.
  this->CurrentlyAddedArmatureNode = armatureNode;

  vtkNew<vtkIntArray> events;
  events->InsertNextValue(vtkMRMLScene::NodeAddedEvent);
  vtkNew<vtkFloatArray> priorities;
  priorities->InsertNextValue(1.0);
  this->SceneObserverManager->AddObjectEvents(
    this->GetScene(), events.GetPointer(), priorities.GetPointer());

  vtkNew<vtkCollection> addedBones;
  for (vtkIdType id = 0, pointId = 0;
    id < parenthood->GetNumberOfTuples(); ++id, pointId += 2)
    {
    vtkIdType parentId = parenthood->GetValue(id);
    if (parentId > id)
      {
      std::cerr<<"There most likely were reparenting."
        << " Not supported yet."<<std::endl;
      return 0;
      }

    vtkMRMLBoneNode* boneParentNode = 0;
    if (parentId > -1)
      {
      boneParentNode =
        vtkMRMLBoneNode::SafeDownCast(addedBones->GetItemAsObject(parentId));
      if (! boneParentNode)
        {
        std::cerr<<"Could not find bone parent ! Stopping"<<std::endl;
        return 0;
        }
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

    this->CurrentlyAddedBoneNode = boneNode;
    this->CurrentlyAddedBoneNodeParent = boneParentNode;
    boneNode->Initialize(this->GetScene());
    this->CurrentlyAddedBoneNodeParent = 0;
    this->CurrentlyAddedBoneNode = 0;

    addedBones->AddItem(boneNode);
    boneNode->Delete();
    }

  // Reset observer and CurrentlyAdded*Node variables
  this->SceneObserverManager->RemoveObjectEvents(this->GetScene());
  this->CurrentlyAddedBoneNode = 0;
  this->CurrentlyAddedBoneNodeParent = 0;
  this->CurrentlyAddedArmatureNode = 0;

  return 1;
}

//----------------------------------------------------------------------------
int vtkMRMLArmatureStorageNode::ReadDataInternal(vtkMRMLNode *refNode)
{
  vtkMRMLArmatureNode* armatureNode =
    dynamic_cast <vtkMRMLArmatureNode*>(refNode);

  std::string fullName = this->GetFullNameFromFileName();
  if (fullName == std::string(""))
    {
    vtkErrorMacro("ReadDataInternal: File name not specified");
    return 0;
    }
  
  // check that the file exists
  if (vtksys::SystemTools::FileExists(fullName.c_str()) == false)
    {
    vtkErrorMacro("ReadDataInternal: model file '"
      << fullName.c_str() << "' not found.");
    return 0;
    }
      
  // compute file prefix
  std::string name(fullName);
  std::string::size_type loc = name.find_last_of(".");
  if( loc == std::string::npos )
    {
    vtkErrorMacro("ReadDataInternal: no file extension specified: "
      << name.c_str());
    return 0;
    }
  std::string extension = name.substr(loc);

  vtkDebugMacro("ReadDataInternal: extension = " << extension.c_str());

  int result = 1;
  try
    {
    if (extension == std::string(".bvh"))
      {
      vtkSmartPointer<vtkBVHReader> reader
        = vtkSmartPointer<vtkBVHReader>::New(); //\todo keep the reader alive
      reader->SetFileName(fullName.c_str());
      reader->Update();

      // Do Stuff

      }
    else if (extension == std::string(".vtk")
      || extension == std::string(".arm"))
      {
      vtkNew<vtkPolyDataReader> reader;
      reader->SetFileName(fullName.c_str());
      reader->Update();

      result =
        this->CreateArmatureFromModel(armatureNode, reader->GetOutput());
      }
    else
      {
      vtkDebugMacro("Cannot read armature file '" << name.c_str()
        << "' (extension = " << extension.c_str() << ")");
      return 0;
      }
    }
  catch (...)
    {
    result = 0;
    }

  return result;
}

//------------------------------------------------------------------------------
int vtkMRMLArmatureStorageNode::WriteDataInternal(vtkMRMLNode* vtkNotUsed(refNode))
{
  return 0;
}

//----------------------------------------------------------------------------
void vtkMRMLArmatureStorageNode::InitializeSupportedReadFileTypes()
{
  this->SupportedReadFileTypes->InsertNextValue(
    "Biovision Hierarchy (BVH) (.bvh)");
}

//----------------------------------------------------------------------------
void vtkMRMLArmatureStorageNode::InitializeSupportedWriteFileTypes()
{
  this->SupportedWriteFileTypes->Reset();
  this->SupportedWriteFileTypes->SetNumberOfTuples(0);
}

//----------------------------------------------------------------------------
const char* vtkMRMLArmatureStorageNode::GetDefaultWriteFileExtension()
{
  return "bvh";
}

