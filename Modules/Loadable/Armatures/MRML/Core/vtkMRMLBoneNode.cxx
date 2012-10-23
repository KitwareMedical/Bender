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
#include <vtkBoneWidget.h>

// MRML includes
#include <vtkMRMLScene.h>

// VTK includes
#include <vtkNew.h>
#include <vtkObjectFactory.h>

// STD includes
//#include <algorithm>
//#include <cassert>
//#include <sstream>

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLBoneNode);

//----------------------------------------------------------------------------
vtkMRMLBoneNode::vtkMRMLBoneNode()
{
  this->BoneProperties = vtkBoneWidget::New();
  this->BoneProperties->CreateDefaultRepresentation();
  this->BoneProperties->SetWidgetStateToRest();
}

//----------------------------------------------------------------------------
vtkMRMLBoneNode::~vtkMRMLBoneNode()
{
  this->BoneProperties->Delete();
}

//----------------------------------------------------------------------------
void vtkMRMLBoneNode::WriteXML(ostream& of, int nIndent)
{
  this->Superclass::WriteXML(of, nIndent);
  // of << indent << " ctrlPtsNumberingScheme=\"" << this->NumberingScheme << "\"";
}

//----------------------------------------------------------------------------
void vtkMRMLBoneNode::ReadXMLAttributes(const char** atts)
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
void vtkMRMLBoneNode::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}

//----------------------------------------------------------------------------
void vtkMRMLBoneNode::Initialize(vtkMRMLScene* mrmlScene)
{
  if (!mrmlScene)
    {
    vtkErrorMacro(<< __FUNCTION__ << ": No scene");
    return;
    }
  // \tbd remove this SetScene call as it shouldn't be mandatory.
  this->SetScene(mrmlScene);
  this->Superclass::Initialize(mrmlScene);
  this->CreateBoneDisplayNode();
}

//----------------------------------------------------------------------------
vtkMRMLBoneDisplayNode* vtkMRMLBoneNode::GetBoneDisplayNode()
{
  return vtkMRMLBoneDisplayNode::SafeDownCast(
    this->GetNthDisplayNodeByClass(0, "vtkMRMLBoneDisplayNode"));
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
void vtkMRMLBoneNode::SetWorldHeadRest(double headPoint[3])
{
  this->BoneProperties->SetWorldHeadRest(headPoint);
  this->Modified();
}

//---------------------------------------------------------------------------
double* vtkMRMLBoneNode::GetWorldHeadRest()
{
  return this->BoneProperties->GetWorldHeadRest();
}

//---------------------------------------------------------------------------
void vtkMRMLBoneNode::SetWorldTailRest(double tailPoint[3])
{
  this->BoneProperties->SetWorldTailRest(tailPoint);
  this->Modified();
}

//---------------------------------------------------------------------------
double* vtkMRMLBoneNode::GetWorldTailRest()
{
  return this->BoneProperties->GetWorldTailRest();
}

