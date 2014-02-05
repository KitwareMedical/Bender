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

  This file was originally developed by Johan Andruejol, Kitware Inc.

=========================================================================*/

#include "vtkBrokenCells.h"

// VTK includes
#include <vtkCell.h>
#include <vtkCenterOfMass.h>
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPoints.h>

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkBrokenCells);

//----------------------------------------------------------------------------
vtkBrokenCells::vtkBrokenCells()
{
  this->Points = 0;
  this->Verbose = false;
}

//----------------------------------------------------------------------------
vtkBrokenCells::~vtkBrokenCells()
{
  for (BrokenCellsMapIterator it = this->Cells.begin();
    it != Cells.end(); ++it)
    {
    it->second->UnRegister(this);
    }
}

//----------------------------------------------------------------------------
bool vtkBrokenCells::RepairAllCells()
{
  for (BrokenCellsMapIterator it = this->Cells.begin(); it != Cells.end();
    it = this->Cells.upper_bound(it->first))
    {
    if (!this->RepairCells(it->first))
      {
      return false;
      }
    }
  return true;
}

//----------------------------------------------------------------------------
bool vtkBrokenCells::RepairCells(vtkIdType vertexIndex)
{
  if (!this->Points)
    {
    std::cout<<"Cannot repair without points."<<std::endl;
    return false;
    }

  std::pair<BrokenCellsMapIterator, BrokenCellsMapIterator> range =
    this->Cells.equal_range(vertexIndex);
  if (range.first == this->Cells.end())
    {
    std::cout<<"Could not find given index: "<<vertexIndex<<std::endl;
    return false;
    }

  if (this->Verbose)
    {
    std::cout<<"Repairing invalid point with index "
      <<vertexIndex<<" with the following cell points:"<<std::endl;
    }

  vtkNew<vtkPoints> nonNaNPoints;
  int cellCount = 0;
  for (BrokenCellsMapIterator it = range.first; it != range.second; ++it)
    {
    if (this->Verbose)
      {
      std::cout<<"Cell #"<<cellCount++<<std::endl;
      }
    for (vtkIdType j = 0; j < it->second->GetNumberOfPoints(); ++j)
      {
      vtkIdType currentId = it->second->GetPointId(j);
      if (currentId == it->first)
        {
        continue;
        }

      double p[3];
      this->Points->GetPoint(currentId, p);
      nonNaNPoints->InsertNextPoint(p);

      if (this->Verbose)
        {
        std::cout<<"  Point with index "<<currentId<<": "
          <<p[0]<<" "<<p[1]<<" "<<p[2]<<std::endl;
        }
      }
    }

  vtkNew<vtkCenterOfMass> centerOfMass;
  double center[3];
  centerOfMass->ComputeCenterOfMass(nonNaNPoints.GetPointer(), NULL, center);
  this->Points->SetPoint((range.first)->first, center);

  if (this->Verbose)
    {
    std::cout<<"Invalid point was replaced by "
      <<center[0]<<" "<<center[1]<<" "<<center[2]<<std::endl;
    }

  return true;
}

//----------------------------------------------------------------------------
void vtkBrokenCells::AddCell(vtkIdType vertexIndex, vtkCell* cell)
{
  if (!cell)
    {
    return;
    }

  cell->Register(this);
  this->Cells.insert(std::pair<vtkIdType, vtkCell*>(vertexIndex, cell));
}

//----------------------------------------------------------------------------
void vtkBrokenCells::SetPoints(vtkPoints* points)
{
  if (this->Points)
    {
    this->Points->UnRegister(this);
    }
  this->Points = points;
  if (this->Points)
    {
    this->Points->Register(this);
    }
}

namespace
{
//----------------------------------------------------------------------------
bool IsNaN(double x, double y, double z)
{
  return vtkMath::IsNan(x) || vtkMath::IsNan(y) || vtkMath::IsNan(z);
}

//----------------------------------------------------------------------------
bool IsPositiveInfinite(double x, double y, double z)
{
  return x >= vtkMath::Inf() || y >= vtkMath::Inf() || z >= vtkMath::Inf();
}

//----------------------------------------------------------------------------
bool IsNegativeInfinite(double x, double y, double z)
{
  return x <= vtkMath::NegInf()
    || y <= vtkMath::NegInf()
    || z <= vtkMath::NegInf();
}

//----------------------------------------------------------------------------
bool IsInfinite(double x, double y, double z)
{
  return IsPositiveInfinite(x, y, z) || IsNegativeInfinite(x, y, z);
}

} // end namespace

//----------------------------------------------------------------------------
bool vtkBrokenCells::IsPointValid(double x, double y, double z)
{
  return !IsNaN(x, y, z) && !IsInfinite(x, y, z);
}

//----------------------------------------------------------------------------
bool vtkBrokenCells::IsPointValid(double p[3])
{
  return IsPointValid(p[0], p[1], p[2]);
}
