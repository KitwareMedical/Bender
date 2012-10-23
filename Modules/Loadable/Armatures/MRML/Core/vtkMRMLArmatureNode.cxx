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
//#include "vtkMRMLBoneStorageNode.h"

// MRML includes
#include <vtkMRMLScene.h>

// VTK includes
#include <vtkObjectFactory.h>

// STD includes
//#include <algorithm>
//#include <cassert>
//#include <sstream>

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLArmatureNode);

//----------------------------------------------------------------------------
vtkMRMLArmatureNode::vtkMRMLArmatureNode()
{
  this->SetHideFromEditors(0);
}

//----------------------------------------------------------------------------
vtkMRMLArmatureNode::~vtkMRMLArmatureNode()
{
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
