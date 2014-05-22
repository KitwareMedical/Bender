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
#include "VoxelizeTetrahedralMeshCLP.h"
#include "benderIOUtils.h"

// ITK includes
#include "itkBinaryThresholdImageFilter.h"
#include "itkCastImageFilter.h"
#include "itkConstantPadImageFilter.h"
#include "itkImageFileWriter.h"
#include "itkImageRegionConstIterator.h"
#include "itkImageRegionIterator.h"
#include "itkNearestNeighborInterpolateImageFunction.h"
#include "itkRelabelComponentImageFilter.h"

// Slicer includes
#include "itkPluginUtilities.h"

// VTK includes
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkCellLocator.h>
#include <vtkTetra.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkPolyData.h>
#include <vtkPolyDataWriter.h>
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>



// Use an anonymous namespace to keep class types and function names
// from colliding when module is used as shared object module.  Every
// thing should be in an anonymous namespace except for the module
// entry point, e.g. main()
//
namespace
{
template <class T>
int DoIt( int argc, char * argv[] );
}; // end of anonymous namespace

int main( int argc, char * argv[] )
{
  PARSE_ARGS;

  itk::ImageIOBase::IOPixelType     pixelType;
  itk::ImageIOBase::IOComponentType componentType;

  try
    {
    itk::GetImageType(InputRestVolume, pixelType, componentType);

    switch( componentType )
      {
    case itk::ImageIOBase::UCHAR:
      return DoIt<unsigned char>( argc, argv );
      break;
    case itk::ImageIOBase::CHAR:
      return DoIt<char>( argc, argv );
      break;
    case itk::ImageIOBase::USHORT:
      return DoIt<unsigned short>( argc, argv );
      break;
    case itk::ImageIOBase::SHORT:
      return DoIt<short>( argc, argv );
      break;
    case itk::ImageIOBase::UINT:
      return DoIt<unsigned int>( argc, argv );
      break;
    case itk::ImageIOBase::INT:
      return DoIt<int>( argc, argv );
      break;
    case itk::ImageIOBase::ULONG:
      return DoIt<unsigned long>( argc, argv );
      break;
    case itk::ImageIOBase::LONG:
      return DoIt<long>( argc, argv );
      break;
    case itk::ImageIOBase::FLOAT:
      return DoIt<float>( argc, argv );
      break;
    case itk::ImageIOBase::DOUBLE:
      return DoIt<double>( argc, argv );
      break;
    case itk::ImageIOBase::UNKNOWNCOMPONENTTYPE:
    default:
      std::cerr << "Unknown component type: " << componentType << std::endl;
      break;
      }
    }

  catch( itk::ExceptionObject & excep )
    {
    std::cerr << argv[0] << ": exception caught !" << std::endl;
    std::cerr << excep << std::endl;
    return EXIT_FAILURE;
    }
  return EXIT_SUCCESS;
}

// Use an anonymous namespace to keep class types and function names
// from colliding when module is used as shared object module.  Every
// thing should be in an anonymous namespace except for the module
// entry point, e.g. main()
//
namespace
{


//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
template <class T>
int DoIt( int argc, char * argv[] )
{
  PARSE_ARGS;

  //----------------------------
  // Read in the labelmap (rest)
  //----------------------------
  std::cout << "############# Read input rest labelmap..." << std::endl;
  typedef itk::Image<T, 3> LabelImageType;
  typedef itk::ImageFileReader<LabelImageType> LabelMapReaderType;
  typename LabelMapReaderType::Pointer labelMapReader = LabelMapReaderType::New();
  labelMapReader->SetFileName(InputRestVolume.c_str() );
  labelMapReader->Update();
  typename LabelImageType::Pointer restLabelMap = labelMapReader->GetOutput();
  if (!restLabelMap)
    {
    std::cerr << "Can't read labelmap " << InputRestVolume << std::endl;
    return EXIT_FAILURE;
    }
  std::cout << "############# done." << std::endl;

  if (Verbose)
    {
    std::cout << "Input Labelmap: \n"
              << " Origin: " << restLabelMap->GetOrigin() << "\n"
              << " Spacing: " << restLabelMap->GetSpacing() << "\n"
              << " Direction: " << restLabelMap->GetDirection() << "\n"
              << " " << restLabelMap->GetLargestPossibleRegion()
              << std::endl;
    }
  //----------------------------
  // Read tet mesh (rest+posed)
  //----------------------------
  vtkSmartPointer<vtkPolyData> restTetMeshPolyData;
  restTetMeshPolyData.TakeReference(
    bender::IOUtils::ReadPolyData(InputRestMesh.c_str(), !IsMeshInRAS));
  vtkSmartPointer<vtkUnstructuredGrid> restTetMesh;
  restTetMesh.TakeReference(
    bender::IOUtils::PolyDataToUnstructuredGrid(restTetMeshPolyData));

  vtkSmartPointer<vtkPolyData> posedTetMeshPolyData;
  posedTetMeshPolyData.TakeReference(
    bender::IOUtils::ReadPolyData(InputPosedMesh.c_str(), !IsMeshInRAS));
  vtkSmartPointer<vtkUnstructuredGrid> posedTetMesh;
  posedTetMesh.TakeReference(
    bender::IOUtils::PolyDataToUnstructuredGrid(posedTetMeshPolyData));

  if ((restTetMesh->GetNumberOfPoints() != posedTetMesh->GetNumberOfPoints()) ||
      (restTetMesh->GetNumberOfCells() != posedTetMesh->GetNumberOfCells()))
    {
    std::cerr << "The rest and posed mesh do not have "
              << "the same number of points or cells. "
              << "The rest mesh has " << restTetMesh->GetNumberOfPoints()
              << " points and " << restTetMesh->GetNumberOfCells() << "cells "
              << " but the posed mesh has " << posedTetMesh->GetNumberOfPoints()
              << " points and " << posedTetMesh->GetNumberOfCells()
              << std::endl;
    return EXIT_FAILURE;
    }

  //----------------------------
  // Output labelmap
  //----------------------------
  typename LabelImageType::Pointer posedLabelMap = LabelImageType::New();
  posedLabelMap->CopyInformation(restLabelMap);
  std::cout << "Padding: " << Padding << std::endl;
  double posedTetMeshBounds[6] = {0., -1., 0., -1., 0., -1.};
  posedTetMesh->GetBounds(posedTetMeshBounds);
  /*
  if (!IsMeshInRAS)
    {
    posedTetMeshBounds[0] *= -1;
    posedTetMeshBounds[1] *= -1;
    posedTetMeshBounds[2] *= -1;
    posedTetMeshBounds[3] *= -1;
    std::swap(posedTetMeshBounds[0], posedTetMeshBounds[1]);
    std::swap(posedTetMeshBounds[2], posedTetMeshBounds[3]);
    }
  */
  std::cout << "Posed TetMesh bounds: "
            << posedTetMeshBounds[0] << "," << posedTetMeshBounds[1] << ","
            << posedTetMeshBounds[2] << "," << posedTetMeshBounds[3] << ","
            << posedTetMeshBounds[4] << "," << posedTetMeshBounds[5] << std::endl;
  double posedLabelmapBounds[6] = {0., -1., 0., -1., 0., -1.};
  double bounds[6] = {0., -1., 0., -1., 0., -1.};
  for (int i = 0; i < 3; ++i)
    {
    posedLabelmapBounds[i*2] = posedTetMeshBounds[i*2] - Padding;
    posedLabelmapBounds[i*2 + 1] = posedTetMeshBounds[i*2 + 1] + Padding;
    bounds[i*2] = posedLabelmapBounds[i*2];
    bounds[i*2 + 1] = posedLabelmapBounds[i*2 + 1];
    }
  typename LabelImageType::PointType origin;
  origin[0] = posedLabelMap->GetDirection()[0][0] >= 0. ? bounds[0] : bounds[1];
  origin[1] = posedLabelMap->GetDirection()[1][1] >= 0. ? bounds[2] : bounds[3];
  origin[2] = posedLabelMap->GetDirection()[2][2] >= 0. ? bounds[4] : bounds[5];
  posedLabelMap->SetOrigin(origin);
  typename LabelImageType::RegionType region;
  assert(bounds[1] >= bounds[0] && bounds[3] >= bounds[2] && bounds[5] >= bounds[4]);
  region.SetSize(0, (bounds[1]-bounds[0]) / posedLabelMap->GetSpacing()[0]);
  region.SetSize(1, (bounds[3]-bounds[2]) / posedLabelMap->GetSpacing()[1]);
  region.SetSize(2, (bounds[5]-bounds[4]) / posedLabelMap->GetSpacing()[2]);
  posedLabelMap->SetRegions(region);
  std::cout << "Allocate output posed labelmap: \n"
            << " Origin: " << posedLabelMap->GetOrigin() << "\n"
            << " Spacing: " << posedLabelMap->GetSpacing() << "\n"
            << " Direction: " << posedLabelMap->GetDirection()
            << " " << posedLabelMap->GetLargestPossibleRegion()
            << std::endl;
  posedLabelMap->Allocate();
  T BackgroundValue = 0;// Background by default.
  posedLabelMap->FillBuffer(BackgroundValue);

  //----------------------------
  // Voxelize
  //----------------------------

  std::cout << "############# Voxelize..." << std::endl;
  //vtkNew<vtkCellLocator> restCellLocator;
  //restCellLocator->SetDataSet(restTetMesh);
  //restCellLocator->BuildLocator();
  vtkNew<vtkCellLocator> posedCellLocator;
  posedCellLocator->SetDataSet(posedTetMesh);
  posedCellLocator->BuildLocator();

  itk::ImageRegionIteratorWithIndex<LabelImageType> imageIt(
    posedLabelMap, posedLabelMap->GetLargestPossibleRegion());

  typedef itk::NearestNeighborInterpolateImageFunction<LabelImageType> InterpolatorType;
  typename InterpolatorType::Pointer interpolator = InterpolatorType::New();
  interpolator->SetInputImage(restLabelMap);

  size_t voxelIt(0);
  size_t assignedPixelCount(0);
  size_t skippedPixelCount(0);
  const size_t voxelCount =
    posedLabelMap->GetLargestPossibleRegion().GetSize(0) *
    posedLabelMap->GetLargestPossibleRegion().GetSize(1) *
    posedLabelMap->GetLargestPossibleRegion().GetSize(2);
  const size_t progress((voxelCount-1) / 100);
  for (imageIt.GoToBegin(); !imageIt.IsAtEnd() ; ++imageIt)
    {
    if (voxelIt++ % progress == 0)
      {
      std::cout << "+";
      std::cout.flush();
      static int i  = 0;
      if (i++ == 10)
        {
        //break;
        }
      }
    //const itk::ContinuousIndex<double,3>& posedIndex = imageIt.GetIndex();
    const typename LabelImageType::IndexType& posedIndex = imageIt.GetIndex();
    typename LabelImageType::PointType posedPoint;
    posedLabelMap->TransformIndexToPhysicalPoint(posedIndex, posedPoint);

    double posedVoxelPosition[3];
    posedVoxelPosition[0] = posedPoint[0];
    posedVoxelPosition[1] = posedPoint[1];
    posedVoxelPosition[2] = posedPoint[2];

    double closestPoint[3] = {0., 0., 0.};
    vtkIdType closestCell = -1;
    int subId = 0;
    double distance = 0.;
    posedCellLocator->FindClosestPoint(
      posedVoxelPosition, closestPoint, closestCell, subId, distance);
    assert(closestCell != static_cast<vtkIdType>(-1));

    vtkCell* posedCell = posedTetMesh->GetCell(closestCell);
    assert(posedCell);
    assert(posedCell->GetPoints()->GetNumberOfPoints() == 4);
    double pcoords[3];
    double weights[4];
    int insideOutside = posedCell->EvaluatePosition(
      posedVoxelPosition, closestPoint, subId, pcoords, distance, weights);
    //if (insideOutside < 0 || (insideOutside == 0 && distance > 15.))
    if (insideOutside <= 0)
      {
      ++skippedPixelCount;
      continue;
      }
    Verbose = (closestCell == 5387);
    if (Verbose)
      {
      std::cout << "closestCell: " << closestCell << ", " << subId << ", " << distance << std::endl;
      }
    double weightedPoint[3] = {0.,0.,0.};
    for (int i = 0; i < 4; ++i)
      {
      double* posedPoint = posedCell->GetPoints()->GetPoint(i);
      weightedPoint[0] += weights[i] * posedPoint[0];
      weightedPoint[1] += weights[i] * posedPoint[1];
      weightedPoint[2] += weights[i] * posedPoint[2];
      if (Verbose)
        {
        std::cout << "p p" << i <<": "
                  << posedPoint[0] << ", "
                  << posedPoint[1] << ", "
                  << posedPoint[2] << std::endl;
        std::cout << "p p" << i << ": "
                  << posedTetMesh->GetPoints()->GetPoint(posedCell->GetPointId(i))[0] << ", "
                  << posedTetMesh->GetPoints()->GetPoint(posedCell->GetPointId(i))[1] << ", "
                  << posedTetMesh->GetPoints()->GetPoint(posedCell->GetPointId(i))[2] << std::endl;

        }
      }
    double diff2 = 0.;
    diff2 += (weightedPoint[0] - posedVoxelPosition[0]) * (weightedPoint[0] - posedVoxelPosition[0]);
    diff2 += (weightedPoint[1] - posedVoxelPosition[1]) * (weightedPoint[1] - posedVoxelPosition[1]);
    diff2 += (weightedPoint[2] - posedVoxelPosition[2]) * (weightedPoint[2] - posedVoxelPosition[2]);
    if (Verbose && diff2 >= 0.001)
      {
      std::cerr << "Problem: " << diff2 << " " << insideOutside << std::endl;
      std::cerr << "  pos: " << posedVoxelPosition[0] << ", " << posedVoxelPosition[1] << ", " << posedVoxelPosition[2] << std::endl;
      std::cerr << "  closestpos: " << closestPoint[0] << ", " << closestPoint[1] << ", " << closestPoint[2] << std::endl;
      std::cerr << "  weightedPoint: " << weightedPoint[0] << ", " << weightedPoint[1] << ", " << weightedPoint[2] << std::endl;
      }

    vtkCell* restCell = restTetMesh->GetCell(closestCell);
    assert(restCell);
    double restVoxelPosition[3] = {0.,0.,0.};
    for (int i = 0; i < 4; ++i)
      {
      double* restPoint = restCell->GetPoints()->GetPoint(i);
      restVoxelPosition[0] += weights[i] * restPoint[0];
      restVoxelPosition[1] += weights[i] * restPoint[1];
      restVoxelPosition[2] += weights[i] * restPoint[2];
      if (Verbose)
        {
        std::cout << "r p" << i << ": "
                  << restPoint[0] << ", "
                  << restPoint[1] << ", "
                  << restPoint[2] << std::endl;
        std::cout << "r p" << i << ": "
                  << restTetMesh->GetPoints()->GetPoint(restCell->GetPointId(i))[0] << ", "
                  << restTetMesh->GetPoints()->GetPoint(restCell->GetPointId(i))[1] << ", "
                  << restTetMesh->GetPoints()->GetPoint(restCell->GetPointId(i))[2] << std::endl;
        }
      }
    if (Verbose)
      {
      std::cerr << " Start(" << posedVoxelPosition[0] << ", " << posedVoxelPosition[1] << ", " << posedVoxelPosition[2] << ")";
      std::cerr << " End(" << restVoxelPosition[0] << ", " << restVoxelPosition[1] << ", " << restVoxelPosition[2] << ")";
      std::cerr << std::endl;
      }

    typename LabelImageType::PointType restPoint;
    restPoint[0] = restVoxelPosition[0];
    restPoint[1] = restVoxelPosition[1];
    restPoint[2] = restVoxelPosition[2];
    //restPoint[0] = posedVoxelPosition[0];
    //restPoint[1] = posedVoxelPosition[1];
    //restPoint[2] = posedVoxelPosition[2];

    //itk::ContinuousIndex<double,3> restIndex;
    //restLabelMap->TransformPhysicalPointToContinuousIndex(restPoint, restIndex);
    typename LabelImageType::IndexType restIndex;
    bool inside = restLabelMap->TransformPhysicalPointToIndex(restPoint, restIndex);
    if (Verbose)
      {
      std::cout << "Posed index: " << posedIndex << std::endl;
      std::cout << "Rest index: " << restIndex << std::endl;
      }

    T value = BackgroundValue;
    if (inside)
      {
      value = restLabelMap->GetPixel(restIndex);
      }
    //if (restLabelMap->GetLargestPossibleRegion().IsInside(restIndex))
    //  {
    //  value = interpolator->Evaluate(restIndex);
    //  }
    imageIt.Set(value);
    ++assignedPixelCount;
    }

  std::cout << std::endl;
  std::cout << assignedPixelCount << " pixels assigned" << std::endl;
  std::cout << skippedPixelCount << " voxels skipped" << std::endl;
  std::cout << "############# done." << std::endl;

  //----------------------------
  // Write output
  //----------------------------
  bender::IOUtils::WriteImage<LabelImageType>(
    posedLabelMap, OutputPosedVolume.c_str());

  return EXIT_SUCCESS;
}

} // end of anonymous namespace

