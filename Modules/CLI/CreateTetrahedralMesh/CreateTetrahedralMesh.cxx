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
#include "CreateTetrahedralMeshCLP.h"
#include "benderIOUtils.h"
#include "vtkBrokenCells.h"

// ITK includes
#include "itkBinaryThresholdImageFilter.h"
#include "itkCastImageFilter.h"
#include "itkImageFileWriter.h"
#include "itkImageRegionConstIterator.h"
#include "itkImageRegionIterator.h"
#include "itkRelabelComponentImageFilter.h"
#include "itkConstantPadImageFilter.h"

// Slicer includes
#include "itkPluginUtilities.h"

// VTK includes
#include <vtkCleanPolyData.h>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkTetra.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkPolyData.h>
#include <vtkPolyDataWriter.h>
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkSmartPointer.h>

// Cleaver includes
#include <Cleaver/Cleaver.h>
#include <Cleaver/InverseField.h>
#include <Cleaver/PaddedVolume.h>
#include <Cleaver/ScalarField.h>
#include <Cleaver/Volume.h>
#include <LabelMapField.h>


// Use an anonymous namespace to keep class types and function names
// from colliding when module is used as shared object module.  Every
// thing should be in an anonymous namespace except for the module
// entry point, e.g. main()
//
namespace
{
template <class T> int DoIt( int argc, char * argv[] );

std::vector<Cleaver::LabelMapField::ImageType::Pointer> SplitLabelMaps(
  int    argc,
  char * argv[]);
} // end of anonymous namespace

int main( int argc, char * argv[] )
{

  PARSE_ARGS;

  itk::ImageIOBase::IOPixelType     pixelType;
  itk::ImageIOBase::IOComponentType componentType;

  try
    {
    itk::GetImageType(InputVolume, pixelType, componentType);

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
std::vector<Cleaver::LabelMapField::ImageType::Pointer >
SplitLabelMaps(Cleaver::LabelMapField::ImageType *image, bool verbose)
{

  typedef Cleaver::LabelMapField::ImageType LabelImageType;
  typedef itk::RelabelComponentImageFilter<LabelImageType,
                                           LabelImageType> RelabelFilterType;
  typedef itk::BinaryThresholdImageFilter<LabelImageType,
                                          LabelImageType> ThresholdFilterType;

  // Assign continuous labels to the connected components, background is
  // considered to be 0 and will be ignored in the relabeling process.
  RelabelFilterType::Pointer relabelFilter = RelabelFilterType::New();
  relabelFilter->SetInput( image );
  relabelFilter->Update();

  if (verbose)
    {
    std::cout << "Total Number of Labels: " <<
      relabelFilter->GetNumberOfObjects() << std::endl;
    }

  // Extract the labels
  typedef RelabelFilterType::LabelType LabelType;
  ThresholdFilterType::Pointer skinThresholdFilter =
    ThresholdFilterType::New();

  // Create a list of images corresponding to labels
  std::vector<LabelImageType::Pointer> labels;

  // The skin label will become background for internal (smaller) organs
  skinThresholdFilter->SetInput(relabelFilter->GetOutput());
  skinThresholdFilter->SetLowerThreshold(1);
  skinThresholdFilter->SetUpperThreshold(relabelFilter->GetNumberOfObjects()+1);
  skinThresholdFilter->SetInsideValue(-1);
  skinThresholdFilter->SetOutsideValue(0);
  skinThresholdFilter->Update();
  labels.push_back(skinThresholdFilter->GetOutput());

  for (LabelType i = 1, end = relabelFilter->GetNumberOfObjects()+1; i < end;
       ++i)
    {
    ThresholdFilterType::Pointer organThresholdFilter =
      ThresholdFilterType::New();
    organThresholdFilter->SetInput(relabelFilter->GetOutput());
    organThresholdFilter->SetLowerThreshold(i);
    organThresholdFilter->SetUpperThreshold(i);
    organThresholdFilter->SetInsideValue(i);
    organThresholdFilter->SetOutsideValue(-1);
    organThresholdFilter->Update();

    labels.push_back(organThresholdFilter->GetOutput());
    }

  return labels;
}

//----------------------------------------------------------------------------
bool IsPointValid(Cleaver::vec3 pos)
{
  return vtkBrokenCells::IsPointValid(pos.x, pos.y, pos.z);
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
template <class T>
int DoIt( int argc, char * argv[] )
{

  PARSE_ARGS;

  typedef Cleaver::LabelMapField::ImageType LabelImageType;
  typedef T InputPixelType;
  typedef itk::Image<InputPixelType,3> InputImageType;
  typedef itk::CastImageFilter<InputImageType,
                               LabelImageType> CastFilterType;
  typedef itk::ImageFileReader<InputImageType> ReaderType;
  typedef itk::ConstantPadImageFilter<InputImageType,
                                      InputImageType> ConstantPadType;

  typename ReaderType::Pointer reader            = ReaderType::New();
  reader->SetFileName( InputVolume );
  reader->Update();
  LabelImageType::SpacingType spacing = reader->GetOutput()->GetSpacing();
  LabelImageType::PointType origin = reader->GetOutput()->GetOrigin();
  LabelImageType::DirectionType imageDirection =
    reader->GetOutput()->GetDirection();

  typename CastFilterType::Pointer castingFilter = CastFilterType::New();
  castingFilter->SetInput(reader->GetOutput());

  std::vector<Cleaver::ScalarField*> labelMaps;

  std::vector<LabelImageType::Pointer> labels =
    SplitLabelMaps(castingFilter->GetOutput(), Verbose);

  // Get a map from the original labels to the new labels
  std::map<InputPixelType, InputPixelType> originalLabels;

  for(size_t i = 0; i < labels.size(); ++i)
    {
    itk::ImageRegionConstIterator<InputImageType> imageIterator(
      reader->GetOutput(), reader->GetOutput()->GetLargestPossibleRegion());
    itk::ImageRegionConstIterator<LabelImageType> labelsIterator(
      labels[i], labels[i]->GetLargestPossibleRegion());
    bool foundCorrespondence = false;
    while(!imageIterator.IsAtEnd()
      && !labelsIterator.IsAtEnd()
      && !foundCorrespondence)
      {
      if (labelsIterator.Value() > 0)
        {
        originalLabels[labelsIterator.Value()] = imageIterator.Value();
        }

      ++imageIterator;
      ++labelsIterator;
      }
    if (SaveLabelImages)
      {
      std::stringstream fileName;
      fileName << "label" << i << ".nrrd";
      bender::IOUtils::WriteDebugImage<LabelImageType>(labels[i], fileName.str());
      }
    }

  if (Verbose)
    {
    std::cout << "Total labels found:  " << labels.size() << std::endl;
    }
  for(size_t i = 0; i < labels.size(); ++i)
    {
    labelMaps.push_back(new Cleaver::LabelMapField(labels[i]));
    }

  if(labelMaps.empty())
    {
    std::cerr << "Failed to load image data. Terminating." << std::endl;
    return 0;
    }
  if(labelMaps.size() < 2)
    {
    labelMaps.push_back(new Cleaver::InverseField(labelMaps[0]));
    }

  Cleaver::AbstractVolume *cleaverVolume = new Cleaver::Volume(labelMaps);
  if (Padding)
    {
    cleaverVolume = new Cleaver::PaddedVolume(cleaverVolume);
    }

  if (Verbose)
    {
    std::cout << "Creating Mesh with Volume Size "
      << cleaverVolume->size().toString() << std::endl;
    }

  //--------------------------------
  //  Create Mesher & TetMesh
  //--------------------------------
  Cleaver::TetMesh *cleaverMesh =
    Cleaver::createMeshFromVolume(cleaverVolume, Verbose);

  // No need for the volume nor the labelmaps anymore therefore we can release
  // the memory.
  delete cleaverVolume;
  for(size_t i=0; i < labelMaps.size(); ++i)
    delete labelMaps[i];
  labelMaps.clear();
  labels.clear();
  castingFilter = NULL;
  reader = NULL;

  if (!cleaverMesh)
    {
    std::cerr << "Mesh computation failed !" << std::endl;
    return EXIT_FAILURE;
    }

  //------------------
  //  Compute Angles
  //------------------
  if(Verbose)
    {
    cleaverMesh->computeAngles();
    std::cout.precision(12);
    std::cout << "Worst Angles:" << std::endl;
    std::cout << "min: " << cleaverMesh->min_angle << std::endl;
    std::cout << "max: " << cleaverMesh->max_angle << std::endl;
    }

  //-----------------------
  //  Fill polydata arrays
  //-----------------------
  // Constants for undesired material
  const int airLabel = 0;
  int paddedVolumeLabel = labels.size();

  // Points and cell arrays
  vtkNew<vtkPoints> points;
  points->SetNumberOfPoints(cleaverMesh->tets.size() * 4);

  vtkNew<vtkCellArray> meshTetras;
  meshTetras->SetNumberOfCells(cleaverMesh->tets.size());

  vtkNew<vtkIntArray> cellData;
  cellData->SetName("MaterialId");
  cellData->SetNumberOfTuples(cleaverMesh->tets.size());

  vtkSmartPointer<vtkBrokenCells> brokenCells =
    vtkSmartPointer<vtkBrokenCells>::New();
  brokenCells->SetPoints(points.GetPointer());
  brokenCells->SetVerbose(Verbose);
  for(size_t i = 0; i < cleaverMesh->tets.size(); ++i)
    {
    int label = cleaverMesh->tets[i]->mat_label;

    if(label == airLabel || label == paddedVolumeLabel)
      {
      continue;
      }

    vtkNew<vtkTetra> meshTetra;
    for (size_t j = 0; j < 4; ++j)
      {
      Cleaver::vec3 &pos = cleaverMesh->tets[i]->verts[j]->pos();
      size_t vertexIndex = cleaverMesh->tets[i]->verts[j]->tm_v_index;

      points->SetPoint(vertexIndex, pos.x, pos.y, pos.z);
      meshTetra->GetPointIds()->SetId(j, vertexIndex);

      // If invalid, flag the cell so it can be rebuild later
      if (! IsPointValid(pos))
        {
        std::cerr << "Invalid point (" << pos.x << ", " << pos.y << ", " << pos.z
                  << ") at cell " << i
          << ", this point will be patched up but something went wrong with"
          << " Cleaver !" << std::endl;
        brokenCells->AddCell(vertexIndex, meshTetra.GetPointer());
        }
      }

    meshTetras->InsertNextCell(meshTetra.GetPointer());
    cellData->InsertNextValue(originalLabels[label]);
    }

  // No need for the mesh anymore, release the memory.
  delete cleaverMesh;

  std::cerr << "There are " << brokenCells->GetNumberOfBrokenCells()
            << " broken cells for " << meshTetras->GetNumberOfCells() << " cells."
            << std::endl;
  //  Repair broken cells
  if (!brokenCells->RepairAllCells())
    {
    std::cerr << "Fail to fix the broken cells." << std::endl;
    return EXIT_FAILURE;
    }
  // No need for the cell fixer anymore, release the memory.
  brokenCells = NULL;

  //-----------------------------
  //  Create and clean polydata
  //-----------------------------

  vtkSmartPointer<vtkPolyData> vtkMesh = vtkSmartPointer<vtkPolyData>::New();
  vtkMesh->SetPoints(points.GetPointer());
  vtkMesh->SetPolys(meshTetras.GetPointer());
  vtkMesh->GetCellData()->SetScalars(cellData.GetPointer());

  if (Verbose)
    {
    std::cout << "Clean PolyData..." << std::endl;
    }
  vtkNew<vtkCleanPolyData> cleanFilter;
  cleanFilter->PointMergingOff(); // Prevent from creating triangles or lines
  cleanFilter->SetInput(vtkMesh);

  //---------------------------------------
  //  Transform polydata to fit the image
  //---------------------------------------
  // Since cleaver does not take into account the image properties such as
  // spacing or origin, we need to transform the output points so the mesh can
  // match the original image.
  vtkNew<vtkTransform> transform;

  // Transform points to RAS (what is concatenated first is done last !)
  vtkNew<vtkMatrix4x4> rasMatrix;
  rasMatrix->Identity();
  rasMatrix->SetElement(0, 0, -1.0);
  rasMatrix->SetElement(1, 1, -1.0);
  transform->Concatenate(rasMatrix.GetPointer());

  // Translation
  vtkNew<vtkMatrix4x4> directionMatrix;
  directionMatrix->Identity();
  for (int i = 0; i < imageDirection.RowDimensions; ++i)
    {
    for (int j = 0; j < imageDirection.ColumnDimensions; ++j)
      {
      directionMatrix->SetElement(i, j, imageDirection[i][j]);
      }
    }

  vtkNew<vtkTransform> offsetTransform;
  offsetTransform->Concatenate(directionMatrix.GetPointer());
  double offsets[3];
  if (Padding)
    {
    for (int i = 0; i < 3; ++i)
      {
      offsets[i] =
        spacing[i]*Cleaver::PaddedVolume::DefaultThickness + spacing[i]/2.0;
      }
    }
  else
    {
    for (int i = 0; i < 3; ++i)
      {
      offsets[i] = spacing[i] / 2.0;
      }
    }
  double* transformedOffsets = offsetTransform->TransformDoubleVector(offsets);
  transform->Translate(
    origin[0] - transformedOffsets[0],
    origin[1] - transformedOffsets[1],
    origin[2] - transformedOffsets[2]);

  // Scaling and rotation
  vtkNew<vtkMatrix4x4> scaleMatrix;
  scaleMatrix->DeepCopy(directionMatrix.GetPointer());
  for (size_t i = 0; i < spacing.GetNumberOfComponents(); ++i)
    {
    scaleMatrix->SetElement(i, i, scaleMatrix->GetElement(i, i) * spacing[i]);
    }
  transform->Concatenate(scaleMatrix.GetPointer());

  if (Verbose)
    {
    transform->Print(std::cout);
    }
  // Actual transformation
  vtkNew<vtkTransformPolyDataFilter> transformFilter;
  transformFilter->SetInput(cleanFilter->GetOutput());
  transformFilter->SetTransform(transform.GetPointer());

  // Conserve memory
  transformFilter->GetOutput()->GlobalReleaseDataFlagOn();
  bender::IOUtils::WritePolyData(transformFilter->GetOutput(), OutputMesh);

  return EXIT_SUCCESS;
}



} // end of anonymous namespace

