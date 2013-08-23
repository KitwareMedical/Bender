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

// Bender includes
#include "ModelQuadricClusteringDecimationCLP.h"
#include "benderIOUtils.h"

// VTK includes
#include <vtkNew.h>
#include <vtkPolyData.h>
#include <vtkQuadricClustering.h>
#include <vtkSmartPointer.h>

// Slicer includes
#include <vtkPluginFilterWatcher.h>

// STD includes
#include <exception>

namespace
{

// Helper class: Checks that the given vector has 3 and only 3 values
// and that they are all strictly positive
template<class T> bool CheckVector(std::vector<T>& vec)
{
  bool isVectorValid = vec.size() == 3;
  if (isVectorValid)
    {
    for (size_t i = 0; i < vec.size(); ++i)
      {
      isVectorValid &= vec[i] > 0;
      }
    }
  return isVectorValid;
}

} // end namespace

int main( int argc, char * argv[] )
{
  PARSE_ARGS;

  vtkSmartPointer<vtkPolyData> model;
  model.TakeReference(bender::IOUtils::ReadPolyData(InputModel.c_str()));

  if (DebugMode)
    {
    std::cout << "Input model:\n"
              << "  Points: " << model->GetNumberOfPoints() << "\n"
              << "  Cells: " << model->GetNumberOfCells() << "\n"
              << std::endl;
    }

  if (UseFeaturePoints && !UseFeatureEdges)
    {
    std::cout<<"Warning: Use Feature Points is on but Use Feature Edges isn't."
      << " Use Feature Points is active only when Use Feature Edges is."
      <<std::endl;
    }

  vtkNew<vtkQuadricClustering> decimator;
  decimator->SetInput(model);

  decimator->SetUseInputPoints(UseInputPoints);
  decimator->SetUseFeatureEdges(UseFeatureEdges);
  decimator->SetUseFeaturePoints(UseFeaturePoints);
  decimator->AutoAdjustNumberOfDivisionsOff();

  if (UseNumberOfDivisions)
    {
    if (! CheckVector<int>(Divisions))
      {
      std::cerr<<"ERROR: Invalid number of divisions."<<std::endl;
      return EXIT_FAILURE;
      }

    decimator->SetNumberOfXDivisions(Divisions[0]);
    decimator->SetNumberOfYDivisions(Divisions[1]);
    decimator->SetNumberOfZDivisions(Divisions[2]);
    }
  else
    {
    if (! CheckVector<float>(Spacing))
      {
      std::cerr<<"ERROR: Invalid spacing."<<std::endl;
      return EXIT_FAILURE;
      }

    double bounds[6];
    model->GetBounds(bounds);

    double divisions[3];
    for (int i = 0; i < 3; ++i)
      {
      divisions[i] = ceil((bounds[2*i+1] - bounds[2*i]) / Spacing[i]);
      }

    decimator->SetNumberOfXDivisions(static_cast<int>(divisions[0]));
    decimator->SetNumberOfYDivisions(static_cast<int>(divisions[1]));
    decimator->SetNumberOfZDivisions(static_cast<int>(divisions[2]));
    }

  if (DebugMode)
    {
    decimator->Print(std::cout);
    decimator->DebugOn();
    }

  vtkPluginFilterWatcher watchDecimate(decimator.GetPointer(),
    "Reducing", CLPProcessInformation, 0.0, 1.0);
  try
    {
    decimator->Update();
    }
  catch (std::bad_alloc&)
    {
    std::cerr <<"Could not allocate memory for the given inputs. \n"
      << "-> Stopping."<<std::endl;
    return EXIT_FAILURE;
    }

  vtkSmartPointer<vtkPolyData> decimatedModel = decimator->GetOutput();

  if (DebugMode)
    {
    std::cout << "Decimated model:\n"
              << "  Points: " << decimatedModel->GetNumberOfPoints() << "\n"
              << "  Cells: " << decimatedModel->GetNumberOfCells() << "\n"
              << std::endl;
    }

  bender::IOUtils::WritePolyData(decimatedModel, DecimatedModel.c_str());

  return EXIT_SUCCESS;
}
