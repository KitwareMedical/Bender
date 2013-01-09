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
#include "vtkMRMLBoneDisplayNode.h"
#include "vtkMRMLNodeHelper.h"

// VTK includes
#include <vtkObjectFactory.h>
#include <vtkProperty.h>

// Bender includes
#include <vtkBoneEnvelopeRepresentation.h>
#include <vtkBoneRepresentation.h>
#include <vtkBoneWidget.h>
#include <vtkCylinderBoneRepresentation.h>
#include <vtkDoubleConeBoneRepresentation.h>

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLBoneDisplayNode);

//----------------------------------------------------------------------------
vtkMRMLBoneDisplayNode::vtkMRMLBoneDisplayNode()
{
  this->ShowEnvelope = 0;
  this->EnvelopeRadius = 10.0;
  this->SetVisibility(1);
}

//----------------------------------------------------------------------------
vtkMRMLBoneDisplayNode::~vtkMRMLBoneDisplayNode()
{
}

//----------------------------------------------------------------------------
void vtkMRMLBoneDisplayNode::WriteXML(ostream& of, int nIndent)
{
  this->Superclass::WriteXML(of, nIndent);

  vtkIndent indent(nIndent);
  of << indent << " InteractionColor=\""
    << this->InteractionColor[0] << " "
    << this->InteractionColor[1] << " "
    << this->InteractionColor[2] << "\"";
  of << indent << " ShowEnvelope=\"" << this->ShowEnvelope << "\"";
  of << indent << " EnvelopeRadius=\"" << this->EnvelopeRadius << "\"";
}

//----------------------------------------------------------------------------
void vtkMRMLBoneDisplayNode::ReadXMLAttributes(const char** atts)
{
  int disabledModify = this->StartModify();

  this->Superclass::ReadXMLAttributes(atts);

  while (*atts != NULL)
    {
    const char* attName = *(atts++);
    std::string attValue(*(atts++));

    if (!strcmp(attName, "InteractionColor"))
      {
      double rgb[3];
      vtkMRMLNodeHelper::StringToVector3(attValue, rgb);
      this->SetInteractionColor(rgb);
      }
    if (!strcmp(attName, "ShowEnvelope"))
      {
      this->SetShowEnvelope(
        vtkMRMLNodeHelper::StringToInt(attValue));
      }
    if (!strcmp(attName, "EnvelopeRadius"))
      {
      this->SetShowEnvelope(
        vtkMRMLNodeHelper::StringToDouble(attValue));
      }
    }
  this->EndModify(disabledModify);
}

//-----------------------------------------------------------
void vtkMRMLBoneDisplayNode::Copy(vtkMRMLNode* node)
{
  this->Superclass::Copy(node);
}

//-----------------------------------------------------------
void vtkMRMLBoneDisplayNode::UpdateScene(vtkMRMLScene *scene)
{
  this->Superclass::UpdateScene(scene);
}

//---------------------------------------------------------------------------
void vtkMRMLBoneDisplayNode::ProcessMRMLEvents(vtkObject* caller,
                                        unsigned long event,
                                        void* callData)
{
  this->Superclass::ProcessMRMLEvents(caller, event, callData);
}

//---------------------------------------------------------------------------
void vtkMRMLBoneDisplayNode::SetColor(double color[3])
{
  this->SetColor(color[0], color[1], color[2]);
}

//---------------------------------------------------------------------------
void vtkMRMLBoneDisplayNode::SetColor(double r, double g, double b)
{
  this->Superclass::SetColor(r, g, b);

  double h, s, v;
  vtkMath::RGBToHSV(r, g, b, &h, &s, &v);
  v *= 1.5; // enlight
  vtkMath::HSVToRGB(h, s, v, &r, &g, &b);
  this->SetSelectedColor(r, g, b);

  v *= 1.2; // enlight
  vtkMath::HSVToRGB(h, s, v, &r, &g, &b);
  this->SetInteractionColor(r, g, b);
}

//----------------------------------------------------------------------------
void vtkMRMLBoneDisplayNode::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}

//---------------------------------------------------------------------------
void vtkMRMLBoneDisplayNode
::CopyBoneWidgetDisplayProperties(vtkBoneWidget* boneWidget)
{
  if (!boneWidget)
    {
    return;
    }

  // -- Color --
  // Color is never updated from the widget because the widget selected color
  // and normal color aren't synched with the node colors.

  // -- Opacity --
  this->SetOpacity(
    boneWidget->GetBoneRepresentation()->GetLineProperty()->GetOpacity());

  this->SetShowEnvelope(
    boneWidget->GetBoneRepresentation()->GetShowEnvelope());
  this->SetEnvelopeRadius(
    boneWidget->GetBoneRepresentation()->GetEnvelope()->GetRadius());
}

//---------------------------------------------------------------------------
void vtkMRMLBoneDisplayNode
::PasteBoneDisplayNodeProperties(vtkBoneWidget* boneWidget)
{
  if (!boneWidget)
    {
    return;
    }

  // -- Color --
  double color[3];
  if (this->GetSelected())
    {
    this->GetSelectedColor(color);
    }
  else
    {
    this->GetColor(color);
    }
  double interactionColor[3];
  this->GetInteractionColor(interactionColor);

  vtkCylinderBoneRepresentation* cylinderRep =
    vtkCylinderBoneRepresentation::SafeDownCast(
      boneWidget->GetBoneRepresentation());
  if (cylinderRep)
    {
    cylinderRep->GetCylinderProperty()->SetColor(color);
    cylinderRep->GetSelectedCylinderProperty()->SetColor(interactionColor);
    }
  vtkDoubleConeBoneRepresentation* doubleConeRep =
    vtkDoubleConeBoneRepresentation::SafeDownCast(
      boneWidget->GetBoneRepresentation());
  if (doubleConeRep)
    {
    doubleConeRep->GetConesProperty()->SetColor(color);
    doubleConeRep->GetSelectedConesProperty()->SetColor(interactionColor);
    }
  boneWidget->GetBoneRepresentation()->GetLineProperty()->SetColor(color);
  boneWidget->GetBoneRepresentation()->GetSelectedLineProperty()
    ->SetColor(interactionColor);

  // -- Opacity --
  boneWidget->GetBoneRepresentation()->SetOpacity(this->GetOpacity());
  //And the parenthood line:
  boneWidget->GetParenthoodRepresentation()->GetLineProperty()->SetOpacity(
    this->GetOpacity());
  boneWidget->GetParenthoodRepresentation()->GetEndPointProperty()
    ->SetOpacity(this->GetOpacity());
  boneWidget->GetParenthoodRepresentation()->GetEndPoint2Property()
    ->SetOpacity(this->GetOpacity());

  // -- Envelope --
  boneWidget->GetBoneRepresentation()->SetShowEnvelope(
    this->GetShowEnvelope());
  boneWidget->GetBoneRepresentation()->GetEnvelope()->SetRadius(
    this->GetEnvelopeRadius());
}
