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

#include "vtkArmatureReader.h"

#include "vtkArmatureWidget.h"
#include "vtkBoneWidget.h"
#include "vtkCellData.h"
#include "vtkCollection.h"
#include "vtkDoubleArray.h"
#include "vtkExecutive.h"
#include "vtkIdTypeArray.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkNew.h"
#include "vtkObjectFactory.h"
#include "vtkPolyData.h"
#include "vtkPolyDataReader.h"
#include "vtkStringArray.h"

vtkStandardNewMacro(vtkArmatureReader);

//----------------------------------------------------------------------------
vtkArmatureReader::vtkArmatureReader()
{
  this->FileName = "";
  this->Armature = 0;
  this->ArmatureIsValid = 0;

  this->SetNumberOfInputPorts(0);
  this->SetNumberOfOutputPorts(0);
}

//----------------------------------------------------------------------------
vtkArmatureReader::~vtkArmatureReader()
{
  if (this->Armature)
    {
    this->Armature->Delete();
    }
}

//----------------------------------------------------------------------------
void vtkArmatureReader::SetFileName(const char* filename)
{
  if (!filename && this->FileName == ""
    || filename && strcmp(this->FileName.c_str(), filename) == 0)
    {
    return;
    }

  this->FileName = filename ? filename : "";
  this->ArmatureIsValid = 0;
  this->Modified();
}

//----------------------------------------------------------------------------
const char* vtkArmatureReader::GetFileName() const
{
  return this->FileName.c_str();
}

//----------------------------------------------------------------------------
vtkArmatureWidget* vtkArmatureReader::GetArmature()
{
  return this->Armature;
}

//----------------------------------------------------------------------------
int vtkArmatureReader
::RequestData(vtkInformation *vtkNotUsed(request),
              vtkInformationVector **vtkNotUsed(inputVector),
              vtkInformationVector *outputVector)
{
  if (this->Armature && !this->ArmatureIsValid)
    {
    this->InvalidReader();
    }

  // Create new armature if necessary
  if (!this->Armature)
    {
    this->Armature = vtkArmatureWidget::New();
    if (this->FileName == "")
      {
      vtkErrorMacro("A file name must be specified.");
      return 0;
      }

    vtkNew<vtkPolyDataReader> reader;
    reader->SetFileName(this->FileName.c_str());
    reader->Update();

    this->ArmatureIsValid = this->Parse(reader->GetOutput());
    }

  return this->ArmatureIsValid;
}

//----------------------------------------------------------------------------
void vtkArmatureReader::InvalidReader()
{
  this->ArmatureIsValid = false;
  if (this->Armature)
    {
    this->Armature->Delete();
    this->Armature = NULL;
    }
}

//----------------------------------------------------------------------------
int vtkArmatureReader::Parse(vtkPolyData* polydata)
{
  if (!polydata)
    {
    return 0;
    }

  vtkPoints* points = polydata->GetPoints();
  if (!points)
    {
    std::cerr<<"Cannot create armature from model, No points !"<<std::endl;
    return 0;
    }

  vtkCellData* cellData = polydata->GetCellData();
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

    vtkBoneWidget* boneParent = 0;
    if (parentId > -1)
      {
      boneParent =
        vtkBoneWidget::SafeDownCast(addedBones->GetItemAsObject(parentId));
      if (! boneParent)
        {
        std::cerr<<"Could not find bone parent ! Stopping"<<std::endl;
        return 0;
        }
      }

    std::string name = "";
    if (names)
      {
      name = names->GetValue(id);
      }
    vtkBoneWidget* bone = this->Armature->CreateBone(boneParent, name);

    double p[3];
    points->GetPoint(pointId, p);
    bone->SetWorldHeadRest(p);
    points->GetPoint(pointId + 1, p);
    bone->SetWorldTailRest(p);

    if (restToPose)
      {
      double quad[4];
      restToPose->GetTupleValue(id, quad);
      bone->SetRestToPoseRotation(quad);
      }

    bool linkedWithParent = true;
    if (boneParent)
      {
      double diff[3];
      vtkMath::Subtract(
        boneParent->GetWorldTailRest(), bone->GetWorldHeadRest(), diff);
      linkedWithParent = vtkMath::Dot(diff, diff) < 1e-6;
      }

    this->Armature->AddBone(bone, boneParent, linkedWithParent);
    addedBones->AddItem(bone);
    bone->Delete();
    }

  return 1;
}

//----------------------------------------------------------------------------
int vtkArmatureReader::CanReadFile(const char *filename)
{
  std::ifstream file(filename);
  if (!file.good())
    {
    return 0;
    }

  std::string name = filename;
  if (name.size() < 3
    || (name.substr(name.size() - 3) != "vtk"
      && name.substr(name.size() - 3) != "arm"))
    {
    return 0;
    }

  return 1;
}

//----------------------------------------------------------------------------
void vtkArmatureReader::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  os << indent << "File Name: " << this->FileName << "\n";
}
