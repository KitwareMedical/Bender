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
#include "vtkSlicerModelsLogic.h"

// Armatures includes
#include "vtkSlicerArmaturesLogic.h"

// MRML includes
#include <vtkMRMLModelNode.h>

// VTK includes
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkXMLDataElement.h>
#include <vtkXMLDataParser.h>
#include <vtksys/SystemTools.hxx>

// STD includes
#include <cassert>

vtkCxxSetObjectMacro(vtkSlicerArmaturesLogic, ModelsLogic, vtkSlicerModelsLogic);

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerArmaturesLogic);

//----------------------------------------------------------------------------
vtkSlicerArmaturesLogic::vtkSlicerArmaturesLogic()
{
  this->ModelsLogic = 0;
}

//----------------------------------------------------------------------------
vtkSlicerArmaturesLogic::~vtkSlicerArmaturesLogic()
{
}

//----------------------------------------------------------------------------
void vtkSlicerArmaturesLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
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

  vtkNew<vtkUnsignedCharArray> colors;
  colors->SetNumberOfComponents(3);
  colors->SetName("Colors");
  armature->GetPointData()->SetScalars(colors.GetPointer());

  vtkXMLDataElement* armatureElement = armatureParser->GetRootElement();

  double origin[3] = {0., 0., 0.};
  armatureElement->GetVectorAttribute("location", 3, origin);

  double scale[3] = {1.0, 1.0, 1.0};
  armatureElement->GetVectorAttribute("scale", 3, scale);
  double scaleMat[3][3];
  vtkMath::Identity3x3(scaleMat);
  scaleMat[0][0] = scale[0];
  scaleMat[1][1] = scale[1];
  scaleMat[2][2] = scale[2];

  double orientationXYZW[4] = {0.0, 0.0, 0.0, 1.0};
  armatureElement->GetVectorAttribute("orientation", 4, orientationXYZW);
  double orientationWXYZ[4] = {orientationXYZW[3], orientationXYZW[0],
                               orientationXYZW[1], orientationXYZW[2]};
  double mat[3][3];
  vtkMath::QuaternionToMatrix3x3(orientationWXYZ, mat);

  vtkMath::Multiply3x3(mat, scaleMat, mat);

  for (int child = 0; child < armatureElement->GetNumberOfNestedElements(); ++child)
    {
    this->ReadBone(armatureElement->GetNestedElement(child), armature.GetPointer(),
                   origin, mat);
    }
  vtkMRMLModelNode* modelNode= this->ModelsLogic->AddModel(armature.GetPointer());
  std::string modelName = vtksys::SystemTools::GetFilenameName(fileName);
  modelNode->SetName(modelName.c_str());
  return modelNode;
}

//----------------------------------------------------------------------------
void vtkSlicerArmaturesLogic::ReadBone(vtkXMLDataElement* boneElement,
                                       vtkPolyData* polyData,
                                       const double origin[3],
                                       const double parentMatrix[3][3],
                                       const double parentLength)
{
  double parentTransMatrix[3][3];
  vtkMath::Transpose3x3(parentMatrix, parentTransMatrix);

  double localOrientationXYZW[4] = {0., 0., 0., 1.};
  boneElement->GetVectorAttribute("orientation", 4, localOrientationXYZW);
  double localOrientationWXYZ[4] = {localOrientationXYZW[3], localOrientationXYZW[0],
                                    localOrientationXYZW[1], localOrientationXYZW[2]};
  double mat[3][3];
  vtkMath::QuaternionToMatrix3x3(localOrientationWXYZ, mat);
  vtkMath::Invert3x3(mat, mat);
  vtkMath::Multiply3x3(mat, parentMatrix, mat);

  bool applyPose = true;
  if (applyPose)
    {
    vtkXMLDataElement * poseElement =
      boneElement->FindNestedElementWithName("pose");
    double poseRotationXYZW[4] = {0., 0., 0., 0.};
    poseElement->GetVectorAttribute("rotation", 4, poseRotationXYZW);
    double poseRotationWXYZ[4] = {poseRotationXYZW[3], poseRotationXYZW[0],
                                  poseRotationXYZW[1], poseRotationXYZW[2]};
    double poseMat[3][3];
    vtkMath::QuaternionToMatrix3x3(poseRotationWXYZ, poseMat);
    vtkMath::Invert3x3(poseMat, poseMat);
    vtkMath::Multiply3x3(poseMat, mat, mat);
    }

  double localHead[3] = {0., 0., 0.};
  boneElement->GetVectorAttribute("head", 3, localHead);

  double head[3] = {0., 0., 0.};
  head[0] = localHead[0];
  head[1] = localHead[1] + parentLength;
  head[2] = localHead[2];
  vtkMath::Multiply3x3(parentTransMatrix, head, head);
  vtkMath::Add(origin, head, head);

  double localTail[3] = {0., 0. ,0.};
  boneElement->GetVectorAttribute("tail", 3, localTail);

  double tail[3] = {0., 0. ,0};
  vtkMath::Subtract(localTail, localHead, tail);
  double length = vtkMath::Norm(tail);

  tail[0] = mat[1][0]; tail[1] = mat[1][1]; tail[2] = mat[1][2];
  vtkMath::MultiplyScalar(tail, length);

  vtkMath::Add(head, tail, tail);

  vtkIdType indexes[2];
  indexes[0] = polyData->GetPoints()->InsertNextPoint(head);
  indexes[1] = polyData->GetPoints()->InsertNextPoint(tail);
  static unsigned char color[3] = {255, 255, 255};
  unsigned char offset = 20;
  polyData->GetPointData()->GetScalars("Colors")->InsertNextTuple3(
    color[0], color[1], color[2]);
  color[0] -= offset; color[1] -= offset; color[2] -= offset;
  polyData->GetPointData()->GetScalars("Colors")->InsertNextTuple3(
    color[0], color[1], color[2]);
  color[0] -= offset; color[1] -= offset; color[2] -= offset;

  polyData->InsertNextCell(VTK_LINE, 2, indexes);

  for (int child = 0; child < boneElement->GetNumberOfNestedElements(); ++child)
    {
    vtkXMLDataElement* childElement = boneElement->GetNestedElement(child);
    if (strcmp(childElement->GetName(),"bone") == 0)
      {
      this->ReadBone(childElement, polyData, head, mat, length);
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
                                       double parentOrientation[4], bool preMult)
{
  double poseRotationXYZW[4] = {0., 0., 0., 0.};
  poseElement->GetVectorAttribute("rotation", 4, poseRotationXYZW);
  double poseRotationWXYZ[4] = {poseRotationXYZW[3], poseRotationXYZW[0],
                                poseRotationXYZW[1], poseRotationXYZW[2]};
  if (preMult)
    {
    vtkMath::MultiplyQuaternion(poseRotationWXYZ, parentOrientation, parentOrientation);
    }
  else
    {
    vtkMath::MultiplyQuaternion(parentOrientation, poseRotationWXYZ, parentOrientation);
    }
}
