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

#ifndef __vtkBrokenCells_h
#define __vtkBrokenCells_h

// VTK includes
#include <vtkObject.h>

class vtkCell;
class vtkPoints;

// STD includes
#include <map>

// Repairs a list of cells that have a common invalid vertex.
// A vertex is invalid if it's either NaN of infinite. The invalid vertex
// is repaired using the center of mass of all the valid vertices in the
// correspoding cells.
class vtkBrokenCells : public vtkObject
{
public:
  static vtkBrokenCells *New();
  vtkTypeMacro(vtkBrokenCells, vtkBrokenCells);
  virtual void PrintSelf(ostream& os, vtkIndent indent) {};

  typedef std::multimap<vtkIdType, vtkCell*> BrokenCellsMap;
  typedef BrokenCellsMap::iterator BrokenCellsMapIterator;

  /// Simply calls the RepairCells on all the broken cells and
  /// returns the result.
  bool RepairAllCells();

  /// Repair the invalid vertex using the center of mass of all
  // the other valid vertices in the cells. This will modify
  /// the invalid points of Points.
  bool RepairCells(vtkIdType);

  /// Add the cell with the invalid point at the vertex index to the
  /// lists. If there is a list with cells that already have the invalid
  /// point, the cell will be added to that list. Otherwise a new list
  /// will be created
  void AddCell(vtkIdType vertexIndex, vtkCell* cell);

  /// Set the cells points. This will be used to lookup the cells' point
  /// values when repairing the cell.
  /// The invalid points of Points will be modified when RepairCells is called.
  void SetPoints(vtkPoints* points);
  vtkGetObjectMacro(Points, vtkPoints);

  /// Set the cells points. This will be used to lookup the cells' point
  /// values when repairing the cell.
  /// The invalid points of Points will be modified when RepairCells is called.
  vtkSetMacro(Verbose, bool);
  vtkGetMacro(Verbose, bool);

  /// Return whether the point is valid or not. A point is invalid if it's NaN
  /// or infinite
  static bool IsPointValid(double x, double y, double z);
  static bool IsPointValid(double p[3]);

protected:
  vtkBrokenCells();
  ~vtkBrokenCells();

  BrokenCellsMap Cells;
  vtkPoints* Points;
  bool Verbose;

  vtkBrokenCells(const vtkBrokenCells&) {}; // not implemented
  void operator=(const vtkBrokenCells&) {}; // not implemented
};

#endif
