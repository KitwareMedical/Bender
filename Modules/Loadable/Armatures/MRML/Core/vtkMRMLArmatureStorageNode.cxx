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
#include <vtkBVHReader.h>
#include "vtkMRMLArmatureNode.h"
#include "vtkMRMLBoneDisplayNode.h"
#include "vtkMRMLBoneNode.h"

// VTK includes
#include <vtkCallbackCommand.h>
#include <vtkCellData.h>
#include <vtkCollection.h>
#include <vtkDoubleArray.h>
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
  this->BVHReader = 0;
}

//----------------------------------------------------------------------------
vtkMRMLArmatureStorageNode::~vtkMRMLArmatureStorageNode()
{
  if (this->BVHReader)
    {
    this->BVHReader->Delete();
    }
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
unsigned int vtkMRMLArmatureStorageNode::GetNumberOfFrames()
{
  if (this->BVHReader)
    {
    return this->BVHReader->GetNumberOfFrames();
    }
  return 0;
}

//----------------------------------------------------------------------------
double vtkMRMLArmatureStorageNode::GetFrameRate()
{
  if (this->BVHReader)
    {
    return this->BVHReader->GetFrameRate();
    }
  return 0;
}

//----------------------------------------------------------------------------
void vtkMRMLArmatureStorageNode
::SetFrame(vtkMRMLArmatureNode* armatureNode, unsigned int frame)
{
  if (!armatureNode || !this->BVHReader)
    {
    return;
    }
  if (frame > this->GetNumberOfFrames())
    {
    std::cerr<<"The input frame exceeds the total number of frames."<<std::endl
      <<" -> Defaulting to the last frame."<<std::endl;
    frame = this->GetNumberOfFrames();
    }

  armatureNode->SetWidgetState(vtkMRMLArmatureNode::Pose);
  armatureNode->ResetPoseMode();

  vtkNew<vtkCollection> bones;
  armatureNode->GetAllBones(bones.GetPointer());
  for (int i = 0; i < bones->GetNumberOfItems(); ++i)
    {
    vtkMRMLBoneNode* boneNode =
      vtkMRMLBoneNode::SafeDownCast(bones->GetItemAsObject(i));
    assert(boneNode);

    vtkQuaterniond rotation =
      this->BVHReader->GetParentToBoneRotation(frame, i);
    double axis[3];
    double angle = rotation.GetRotationAngleAndAxis(axis);
    boneNode->RotateTailWithParentWXYZ(angle, axis);
    }
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

    vtkMRMLAnnotationHierarchyNode* parentHierarchyNode = 0;
    if (boneParentNode)
      {
      parentHierarchyNode = boneParentNode->GetHierarchyNode();
      }
    else
      {
      parentHierarchyNode = armatureNode;
      }
    boneNode->Initialize(this->GetScene(), parentHierarchyNode);


    addedBones->AddItem(boneNode);
    boneNode->Delete();
    }

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

  if (this->BVHReader)
    {
    this->BVHReader->Delete();
    }

  int result = 1;
  try
    {
    if (extension == std::string(".bvh"))
      {
      this->BVHReader = vtkBVHReader::New();
      this->BVHReader->SetFileName(fullName.c_str());
      this->BVHReader->Update();

      result =
        this->CreateArmatureFromModel(
          armatureNode, this->BVHReader->GetOutput());
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

