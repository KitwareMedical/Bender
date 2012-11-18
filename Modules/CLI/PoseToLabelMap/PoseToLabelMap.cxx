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

// ModelToLabelMap includes
#include "PoseToLabelMapCLP.h"
#include "vtkPolyDataPointIDSampler.h"

// Armatures includes
#include "vtkSlicerArmaturesLogic.h"
#include "vtkSlicerModelsLogic.h"

// MRML includes
#include <vtkMRMLModelNode.h>

// ITK includes
#ifdef ITKV3_COMPATIBILITY
#include <itkAnalyzeImageIOFactory.h>
#endif
#include "itkImage.h"
#include <itkDanielssonDistanceMapImageFilter.h>
#include <itkImageFileWriter.h>
#include <itkPluginUtilities.h>

// VTK includes
#include <vtkCell.h>
#include <vtkCellData.h>
#include <vtkDebugLeaks.h>
#include <vtkDoubleArray.h>
#include <vtkIdTypeArray.h>
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPolyDataReader.h>
#include <vtkPolyDataWriter.h>
#include <vtkSmartPointer.h>
#include <vtkSTLReader.h>
#include <vtkXMLPolyDataReader.h>

//-----------------------------------------------------------------------------
template<class T>
struct InputImage{
  typedef itk::Image<T, 3> Type;
};

template <class T>
int DoIt( int argc, char * argv[] );

typedef itk::Image<unsigned int, 3> VoxelizedModelImageType;
typedef itk::Image<unsigned long, 3> DistanceMapImageType;

template <class T>
VoxelizedModelImageType::Pointer VoxelizeModel(vtkPolyData* model,
                                               typename InputImage<T>::Type::Pointer inputImage,
                                               double samplingDistance);

vtkPolyData* ReadPolyData(const std::string& fileName);
void WritePolyData(vtkPolyData* polyData, const std::string& fileName);

template<class T>
void WriteImage(typename InputImage<T>::Type::Pointer image,
                const std::string& fileName,
                ModuleProcessInformation* processInformation);

void GetCellCenter(vtkPolyData* poly, vtkIdType cellId, double center[3]);
void GetPoint(vtkPolyData* polyData, vtkIdType cellId, vtkIdType pointIndex, double point[3], const char* pointArray = 0);
void GetMatrix(vtkPolyData* armature, vtkIdType boneId, double matrix[3][3], const char* pointArray = 0);
vtkIdType GetClosestBone(vtkPolyData* armature, vtkPolyData* posedModel, vtkIdType cellId);

template <class T>
void PoseLabelmap(VoxelizedModelImageType::Pointer posedLabelmap,
                  typename InputImage<T>::Type::Pointer restLabelmap,
                  DistanceMapImageType::Pointer posedDistancemap,
                  vtkPolyData* armature,
                  vtkPolyData* restModel, vtkPolyData* posedModel);
double GetDistance2ToFace(vtkPolyData* polyData, vtkIdType cellId, double point[3], double* projection);
void InterpolateQuaternion(double qa[4], double qb[4], double t, double qm[4]);

//-----------------------------------------------------------------------------
int main( int argc, char * argv[] )
{
  PARSE_ARGS;

  itk::ImageIOBase::IOPixelType     pixelType;
  itk::ImageIOBase::IOComponentType componentType;
#ifdef ITKV3_COMPATIBILITY
  itk::ObjectFactoryBase::RegisterFactory( itk::AnalyzeImageIOFactory::New() );
#endif
  try
    {
    itk::GetImageType(RestLabelmap, pixelType, componentType);

    // This filter handles all types on input, but only produces
    // signed types

    switch( componentType )
      {
      case itk::ImageIOBase::UCHAR:
      case itk::ImageIOBase::CHAR:
        return DoIt<char>( argc, argv );
        break;
      case itk::ImageIOBase::USHORT:
      case itk::ImageIOBase::SHORT:
        return DoIt<short>( argc, argv );
        break;
      case itk::ImageIOBase::UINT:
      case itk::ImageIOBase::INT:
        return DoIt<int>( argc, argv );
        break;
      case itk::ImageIOBase::ULONG:
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
        std::cout << "unknown component type" << std::endl;
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

//-----------------------------------------------------------------------------
template <class T>
int DoIt( int argc, char * argv[] )
{
  PARSE_ARGS;
  vtkDebugLeaks::SetExitError(true);

  typedef T InputPixelType;

  typedef itk::ImageFileReader<typename InputImage<T>::Type> ReaderType;
  typedef itk::ImageFileWriter<VoxelizedModelImageType> WriterType;

  std::cout << std::endl << "----- Read Rest Labelmap -----" << std::endl;
  // Read the input volume
  typename ReaderType::Pointer restLabelmapReader = ReaderType::New();
  itk::PluginFilterWatcher watchReader(restLabelmapReader, "Read Rest Labelmap",
                                       CLPProcessInformation);
  restLabelmapReader->SetFileName( RestLabelmap.c_str() );
  restLabelmapReader->Update();
  InputImage<T>::Type::Pointer restLabelmap =
    restLabelmapReader->GetOutput();

  std::cout << std::endl << "----- Read Models -----" << std::endl;
  // Resting model
  vtkPolyData* restPolyData = ReadPolyData(RestModel);
  if (restPolyData == 0)
    {
    return EXIT_FAILURE;
    }
  vtkSmartPointer<vtkPolyData> restModel;
  restModel.TakeReference(restPolyData);

  // Posed model
  vtkPolyData* posedPolyData = ReadPolyData(PosedModel);
  if (posedPolyData == 0)
    {
    return EXIT_FAILURE;
    }
  vtkSmartPointer<vtkPolyData> posedModel;
  posedModel.TakeReference(posedPolyData);

  bool useCache = debug;
  //bool useCache = false;

  std::cout << std::endl << "----- Read Armature -----" << std::endl;
  // Read armature
  vtkNew<vtkSlicerArmaturesLogic> armaturesLogic;
  vtkNew<vtkSlicerModelsLogic> modelsLogic;
  //armaturesLogic->SetLPS2RAS(-1.);
  armaturesLogic->SetModelsLogic(modelsLogic.GetPointer());
  vtkNew<vtkMRMLScene> dummyScene;
  modelsLogic->SetMRMLScene(dummyScene.GetPointer());
  armaturesLogic->SetMRMLScene(dummyScene.GetPointer());
  vtkMRMLModelNode* armatureNode =
    armaturesLogic->AddArmatureFile(Armature.c_str());
  if (!armatureNode)
    {
    std::cout << "Usage: invalid armature file: " << Armature << std::endl;
    return EXIT_FAILURE;
    }
  vtkSmartPointer<vtkPolyData> armature = armatureNode->GetPolyData();
  WritePolyData(armature, "e:\\armature.vtk");

  std::cout << std::endl << "----- Voxelized Map -----" << std::endl;
  // Output label map
  // Generate a labelmap from a posed model by using the metadata of the rest
  // labelmap. Intensities are the cell IDs.
  // \todo Support when the posed model is OUTSIDE the rest labelmap bounds.
  VoxelizedModelImageType::Pointer posedLabelmap;
  if (!useCache)
    {
    posedLabelmap = VoxelizeModel<T>(posedModel, restLabelmap, samplingDistance);
    WriteImage<VoxelizedModelImageType::ValueType>(
      posedLabelmap, "e:\\voxelizedMap.mha", CLPProcessInformation);
    }
  else
    {
    typedef itk::ImageFileReader<VoxelizedModelImageType> PosedReaderType;
    typename PosedReaderType::Pointer posedLabelmapReader = PosedReaderType::New();
    posedLabelmapReader->SetFileName( "e:\\voxelizedMap.mha" );
    posedLabelmapReader->Update();
    posedLabelmap = posedLabelmapReader->GetOutput();
    }

  // Compute the distance map of the skin.
  std::cout << std::endl << "----- Distance Map -----" << std::endl;
  DistanceMapImageType::Pointer posedDistanceMap = 0;
  if (!useCache)
    {
    typedef itk::DanielssonDistanceMapImageFilter<VoxelizedModelImageType, DistanceMapImageType > DistanceMapFilterType;
    DistanceMapFilterType::Pointer distanceMapFilter = DistanceMapFilterType::New();
    distanceMapFilter->SetInput( posedLabelmap );
    distanceMapFilter->Update();
    //posedDistanceMap = distanceMapFilter->GetOutput();
    posedDistanceMap = distanceMapFilter->GetVoronoiMap();

    WriteImage<DistanceMapImageType::ValueType>(posedDistanceMap,
      "e:\\voronoiMap.mha",
      CLPProcessInformation);
    WriteImage<DistanceMapImageType::ValueType>(distanceMapFilter->GetDistanceMap(),
      "e:\\distanceMap.mha",
      CLPProcessInformation);
    }
  else
    {
    typedef itk::ImageFileReader<DistanceMapImageType> DistanceMapReaderType;
    typename DistanceMapReaderType::Pointer distanceMapReader = DistanceMapReaderType::New();
    distanceMapReader->SetFileName( "e:\\voronoiMap.mha" );
    distanceMapReader->Update();
    posedDistanceMap = distanceMapReader->GetOutput();
    }

  // Compute local transform between rest and pose at each cell.
  std::cout << std::endl << "----- ComputeLocalTransform -----" << std::endl;
  if (!useCache)
    {
    ComputeLocalTransform(restModel, posedModel, armature);
    WritePolyData(posedModel, "e:\\posedModelWithTransform.vtk");
    }
  else
    {
    posedModel = ReadPolyData("e:\\posedModelWithTransform.vtk");
    }

  std::cout << std::endl << "----- PoseLabelMap -----" << std::endl;
  PoseLabelmap<T>(posedLabelmap, restLabelmap, posedDistanceMap, armature, restModel, posedModel);

  std::cout << std::endl << "----- Write to disk -----" << std::endl;
  // Final write
  WriteImage<VoxelizedModelImageType::ValueType>(posedLabelmap,
    PosedLabelmap.c_str(),
    CLPProcessInformation);

  return EXIT_SUCCESS;
}

//-----------------------------------------------------------------------------
template <class T>
VoxelizedModelImageType::Pointer VoxelizeModel(vtkPolyData* model,
                                               typename InputImage<T>::Type::Pointer inputImage,
                                               double samplingDistance)
{
  VoxelizedModelImageType::Pointer label = VoxelizedModelImageType::New();
  label->CopyInformation( inputImage );
  // Negate to convert from LPS to RAS
  double wOrigin[3] = {0., 0., 0.};
  wOrigin[0] = -model->GetBounds()[1];
  wOrigin[1] = -model->GetBounds()[3];
  wOrigin[2] = model->GetBounds()[4];
  label->SetOrigin(wOrigin);
  VoxelizedModelImageType::RegionType region;
  double wSize[3] = {0., 0., 0.};
  wSize[0] = (model->GetBounds()[1] - model->GetBounds()[0]);
  wSize[1] = (model->GetBounds()[3] - model->GetBounds()[2]);
  wSize[2] = (model->GetBounds()[5] - wOrigin[2]);
  region.SetSize(0, wSize[0] / label->GetSpacing()[0]);
  region.SetSize(1, wSize[1] / label->GetSpacing()[1]);
  region.SetSize(2, wSize[2] / label->GetSpacing()[2]);
  label->SetRegions( region );
  label->Allocate();
  label->FillBuffer( 0 );

  //Calculation the sample-spacing, i.e the half of the smallest spacing existing in the original image
  double minSpacing = label->GetSpacing()[0];
  for (unsigned int i = 1; i < label->GetSpacing().Size(); ++i)
    {
    minSpacing = std::min(label->GetSpacing()[i], minSpacing);
    }

  vtkNew<vtkPolyDataPointIDSampler> sampler;
  sampler->SetInput( model );
  sampler->SetDistance( samplingDistance * minSpacing );
  sampler->GenerateVertexPointsOff();
  sampler->GenerateEdgePointsOff();
  sampler->GenerateInteriorPointsOn();
  sampler->GenerateVerticesOff();
  sampler->Update();

  std::cout << model->GetNumberOfPoints() << std::endl;
  std::cout << sampler->GetOutput()->GetNumberOfPoints() << std::endl;

  vtkPoints* points = sampler->GetOutput()->GetPoints();
  vtkPointData* pointData = sampler->GetOutput()->GetPointData();
  vtkIdTypeArray* modelCellIndexes = vtkIdTypeArray::SafeDownCast(
    pointData->GetScalars("cellIndexes"));
  for( vtkIdType k = 0; k < points->GetNumberOfPoints(); ++k )
    {
    double* pt = points->GetPoint( k );
    VoxelizedModelImageType::PointType pitk;
    // negate to convert from LPS to RAS
    pitk[0] = -pt[0];
    pitk[1] = -pt[1];
    pitk[2] = pt[2];
    VoxelizedModelImageType::IndexType idx;
    label->TransformPhysicalPointToIndex( pitk, idx );

    if( label->GetLargestPossibleRegion().IsInside(idx) )
      {
      vtkIdType pointId = modelCellIndexes->GetValue(k);
      label->SetPixel( idx, pointId );
      }
    }
  return label;
}

//-----------------------------------------------------------------------------
vtkPolyData* ReadPolyData(const std::string& fileName)
{
  vtkPolyData* polyData = 0;
  vtkSmartPointer<vtkPolyDataReader> pdReader;
  vtkSmartPointer<vtkXMLPolyDataReader> pdxReader;
  vtkSmartPointer<vtkSTLReader> stlReader;

  // do we have vtk or vtp models?
  std::string::size_type loc = fileName.find_last_of(".");
  if( loc == std::string::npos )
    {
    std::cerr << "Failed to find an extension for " << fileName << std::endl;
    return polyData;
    }

  std::string extension = fileName.substr(loc);

  if( extension == std::string(".vtk") )
    {
    pdReader = vtkSmartPointer<vtkPolyDataReader>::New();
    pdReader->SetFileName(fileName.c_str() );
    pdReader->Update();
    polyData = pdReader->GetOutput();
    }
  else if( extension == std::string(".vtp") )
    {
    pdxReader = vtkSmartPointer<vtkXMLPolyDataReader>::New();
    pdxReader->SetFileName(fileName.c_str() );
    pdxReader->Update();
    polyData = pdxReader->GetOutput();
    }
  else if( extension == std::string(".stl") )
    {
    stlReader = vtkSmartPointer<vtkSTLReader>::New();
    stlReader->SetFileName(fileName.c_str() );
    stlReader->Update();
    polyData = stlReader->GetOutput();
    }
  if( polyData == NULL )
    {
    std::cerr << "Failed to read surface " << fileName << std::endl;
    return polyData;
    }
  // Build Cells
  polyData->BuildLinks();

  // LPS vs RAS
  /*
  vtkPoints * allPoints = polyData->GetPoints();
  for( int k = 0; k < allPoints->GetNumberOfPoints(); k++ )
    {
    double* point = polyData->GetPoint( k );
    point[0] = -point[0];
    point[1] = -point[1];
    allPoints->SetPoint( k, point[0], point[1], point[2] );
    }
  */
  polyData->Register(0);
  return polyData;
}

//-----------------------------------------------------------------------------
void WritePolyData(vtkPolyData* polyData, const std::string& fileName)
{
  vtkNew<vtkPolyDataWriter> pdWriter;
  pdWriter->SetInput(polyData);
  pdWriter->SetFileName(fileName.c_str() );
  pdWriter->Update();
}

//-----------------------------------------------------------------------------
template <class T>
void WriteImage(typename InputImage<T>::Type::Pointer image,
                const std::string& fileName,
                ModuleProcessInformation* processInformation)
{
  typedef itk::ImageFileWriter<InputImage<T>::Type> WriterType;
  typename WriterType::Pointer writer = WriterType::New();
  itk::PluginFilterWatcher watchWriter(writer,
                                       "Write Volume",
                                       processInformation);
  writer->SetFileName( fileName.c_str() );
  writer->SetInput( image );
  writer->SetUseCompression(1);
  writer->Update();
}

//-----------------------------------------------------------------------------
void ComputeLocalTransform(vtkPolyData* restModel, vtkPolyData* posedModel, vtkPolyData* armature)
{
  //vtkNew<vtkUnsignedCharArray> colors;
  //colors->SetNumberOfComponents(3);
  //posedModel->GetCellData()->SetScalars(colors.GetPointer());

  vtkNew<vtkDoubleArray> transformArray;
  transformArray->SetName("Transforms");
  transformArray->SetNumberOfComponents(9);
  posedModel->GetCellData()->AddArray(transformArray.GetPointer());

  for (vtkIdType cellId = 0; cellId < posedModel->GetNumberOfCells(); ++cellId)
    {
    double transform[3][3] = {1.,0.,0.,0.,1.,0.,0.,0.,1.};
    // Find cell armature bone
    vtkIdType closestBoneId = GetClosestBone(armature, posedModel, cellId);
    if (closestBoneId != -1)
      {
      double cellPosedCenter[3] = {0., 0., 0.};
      GetCellCenter(posedModel, cellId, cellPosedCenter);
      double cellRestCenter[3] = {0., 0., 0.};
      GetCellCenter(restModel, cellId, cellRestCenter);
      double bonePosedHead[3] = {0., 0., 0.};
      GetPoint(armature, closestBoneId, 0, bonePosedHead);
      double boneRestHead[3] = {0., 0., 0.};
      GetPoint(armature, closestBoneId, 0, boneRestHead, "RestPoints");

      double localPosedCenter[3] = {0., 0., 0.};
      vtkMath::Subtract(cellPosedCenter, bonePosedHead, localPosedCenter);
      double localRestCenter[3] = {0., 0., 0.};
      vtkMath::Subtract(cellRestCenter, boneRestHead, localRestCenter);

      vtkSlicerArmaturesLogic::ComputeTransform(localRestCenter, localPosedCenter, transform);

      //colors->InsertNextTuple3(255, 255, 255);
      }
    else
      {
      //colors->InsertNextTuple3(0, 0, 0);
      }
    transformArray->InsertNextTuple9(
      transform[0][0], transform[0][1], transform[0][2],
      transform[1][0], transform[1][1], transform[1][2],
      transform[2][0], transform[2][1], transform[2][2]);
    }
}

//-----------------------------------------------------------------------------
void GetCellCenter(vtkPolyData* polyData, vtkIdType cellId, double center[3])
{
  center[0] = center[1] = center[2] = 0.;
  vtkIdType numberOfPoints = 0;
  vtkIdType* pointIds = 0;
  polyData->GetCellPoints(cellId, numberOfPoints, pointIds);
  if (numberOfPoints == 0)
    {
    std::cout << "cellId " << cellId << " doesn't exist in polydata."
              << std::endl;
    return;
    }
  for (vtkIdType pointId = 0; pointId < numberOfPoints; ++pointId)
    {
    double point[3] = {0.,0.,0.};
    polyData->GetPoint(pointIds[pointId], point);
    vtkMath::Add(point, center, center);
    }
  vtkMath::MultiplyScalar(center, 1. / numberOfPoints);
}

//-----------------------------------------------------------------------------
void GetPoint(vtkPolyData* polyData, vtkIdType cellId, vtkIdType pointIndex, double point[3], const char* pointArray)
{
  vtkIdType numberOfPoints = 0;
  vtkIdType* points = 0;
  polyData->GetCellPoints(cellId, numberOfPoints, points);
  assert(numberOfPoints == 2);
  assert(pointIndex < numberOfPoints);
  if (pointArray == 0)
    {
    polyData->GetPoint(points[pointIndex], point);
    }
  else
    {
    double* pointInPointArray =
      polyData->GetPointData()->GetScalars(pointArray)->GetTuple3(points[pointIndex]);
    memcpy(point, pointInPointArray, 3 * sizeof(double));
    }
}

//-----------------------------------------------------------------------------
void GetMatrix(vtkPolyData* polyData, vtkIdType cellId, double matrix[3][3], const char* matrixArray)
{
  double* m =
    polyData->GetCellData()->GetArray(matrixArray)->GetTuple9(cellId);
  memcpy(matrix, m, 9 * sizeof(double));
}

//-----------------------------------------------------------------------------
vtkIdType GetClosestBone(vtkPolyData* armature, vtkPolyData* posedModel, vtkIdType cellId)
{
  vtkIdType closestBoneId = -1;
  double minDistance = std::numeric_limits<double>::max();
  double cellCenter[3] = {0., 0., 0.};
  GetCellCenter(posedModel, cellId, cellCenter);
  // Browse all the bone SurfaceCells arrays to find the one that contain the
  // cellId.
  // CellData arrays:
  // 0: "SurfaceCells": link to
  // 1: "Transforms": pose rotation matrix of each bone
  // 2: "SurfaceCells-0-0" : organ0 faces associated to bone 0
  // 3: "SurfaceCells-0-1" : organ1 faces associated to bone 0
  // ...
  // n+2: "SurfaceCells-0-n" : organN faces associated to bone 0
  // n+3: "SurfaceCells-1-0" : organ0 faces associated to bone 1
  // ...
  // The 2 first arrays are: the bones cells and the bones Transform cells.
  // Then
  int surfaceId = 1; // bone = 0, skin = 1
  for (vtkIdType fieldArray = 0;
       fieldArray < armature->GetCellData()->GetNumberOfArrays();
       ++fieldArray)
    {
    std::stringstream fieldArrayName;
    fieldArrayName << "SurfaceCells" << fieldArray << '-' << surfaceId;
    vtkIdTypeArray* cellFieldArray = vtkIdTypeArray::SafeDownCast(
      armature->GetCellData()->GetArray(fieldArrayName.str().c_str()));
    if (cellFieldArray == NULL)
      {
      continue;
      }
    // Check if the cell is associated to the bone (one field array per bone)
    vtkIdType numberOfPoints = 0;
    vtkIdType* pointIds = 0;
    posedModel->GetCellPoints(cellId, numberOfPoints, pointIds);
    vtkIdType index = -1;
    for (vtkIdType i = 0; i < numberOfPoints; ++i)
      {
      index = cellFieldArray->LookupValue(pointIds[i]);
      if (index != -1)
        {
        break;
        }
      }
    if (index != -1)
      {
      double boneCenter[3] = {0.,0.,0.};
      vtkIdType boneId = fieldArray;
      GetCellCenter(armature, boneId, boneCenter);
      double distance = vtkMath::Distance2BetweenPoints(boneCenter, cellCenter);
      if (distance < minDistance)
        {
        minDistance = distance;
        closestBoneId = boneId;
        }
      }
    }
  if (closestBoneId == -1)
    {
    std::cerr << "CellId " << cellId << " doesn't belong to any bone."
              << std::endl;
    }
  return closestBoneId;
}

//-----------------------------------------------------------------------------
template <class T>
void PoseLabelmap(typename VoxelizedModelImageType::Pointer posedLabelmap,
                  typename InputImage<T>::Type::Pointer restLabelmap,
                  typename DistanceMapImageType::Pointer posedDistancemap,
                  vtkPolyData* armature,
                  vtkPolyData* restModel, vtkPolyData* posedModel)
{
  int i = 0;
  itk::ImageRegionIterator<VoxelizedModelImageType> it( posedLabelmap, posedLabelmap->GetRequestedRegion() );
  for (it = it.Begin(); !it.IsAtEnd(); ++it)
    {
    ++i;
    //if (i < 2954382) // foot
    //if (i < 10000000) //mid tibia
    //if (i < 25000000) //high thigh
    //if (i < 41000000) //belly low arm
      {
      //continue;
      }
    //if (i == 3935750) // foot
    //if (i == 12000000) // upper tibia
    //if (i == 26000000) //high thigh
    //if (i == 31000000) //high thigh
    //if (i == 81000000)
    if (i == -1)
      {
      break;
      }
    if (i % 1000000 == 0)
      {
      std::cout << i << std::endl;
      }
    // For each result voxel
    VoxelizedModelImageType::PointType posedPhysicalPoint;
    posedLabelmap->TransformIndexToPhysicalPoint(it.GetIndex(), posedPhysicalPoint);
    if (posedPhysicalPoint.GetElement(0) > 44.5 && posedPhysicalPoint.GetElement(0) <= 46.5 &&
        posedPhysicalPoint.GetElement(1) > 584.5 && posedPhysicalPoint.GetElement(1) <= 586.5 &&
        posedPhysicalPoint.GetElement(2) > -368.5 && posedPhysicalPoint.GetElement(2) <= -366.5)
      {
      std::cout << "Here !" << std::endl;
      }
    // Find the closest posed mesh face
    DistanceMapImageType::IndexType distanceMapIndex;
    posedDistancemap->TransformPhysicalPointToIndex(posedPhysicalPoint, distanceMapIndex);
    DistanceMapImageType::ValueType cellId = posedDistancemap->GetPixel(distanceMapIndex);
    // Get the mesh face transform
    double faceMat[3][3] = {1., 0., 0., 0., 1., 0., 0., 0., 1.};
    memcpy(faceMat,
           posedModel->GetCellData()->GetArray("Transforms")->GetTuple9(cellId),
           9 * sizeof(double));
    // Get the closest bone transform
    vtkIdType boneId = GetClosestBone(armature, posedModel, cellId);
    double boneMat[3][3] = {1., 0., 0., 0., 1., 0., 0., 0., 1.};
    memcpy(boneMat,
           armature->GetCellData()->GetArray("Transforms")->GetTuple9(boneId),
           9 * sizeof(double));
    // Interpolate
    // negate to convert from RAS to LPS
    double posedPoint[3] = {-posedPhysicalPoint.GetElement(0), -posedPhysicalPoint.GetElement(1), posedPhysicalPoint.GetElement(2)};
    double modelProjection[3] = {0., 0., 0.};
    double boneProjection[3] = {0., 0., 0.};
    double distanceToModel = GetDistance2ToFace(posedModel, cellId, posedPoint, modelProjection);
    double distanceToBone = GetDistance2ToFace(armature, boneId, posedPoint, boneProjection);
#if 1
    //if (vtkMath::Dot(modelProjection, boneProjection) < 0. ||
    //    vtkMath::Norm(boneProjection) < vtkMath::Norm(modelProjection))
      {
#define ROTATE 1
#if INTERPOLATE
      double distanceRatio = distanceToModel / (distanceToModel + distanceToBone);
      double faceQuat[4] = {1., 0., 0., 0.};
      vtkMath::Matrix3x3ToQuaternion(faceMat, faceQuat);
      double boneQuat[4] = {1., 0., 0., 0.};
      vtkMath::Matrix3x3ToQuaternion(boneMat, boneQuat);
      double pointQuat[4] = {1., 0., 0., 0.};
      InterpolateQuaternion(faceQuat, boneQuat, distanceRatio, pointQuat);
      double pointMat[3][3] = {1., 0., 0., 0., 1., 0., 0., 0., 1.};
      vtkMath::QuaternionToMatrix3x3(pointQuat, pointMat);
      vtkMath::Invert3x3(pointMat, pointMat);

      double boneRestHead[3] = {0., 0., 0.};
      GetPoint(armature, boneId, 0, boneRestHead, "RestPoints");
      double boneRestTail[3] = {0., 0., 0.};
      GetPoint(armature, boneId, 1, boneRestTail, "RestPoints");
      double boneRest[3] = {0., 0., 0.};
      vtkMath::Subtract(boneRestTail, boneRestHead, boneRest);
      vtkMath::Normalize(boneRest);
      double y[3] = {0., 1., 0.};
      double boneRestMat[3][3] = {1., 0., 0., 0., 1., 0., 0., 0., 1.};
      vtkSlicerArmaturesLogic::ComputeTransform(y, boneRest, boneRestMat);
      vtkMath::Multiply3x3(boneRestMat, pointMat, pointMat);
      double restPoint[3] = {pointMat[1][0], pointMat[1][1], pointMat[1][1]};
      vtkMath::Normalize(restPoint);
      double bonePosedHead[3] = {0., 0., 0.};
      GetPoint(armature, boneId, 0, bonePosedHead);
      double posedPointVec[3] = {0., 0., 0.};
      vtkMath::Subtract(posedPoint, bonePosedHead, posedPointVec);
      double distanceToHead = vtkMath::Norm(posedPointVec);
      vtkMath::MultiplyScalar(restPoint, distanceToHead);
#elif ADD
      double relativePosedPoint[3];
      vtkMath::Subtract(posedPoint, bonePosedHead, relativePosedPoint);
      vtkMath::Add(relativePosedPoint, boneRestHead, restPoint);
#elif ROTATE
      double bonePosedMatrix[3][3] = {1., 0., 0., 0., 1., 0., 0., 0., 1.};
      GetMatrix(armature, boneId, bonePosedMatrix, "Frames");

      double posedBoneHead[3] = {0., 0., 0.};
      GetPoint(armature, boneId, 0, posedBoneHead);
      double posedBoneTail[3] = {0., 0., 0.};
      GetPoint(armature, boneId, 1, posedBoneTail);
      double posedBoneTailVector[3] = {0., 0., 0.};
      vtkMath::Subtract(posedBoneTail, posedBoneHead, posedBoneTailVector);

      double restBoneHead[3] = {0., 0., 0.};
      GetPoint(armature, boneId, 0, restBoneHead, "RestPoints");
      double restBoneTail[3] = {0., 0., 0.};
      GetPoint(armature, boneId, 1, restBoneTail, "RestPoints");
      double restBoneTailVector[3] = {0., 0., 0.};
      vtkMath::Subtract(restBoneTail, restBoneHead, restBoneTailVector);

      double posedPointVector[3] = {0., 0., 0.};
      vtkMath::Subtract(posedPoint, posedBoneHead, posedPointVector);

      double posedPointTransform[3][3] = {1., 0., 0., 0., 1., 0., 0., 0., 1.};
      vtkSlicerArmaturesLogic::ComputeTransform(posedBoneTailVector, posedPointVector, posedPointTransform);

      double boneRestMatrix[3][3] = {1., 0., 0., 0., 1., 0., 0., 0., 1.};
      GetMatrix(armature, boneId, boneRestMatrix, "RestFrames");

      double invBonePosedMatrix[3][3] = {1., 0., 0., 0., 1., 0., 0., 0., 1.};
      vtkMath::Invert3x3(bonePosedMatrix, invBonePosedMatrix);

      double restToPosedTransform[3][3] = {1., 0., 0., 0., 1., 0., 0., 0., 1.};
      vtkSlicerArmaturesLogic::ComputeTransform(restBoneTailVector, posedBoneTailVector, restToPosedTransform);

      double invRestToPosedTransform[3][3] = {1., 0., 0., 0., 1., 0., 0., 0., 1.};
      vtkMath::Invert3x3(restToPosedTransform, invRestToPosedTransform);

      //vtkMath::Normalize(invBonePosedMatrix[0]);
      //vtkMath::Normalize(invBonePosedMatrix[1]);
      //vtkMath::Normalize(invBonePosedMatrix[2]);

      double m[3][3] = {1., 0., 0., 0., 1., 0., 0., 0., 1.};
      vtkMath::Multiply3x3(invRestToPosedTransform, m, m);
      vtkMath::Multiply3x3(posedPointTransform, m, m);
      vtkMath::Multiply3x3(restToPosedTransform, m, m);
      vtkMath::Multiply3x3(boneRestMatrix, m, m);
      double restPointVect[3] = {m[1][0], m[1][1], m[1][2]};


      double restPoint[3] = {0., 0., 0.};
      vtkMath::Add(restPointVect, restBoneHead, restPoint);

      //vtkMath::Multiply3x3(bonePosedMatrix, m, m);
      //vtkMath::Multiply3x3(invBonePosedMatrix, m, m);


#endif

      //double restPoint[3];
      //vtkMath::Multiply3x3(pointMat, posedPoint, restPoint);
      // Find value in rest labelmap
      // Negate to convert from RAS to LPS
      VoxelizedModelImageType::PointType restPhysicalPoint;
      restPhysicalPoint.SetElement(0, -restPoint[0]);
      restPhysicalPoint.SetElement(1, -restPoint[1]);
      restPhysicalPoint.SetElement(2, restPoint[2]);
      typename VoxelizedModelImageType::IndexType restIndex;
      restLabelmap->TransformPhysicalPointToIndex(restPhysicalPoint, restIndex);
      if (restLabelmap->GetLargestPossibleRegion().IsInside(restIndex))
        {
        T restPixel = restLabelmap->GetPixel(restIndex);
        posedLabelmap->SetPixel(it.GetIndex(), restPixel);
        }
      }
#else
    if (vtkMath::Dot(modelProjection, boneProjection) < 0. ||
        vtkMath::Norm(boneProjection) < vtkMath::Norm(modelProjection))
      {
      posedLabelmap->SetPixel(it.GetIndex(), 200);
      }
#endif
    }
}

//-----------------------------------------------------------------------------
double GetDistance2ToFace(vtkPolyData* polyData, vtkIdType cellId, double point[3], double* projection)
{
  vtkCell* cell = polyData->GetCell(cellId);
  int subId = -1;
  double closestPoint[3], pCoords[3], dist2;
  double* weights = new double [cell->GetNumberOfPoints()];
  cell->EvaluatePosition(point, closestPoint, subId, pCoords, dist2, weights);
  if (projection)
    {
    projection[0] = closestPoint[0] - point[0];
    projection[1] = closestPoint[1] - point[1];
    projection[2] = closestPoint[2] - point[2];
    }
  //vtkMath::Normalize(projection);
  delete [] weights;
  return dist2;
}

//-----------------------------------------------------------------------------
void InterpolateQuaternion(double qa[4], double qb[4], double t, double qm[4])
{
  // Calculate angle between them.
  double cosHalfTheta = qa[0] * qb[0] + qa[1] * qb[1] + qa[2] * qb[2] + qa[3] * qb[3];
  // if qa=qb or qa=-qb then theta = 0 and we can return qa
  if (abs(cosHalfTheta) >= 1.0)
    {
    qm[0] = qa[0];qm[1] = qa[1];qm[2] = qa[2];qm[3] = qa[3];
    return;
    }
  // Calculate temporary values.
  double halfTheta = acos(cosHalfTheta);
  double sinHalfTheta = sqrt(1.0 - cosHalfTheta*cosHalfTheta);
  // if theta = 180 degrees then result is not fully defined
  // we could rotate around any axis normal to qa or qb
  if (fabs(sinHalfTheta) < 0.001)
    { // fabs is floating point absolute
    qm[0] = (qa[0] * 0.5 + qb[0] * 0.5);
    qm[1] = (qa[1] * 0.5 + qb[1] * 0.5);
    qm[2] = (qa[2] * 0.5 + qb[2] * 0.5);
    qm[3] = (qa[3] * 0.5 + qb[3] * 0.5);
    return;
    }
  double ratioA = sin((1 - t) * halfTheta) / sinHalfTheta;
  double ratioB = sin(t * halfTheta) / sinHalfTheta;
  //calculate Quaternion.
  qm[0] = (qa[0] * ratioA + qb[0] * ratioB);
  qm[1] = (qa[1] * ratioA + qb[1] * ratioB);
  qm[2] = (qa[2] * ratioA + qb[2] * ratioB);
  qm[3] = (qa[3] * ratioA + qb[3] * ratioB);
  return;
}
