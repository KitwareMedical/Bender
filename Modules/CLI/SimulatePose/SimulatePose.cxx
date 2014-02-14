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
#include "SimulatePoseCLP.h"
#include "benderIOUtils.h"
#include "vtkQuaternion.h"

// SOFA includes
#include <sofa/component/collision/BruteForceDetection.h>
#include <sofa/component/collision/DefaultCollisionGroupManager.h>
#include <sofa/component/collision/DefaultContactManager.h>
#include <sofa/component/collision/DefaultPipeline.h>
#include <sofa/component/collision/LineModel.h>
#include <sofa/component/collision/LocalMinDistance.h>
#include <sofa/component/collision/MinProximityIntersection.h>
#include <sofa/component/collision/NewProximityIntersection.h>
#include <sofa/component/collision/PointModel.h>
#include <sofa/component/collision/TriangleModel.h>
#include <sofa/component/linearsolver/CGLinearSolver.h>
#include <sofa/component/mapping/BarycentricMappingRigid.h>
#include <sofa/component/misc/RequiredPlugin.h>
#include <sofa/component/misc/VTKExporter.h>
#include <sofa/component/odesolver/EulerImplicitSolver.h>
#include <sofa/component/odesolver/EulerSolver.h>
#include <sofa/component/projectiveconstraintset/SkeletalMotionConstraint.h>
#include <sofa/component/topology/MeshTopology.h>
#include <sofa/component/typedef/Sofa_typedef.h>
#include <sofa/helper/vector.h>
#include <sofa/simulation/common/Node.h>
#include <sofa/simulation/graph/DAGSimulation.h>

// SofaCUDA includes
#ifdef SOFA_CUDA
#include <plugins/SofaCUDA/sofa/gpu/cuda/CudaTetrahedronFEMForceField.h>
#include <plugins/SofaCUDA/sofa/gpu/cuda/CudaCollisionDetection.h>
#include <plugins/SofaCUDA/sofa/gpu/cuda/CudaMechanicalObject.h>
#include <plugins/SofaCUDA/sofa/gpu/cuda/CudaTriangleObject.h>
#include <plugins/SofaCUDA/sofa/gpu/cuda/CudaLineModel.h>
#include <plugins/SofaCUDA/sofa/gpu/cuda/CudaPointModel.h>
#include <plugins/SofaCUDA/sofa/gpu/cuda/CudaUniformMass.h>
#endif

// VTK includes
#include <vtkCellArray.h>
#include <vtkCellCenters.h>
#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkDataSetSurfaceFilter.h>
#include <vtkFloatArray.h>
#include <vtkIdTypeArray.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkPolyDataNormals.h>
#include <vtkPolyDataReader.h>
#include <vtkSmartPointer.h>
#include <vtkThreshold.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkUnstructuredGrid.h>
#include <vtkMath.h>
#include <vtkTriangleFilter.h>

// ---------------------------------------------------------------------
// ---
// ---------------------------------------------------------------------
using namespace sofa::component::collision;
using namespace sofa::component::container;
using namespace sofa::component::forcefield;
using namespace sofa::component::odesolver;
using namespace sofa::component::linearsolver;
using namespace sofa::component::mapping;
using namespace sofa::component::projectiveconstraintset;
using namespace sofa::component::topology;
using namespace sofa::component::visualmodel;
using namespace sofa::helper;
using namespace sofa::simulation;

/// helper function for more compact component creation
// ---------------------------------------------------------------------
template<class Component>
typename Component::SPtr addNew( Node::SPtr parentNode, std::string name="" )
{
  typename Component::SPtr component =
    sofa::core::objectmodel::New<Component>();
  parentNode->addObject(component);
  component->setName(parentNode->getName()+"_"+name);
  return component;
}

// Copy point positions from vtk to a mechanical object
// ---------------------------------------------------------------------
void copyVertices( vtkPoints *                  points,
                   MechanicalObject<Vec3Types>* mechanicalMesh )
{
  mechanicalMesh->resize(points->GetNumberOfPoints());

  std::cout << "  Total # of vertices: " << points->GetNumberOfPoints() <<
    std::endl;

  Data<MechanicalObject<Vec3Types>::VecCoord>* x =
    mechanicalMesh->write(VecCoordId::position());

  // Copy vertices from vtk mesh
  MechanicalObject<Vec3Types>::VecCoord &vertices = *x->beginEdit();
  for(vtkIdType i = 0, end = points->GetNumberOfPoints(); i < end; ++i)
    {
    Vector3 point;
    points->GetPoint(i,point.ptr());
    vertices[i] = point;
//     std::cout << "vertex[" << i << "] = " << vertices[i] << std::endl;
    }
  x->endEdit();
}


vtkQuaterniond computeOrientationFromReferenceAxis(Vector3 &head,
                                                   Vector3 &tail )
{
  double         Y[3] = {0.0, 1.0, 0.0};
  vtkQuaterniond newOrientation;
  // Code greatly inspired by: http://www.fastgraph.com/makegames/3drotation/ .

  double viewOut[3]; // The View or "new Z" vector.
  double viewUp[3]; // The Up or "new Y" vector.
  double viewRight[3]; // The Right or "new X" vector.

  double upMagnitude; // For normalizing the Up vector.
  double upProjection; // Magnitude of projection of View Vector on World UP.

  // First: calculate and normalize the view vector.
  vtkMath::Subtract(tail.ptr(), head.ptr(), viewOut);

  // Normalize. This is the unit vector in the "new Z" direction.
  if (vtkMath::Normalize(viewOut) < 0.0000001)
    {
    std::cerr <<
      "Tail and Head are not enough apart, could not rebuild rest Transform" <<
      std::endl;
    return newOrientation;
    }

  // Now the hard part: The ViewUp or "new Y" vector.

  // The dot product of ViewOut vector and World Up vector gives projection of
  // of ViewOut on WorldUp.
  upProjection = vtkMath::Dot(viewOut, Y);

  // First try at making a View Up vector: use World Up.
  viewUp[0] = Y[0] - upProjection*viewOut[0];
  viewUp[1] = Y[1] - upProjection*viewOut[1];
  viewUp[2] = Y[2] - upProjection*viewOut[2];

  // Check for validity:
  upMagnitude = vtkMath::Norm(viewUp);

  if (upMagnitude < 0.0000001)
    {
    // Second try at making a View Up vector: Use Y axis default (0,1,0).
    viewUp[0] = -viewOut[1]*viewOut[0];
    viewUp[1] = 1-viewOut[1]*viewOut[1];
    viewUp[2] = -viewOut[1]*viewOut[2];

    // Check for validity:
    upMagnitude = vtkMath::Norm(viewUp);

    if (upMagnitude < 0.0000001)
      {
      // Final try at making a View Up vector: Use Z axis default (0,0,1).
      viewUp[0] = -viewOut[2]*viewOut[0];
      viewUp[1] = -viewOut[2]*viewOut[1];
      viewUp[2] = 1-viewOut[2]*viewOut[2];

      // Check for validity:
      upMagnitude = vtkMath::Norm(viewUp);

      if (upMagnitude < 0.0000001)
        {
        std::cerr <<
          "Could not fin a vector perpendiculare to the bone, check the bone values. This should not be happening.";
        return newOrientation;
        }
      }
    }

  // Normalize the Up Vector.
  upMagnitude = vtkMath::Normalize(viewUp);

  // Calculate the Right Vector. Use cross product of Out and Up.
  vtkMath::Cross(viewUp, viewOut, viewRight);
  vtkMath::Normalize(viewRight); //Let's be paranoid about the normalization.

  // Get the rest transform matrix.
  newOrientation.SetRotationAngleAndAxis(acos(upProjection), viewRight);
  newOrientation.Normalize();

  return newOrientation;
}

// Add the collision model used to resolve collisions
// ---------------------------------------------------------------------
void addCollisionModels(Node::SPtr                      collisionNode,
                        const std::vector<std::string> &elements,
                        SReal                           stiffness = 10
                        )
{
  for (size_t i=0; i < elements.size(); ++i)
    {
    if (elements[i] == "Triangle")
      {
      TriangleModel::SPtr triModel = addNew<TriangleModel>(collisionNode,
        "TriangleCollision");
      triModel->setSelfCollision(true);
      triModel->setContactStiffness(stiffness);
      }
    else if(elements[i] == "Line")
      {
      LineModel::SPtr lineModel = addNew<LineModel>(collisionNode,
        "LineCollision");
      lineModel->setSelfCollision(true);
      lineModel->setContactStiffness(stiffness);
      }
    else if (elements[i] == "Point")
      {
      PointModel::SPtr pointModel = addNew<PointModel>(collisionNode,
        "PointCollision");
      pointModel->setSelfCollision(true);
      pointModel->setContactStiffness(stiffness);
      }
    else
      {
      std::cerr << "Error: Invalid collision model" << std::endl;
      return;
      }
    }
}

// Create collision pipeline
//--------------------------------------------------------------------------
Node::SPtr createRootWithCollisionPipeline(const std::string& responseType = std::string(
                                             "default"))
{
  typedef LocalMinDistance ProximityType;
  Node::SPtr root = getSimulation()->createNewGraph("root");

  //Components for collision management
  //------------------------------------
  //--> adding collision pipeline
  DefaultPipeline::SPtr collisionPipeline =
    sofa::core::objectmodel::New<DefaultPipeline>();
  collisionPipeline->setName("Collision Pipeline");
  root->addObject(collisionPipeline);

  //--> adding collision detection system
  BruteForceDetection::SPtr detection =
    sofa::core::objectmodel::New<BruteForceDetection>();
  detection->setName("Detection");
  root->addObject(detection);

  //--> adding component to detection intersection of elements
  ProximityType::SPtr detectionProximity =
    sofa::core::objectmodel::New<ProximityType>();
  detectionProximity->setName("Proximity");
  detectionProximity->setAlarmDistance(0.1);     //warning distance
  detectionProximity->setContactDistance(0.07);   //min distance before setting a spring to create a repulsion
  root->addObject(detectionProximity);

  //--> adding contact manager
  DefaultContactManager::SPtr contactManager =
    sofa::core::objectmodel::New<DefaultContactManager>();
  contactManager->setName("Contact Manager");
  contactManager->setDefaultResponseType(responseType);
  root->addObject(contactManager);

  //--> adding component to handle groups of collision.
  DefaultCollisionGroupManager::SPtr collisionGroupManager =
    sofa::core::objectmodel::New<DefaultCollisionGroupManager>();
  collisionGroupManager->setName("Collision Group Manager");
  root->addObject(collisionGroupManager);

  return root;
}


/// Visualization node (for debug purposes only)
// ---------------------------------------------------------------------
Node::SPtr createVisualNode(Node *                        parentNode,
                            vtkPolyData *                 polyMesh,
                            MechanicalObject<Vec3Types> * mechanicalObject,
                            int                           label = 0
                            )
{

  vtkNew<vtkDataSetSurfaceFilter> surfaceExtractor;

  if(label != 0)
    {
    vtkNew<vtkThreshold> meshThreshold;
    meshThreshold->SetInput(polyMesh);
    meshThreshold->ThresholdBetween(label,label);
    surfaceExtractor->SetInput(meshThreshold->GetOutput());
    }
  else
    {
    surfaceExtractor->SetInput(polyMesh);

    }
  surfaceExtractor->Update();

  Node::SPtr     visualNode = parentNode->createChild("visualNode");
  OglModel::SPtr oglModel   = addNew<OglModel>(visualNode,"oglModel");

  vtkNew<vtkPolyDataNormals> surfaceNormals;
  surfaceNormals->SetInput(surfaceExtractor->GetOutput());
  surfaceNormals->ComputeCellNormalsOn();
  surfaceNormals->Update();

  vtkFloatArray *cellNormals = vtkFloatArray::SafeDownCast(
    surfaceNormals->GetOutput()->GetCellData()->GetNormals());

  ResizableExtVector<Vec3f> normals;
  normals.reserve(cellNormals->GetNumberOfTuples());

  for(vtkIdType i = 0, end = cellNormals->GetNumberOfTuples(); i < end; ++i)
    {
    Vec3f normal;
    cellNormals->GetTupleValue(i,normal.ptr());
    normals.push_back(normal);
    }
  oglModel->setVnormals(&normals);

  IdentityMapping<Vec3Types,
                  ExtVec3fTypes>::SPtr identityMapping =
    addNew<IdentityMapping<Vec3Types, ExtVec3fTypes> >(visualNode,
      "identityMapping");
  identityMapping->setModels(mechanicalObject,oglModel.get());

  return visualNode;
}

// Fill armature joints - rest and final positions
// ---------------------------------------------------------------------
void getBoneCoordinates(
  vtkPolyData *                                      armature,
  sofa::helper::vector<SkeletonJoint<Rigid3Types> >& skeletonJoints,
  sofa::helper::vector<SkeletonBone>&                skeletonBones,
  sofa::helper::vector<Rigid3Types::Coord>&          restCoordinates,
  bool                                               invertXY = true)
{
  vtkCellArray* armatureSegments = armature->GetLines();
  vtkCellData*  armatureCellData = armature->GetCellData();

  vtkPoints* points = armature->GetPoints();

  std::cout << "Number of bones: " << armatureSegments->GetNumberOfCells() <<
    std::endl;

  vtkIdTypeArray* parenthood = vtkIdTypeArray::SafeDownCast(armatureCellData->GetArray(
      "Parenthood"));

  vtkNew<vtkIdList> cell;
  armatureSegments->InitTraversal();
  int edgeId(0);
  while(armatureSegments->GetNextCell(cell.GetPointer()))
    {
    vtkIdType a = cell->GetId(0);
    vtkIdType b = cell->GetId(1);
    Vector3   parentJoint(points->GetPoint(a));
    Vector3   childJoint(points->GetPoint(b));

    double A[12];
    armatureCellData->GetArray("Transforms")->GetTuple(edgeId, A);

    Matrix3 rotation;
    Vector3 translation;
    int     iA(0);
    for (int i=0; i<3; ++i)
      {
      for (int j=0; j<3; ++j,++iA)
        {
        rotation(i,j) = A[iA];
        }
      }
    std::cout << "Rotation = " << rotation << std::endl;
    rotation.transpose();
    translation[0] = A[9];
    translation[1] = A[10];
    translation[2] = A[11];

    if(invertXY)
      {
      //    Mat33 flipY;
      for (int i=0; i<3; ++i)
        {
        for (int j=0; j<3; ++j)
          {
          if( (i>1 || j>1) && i!=j)
            {
            rotation(i,j)*=-1;
            }
          }
        }
      translation[0]*=-1;
      translation[1]*=-1;
      }

    Rigid3Types::Coord finalPose,restPosition;
    Vector3            centerOfMass = 0.5*(childJoint+parentJoint);

    vtkQuaterniond q = computeOrientationFromReferenceAxis(centerOfMass,
      childJoint);

    restPosition.getCenter()      = centerOfMass;
    restPosition.getOrientation() = Quat3(q.GetX(),q.GetY(),q.GetZ(),q.GetW());
    restCoordinates.push_back(restPosition);

    finalPose.getCenter() = rotation*
                            (centerOfMass-parentJoint)+parentJoint+translation;
    Matrix3 orientation;
    restPosition.getOrientation().toMatrix(orientation);
    finalPose.getOrientation().fromMatrix(rotation*orientation);

    skeletonJoints.push_back(SkeletonJoint<Rigid3Types>());
    SkeletonJoint<Rigid3Types>& skeletonJoint = skeletonJoints.back();
    skeletonJoint.addChannel(restPosition,0.);
    skeletonJoint.addChannel(finalPose,1.);
    skeletonBones.push_back(edgeId);

    std::cout << "Bone " << skeletonJoint << std::endl;

    ++edgeId;
    }
}

/// Load bone mesh into a rigid mechanical object
//----------------------------------------------------------------------
MechanicalObject<Vec3Types>::SPtr createRigidBoneSurface(
  Node *       parentNode,
  vtkPolyData *polyMesh,
  int          label = 209)
{
  vtkSmartPointer<vtkPoints>    points;
  vtkSmartPointer<vtkCellArray> triangles;

  if(label != 0)
    {
    vtkSmartPointer<vtkThreshold> meshThreshold = vtkThreshold::New();
    meshThreshold->SetInput(polyMesh);
    meshThreshold->ThresholdBetween(label,label);

    vtkSmartPointer<vtkDataSetSurfaceFilter> surfaceExtractor =
      vtkDataSetSurfaceFilter::New();
    surfaceExtractor->SetInput(meshThreshold->GetOutput());
    surfaceExtractor->Update();

    vtkPolyData *mesh = surfaceExtractor->GetOutput();
    points    = mesh->GetPoints();
    triangles = mesh->GetPolys();
    }
  else
    {
    vtkSmartPointer<vtkDataSetSurfaceFilter> surfaceExtractor =
      vtkDataSetSurfaceFilter::New();
    surfaceExtractor->SetInput(polyMesh);
    surfaceExtractor->Update();
    points    = polyMesh->GetPoints();
    triangles = polyMesh->GetPolys();
    }

  MechanicalObject<Vec3Types>::SPtr boneStructure =
    addNew<MechanicalObject<Vec3Types> >(parentNode, "boneStructure");

  copyVertices(points,boneStructure.get());

  return boneStructure;
}

/// Create a mechanical articulated and constrained object
/// This functions loads initial and final position of the armature and
/// optionaly creates an animation between the two keyframes
// ---------------------------------------------------------------------
MechanicalObject<Rigid3Types>::SPtr createArticulatedFrame(
  Node *       parentNode,
  vtkPolyData *armature,
  bool         generateFrameAnimation = true,
  bool         invertXY = true
  )
{
  // Extract coordinates
  sofa::helper::vector<SkeletonJoint<Rigid3Types> > skeletonJoints;
  sofa::helper::vector<SkeletonBone>                skeletonBones;
  sofa::helper::vector<Rigid3Types::Coord>          boneCoordinates;
  getBoneCoordinates(armature, skeletonJoints, skeletonBones,
                     boneCoordinates, invertXY);

  MechanicalObject<Rigid3Types>::SPtr articulatedFrame =
    addNew<MechanicalObject<Rigid3Types> >(parentNode, "articulatedFrame");

  // Get bone positions
  size_t totalNumberOfBones = boneCoordinates.size();
  std::cout << "Number of bones: " << totalNumberOfBones << std::endl;

  articulatedFrame->resize(totalNumberOfBones);
  Data<MechanicalObject<Rigid3Types>::VecCoord> *x =
    articulatedFrame->write(VecCoordId::position());

  MechanicalObject<Rigid3Types>::VecCoord &vertices = *x->beginEdit();

  for(size_t i = 0, end = totalNumberOfBones; i < end; ++i)
    {
    vertices[i] = boneCoordinates[i];
    std::cout << "Bone[" << i << "] = " << vertices[i] << std::endl;
    }
  x->endEdit();

  if(generateFrameAnimation)
    {
    // generating a skeletal motion, this creates an animation of the
    // armature that takes it from initial pose to final pose
    SkeletalMotionConstraint<Rigid3Types>::SPtr skeletalMotionConstraint =
      addNew<SkeletalMotionConstraint<Rigid3Types> >(parentNode,
        "skeletalConstaint");

    skeletalMotionConstraint->setSkeletalMotion(skeletonJoints,skeletonBones);
    }
  return articulatedFrame;
}

/// Create a FEM in parentNode.  A MeshTopology should be defined in
/// parentNode prior to calling this function.
// ---------------------------------------------------------------------
void createFiniteElementModel(Node *              parentNode,
                              Vec3Types::VecReal &youngModulus )
{
  TetrahedronFEMForceField< Vec3Types >::SPtr femSolver =
    addNew<TetrahedronFEMForceField< Vec3Types > >(parentNode,"femSolver");
  femSolver->setComputeGlobalMatrix(false);
  femSolver->setMethod("large");
  femSolver->setPoissonRatio(.3);
  femSolver->_youngModulus.setValue(youngModulus);
}


/// Loads a vtk tetrahedral polymesh and creates a mechanical object and
/// the corresponding MeshTopology.
// ---------------------------------------------------------------------
MechanicalObject<Vec3Types>::SPtr loadMesh(Node*               parentNode,
                                           vtkPolyData *       polyMesh,
                                           Vec3Types::VecReal &youngModulus,
                                           int                 label = 0
                                           )
{
  // load mesh
  vtkSmartPointer<vtkPoints>    points;
  vtkSmartPointer<vtkCellArray> tetras;
  vtkSmartPointer<vtkCellData>  data;

  if(label != 0)
    {
    vtkNew<vtkThreshold> meshThreshold;
    meshThreshold->SetInput(polyMesh);
    meshThreshold->ThresholdBetween(label,label);
    meshThreshold->Update();

    vtkUnstructuredGrid *mesh = meshThreshold->GetOutput();
    points = mesh->GetPoints();
    tetras = mesh->GetCells();
    data   = mesh->GetCellData();
    }
  else
    {
    points = polyMesh->GetPoints();
    tetras = polyMesh->GetPolys();
    data   = polyMesh->GetCellData();
    }

  std::stringstream meshName;
  meshName << "Mesh" << label;

  // Create mechanical object (dof) for the mesh and extract material parameters
  MechanicalObject<Vec3Types>::SPtr mechanicalMesh =
    addNew<MechanicalObject<Vec3Types> >(parentNode,meshName.str());

  copyVertices(points.GetPointer(),mechanicalMesh.get());

  // Create the MeshTopology
  MeshTopology::SPtr meshTopology = addNew<MeshTopology>(parentNode,
    "Topology");
  meshTopology->seqPoints.setParent(&mechanicalMesh->x);

  // Copy tetrahedra array from vtk cell array
  MeshTopology::SeqTetrahedra& tetrahedra =
    *meshTopology->seqTetrahedra.beginEdit();
  tetrahedra.reserve(tetras->GetNumberOfCells());
  youngModulus.reserve(tetras->GetNumberOfCells());

  std::cout << "Total # of tetrahedra: " << tetras->GetNumberOfCells()
            << std::endl;

  tetras->InitTraversal();

  vtkDataArray* materialParameters = data->GetArray("MaterialParameters");
  if (!materialParameters)
    {
    std::cerr << "Error: No material parameters data array in mesh" << std::endl;
    }

  vtkNew<vtkIdList> element;
  vtkIdType         cellId = 0;
  while(tetras->GetNextCell(element.GetPointer()))
    {
    if(element->GetNumberOfIds() != 4)
      {
      std::cerr << "Error: Non-tetrahedron encountered." << std::endl;
      cellId++;
      continue;
      }
    tetrahedra.push_back(MeshTopology::Tetra(element->GetId(0),
        element->GetId(1),element->GetId(2),element->GetId(3)));

    if (materialParameters)
      {
      double parameters[2] = {0};
      materialParameters->GetTuple(cellId, parameters);
      youngModulus.push_back(parameters[0]);
      }
    }
  meshTopology->seqTetrahedra.endEdit();
  return mechanicalMesh;
}

/// Create a skinning map between mesh and armature (is a distance map)
/// This uses a Shepard shape function method
// ---------------------------------------------------------------------
void skinMesh(Node *                              parentNode,
              MechanicalObject<Rigid3Types>::SPtr articulatedFrame,
              MechanicalObject<Vec3Types>::SPtr   mechanicalObject,
              vtkPolyData *                       armature,
              vtkPolyData *                       polyMesh,
              int                                 label = 0
              )
{
  typedef SkinningMapping<Rigid3Types, Vec3Types> SkinningMappingType;

  vtkSmartPointer<vtkPoints>    points;
  vtkSmartPointer<vtkPointData> data;

  if(label != 0)
    {
    vtkSmartPointer<vtkThreshold> meshThreshold = vtkThreshold::New();
    meshThreshold->SetInput(polyMesh);
    meshThreshold->ThresholdBetween(label,label);
    meshThreshold->Update();

    vtkUnstructuredGrid *mesh = meshThreshold->GetOutput();
    points = mesh->GetPoints();
    data   = mesh->GetPointData();
    }
  else
    {
    points = polyMesh->GetPoints();
    data   = polyMesh->GetPointData();
    }

  SkinningMappingType::SPtr boneSkinningMapping =
    addNew<SkinningMappingType>(parentNode,"SkinningMapping");
  if(boneSkinningMapping->isMechanical())
    std::cout << "The map is mechanical." << std::endl;

  boneSkinningMapping->setModels(articulatedFrame.get(),
    mechanicalObject.get());

  vtkIdType numberOfBones = armature->GetNumberOfCells();

  sofa::helper::vector<SVector<SkinningMappingType::InReal> > weights;
  sofa::helper::vector<SVector<unsigned int> >                indices;
  sofa::helper::vector<unsigned int>                          nbIds;
  sofa::helper::vector<float>                                 weightSum;

  vtkIdType numberOfVertices = points->GetNumberOfPoints();
  indices.resize(numberOfVertices);
  weights.resize(numberOfVertices);
  nbIds.resize(numberOfVertices,0);
  weightSum.resize(numberOfVertices,0.);

  for(vtkIdType i = 0; i < numberOfBones; ++i)
    {
    vtkFloatArray *weightArray = vtkFloatArray::SafeDownCast(data->GetArray(i));
    if(weightArray->GetNumberOfTuples() != numberOfVertices || !weightArray)
      {
      std::cerr << "Error extracting weight array." << std::endl;
      return;
      }

    for (vtkIdType j = 0; j < numberOfVertices; ++j)
      {
      float weight = weightArray->GetValue(j);
      if (weight < 0.001)
        {
        continue;
        }
      weights[j].push_back(weight);
      indices[j].push_back(i);
      ++nbIds[j];
      weightSum[j]+=weight;
      }
    }

  // Make sure each vertex has at least one valid associated weight
  // TODO: Normalize weights -> weights[i][*]/weightSum[i]
  int weightErrorCount = 0;
  for(size_t i = 0; i < weightSum.size(); ++i)
    {
    if(weightSum[i] == 0.)
      {
      if (++weightErrorCount < 100)
        {
        std::cerr << "Error: Vertex " << i << " has no weight." << std::endl;
        }
      }
    }
  if (weightErrorCount)
    {
    std::cerr << "-> " << weightErrorCount << " voxels with no weight. " << std::endl;
    }
  boneSkinningMapping->setWeights(weights,indices,nbIds);

}

/// Create a map between mesh and armature (distance map)
/// Using barycentric coordinates
// ---------------------------------------------------------------------
void mapArticulatedFrameToMesh(Node *                              parentNode,
                               MechanicalObject<Rigid3Types>::SPtr articulatedFrame,
                               MechanicalObject<Vec3Types>::SPtr   mechanicalObject
                               )
{
  typedef BarycentricMapping<Vec3Types,Rigid3Types> BarycentricMappingType;
  BarycentricMappingType::SPtr barycentricMapping =
    addNew<BarycentricMappingType>(parentNode,"Mapping");
  barycentricMapping->setModels(mechanicalObject.get(), articulatedFrame.get());
}

/// Sets the collision model
// ---------------------------------------------------------------------
Node::SPtr createCollisionNode(Node *parentNode, vtkPolyData * polyMesh,
                               MechanicalObject<Vec3Types> *volumeMesh,
                               int label = 0,
                               bool createCollisionSurface = false
                               )
{
  std::cout << "Creating collision node..." << std::endl;

  std::vector<std::string> modelTypes;
  modelTypes.push_back("Triangle");
  modelTypes.push_back("Line");
  modelTypes.push_back("Point");

  Node::SPtr collisionNode(parentNode, false);

  // Create a new node for a collision model if a surface is given
  if (createCollisionSurface)
    {

    if(!polyMesh)
      {
      std::cerr << "Warning! No valid surface given." << std::endl;
      addCollisionModels(collisionNode,modelTypes);
      return collisionNode;
      }

    collisionNode = parentNode->createChild("collisionNode");

    // Load mesh
    vtkSmartPointer<vtkPoints>    points;
    vtkSmartPointer<vtkCellArray> triangles;

    if(label != 0)
      {
      vtkSmartPointer<vtkThreshold> meshThreshold = vtkThreshold::New();
      meshThreshold->SetInput(polyMesh);
      meshThreshold->ThresholdBetween(label,label);
      meshThreshold->Update();

      vtkUnstructuredGrid *mesh = meshThreshold->GetOutput();
      points    = mesh->GetPoints();
      triangles = mesh->GetCells();
      }
    else
      {
      vtkSmartPointer<vtkTriangleFilter> extractTriangles =
        vtkTriangleFilter::New();
      extractTriangles->SetInput(polyMesh);
      extractTriangles->Update();

      points    = extractTriangles->GetOutput()->GetPoints();
      triangles = extractTriangles->GetOutput()->GetPolys();
      }

    double materialParameters[2] = {0};

    std::stringstream meshName;
    meshName << "SurfaceMesh" << label;

    // Create mechanical object for the mesh vertices
    MechanicalObject<Vec3Types>::SPtr surfaceMesh =
      addNew<MechanicalObject<Vec3Types> >(collisionNode,meshName.str());

    copyVertices(points.GetPointer(),surfaceMesh.get());

    // Topology
    MeshTopology::SPtr meshTopology = addNew<MeshTopology>(collisionNode,
      "SurfaceTopology");
    meshTopology->seqPoints.setParent(&surfaceMesh->x);

    // Copy tetrahedra array from vtk cell array
    MeshTopology::SeqTriangles& triangleArray =
      *meshTopology->seqTriangles.beginEdit();
    triangleArray.reserve(triangles->GetNumberOfCells());

    std::cout << "  Total # of triangles: " << triangles->GetNumberOfCells() <<
      std::endl;

    triangles->InitTraversal();
    vtkNew<vtkIdList> element;
    vtkIdType         cellId = 0;
    while(triangles->GetNextCell(element.GetPointer()))
      {
      if(element->GetNumberOfIds() != 3)
        {
        std::cerr << " Error: Non-triangle encountered." << std::endl;
        cellId++;
        continue;
        }
      MeshTopology::Triangle t(element->GetId(0),
                               element->GetId(1),element->GetId(2));
      triangleArray.push_back(t);
      }

    // Use a barycentric mapping to map surface to volume mesh
    BarycentricMapping3_to_3::SPtr mechMapping =
      addNew<BarycentricMapping3_to_3>(
        collisionNode,"collisionMapping");
    mechMapping->setModels(volumeMesh, surfaceMesh.get());
    }

  addCollisionModels(collisionNode,modelTypes);
  std::cout << "done creating collision node." << std::endl;

  return collisionNode;
}

void createEulerSolverNode(Node::SPtr         parentNode,
                           const std::string &scheme = "Explicit")
{
  typedef EulerImplicitSolver EulerImplicitSolverType;
  typedef EulerSolver EulerExplicitSolverType;
  typedef CGLinearSolver<GraphScatteredMatrix,
                         GraphScatteredVector> CGLinearSolverType;

  // Implicit time-step method requires a linear solver
  if (scheme == "Implicit")
    {
    EulerImplicitSolverType::SPtr odeSolver =
      addNew<EulerImplicitSolverType>(parentNode,"TimeIntegrator");

    CGLinearSolverType::SPtr linearSolver = addNew<CGLinearSolverType>(
      parentNode,"CGSolver");
    odeSolver->f_rayleighStiffness.setValue(0.01);
    odeSolver->f_rayleighMass.setValue(1);

    linearSolver->f_maxIter.setValue(25);                 //iteration maxi for the CG
    linearSolver->f_smallDenominatorThreshold.setValue(1e-05);
    linearSolver->f_tolerance.setValue(1e-05);
    }
  else if (scheme == "Explicit")
    {
    EulerExplicitSolverType::SPtr solver = addNew<EulerExplicitSolverType>(
      parentNode,"TimeIntegrator");
    }
  else
    {
    std::cerr << "Error: " << scheme <<
      " Integration Scheme not recognized" <<
      std::endl;
    }
}

//------------------------------------------------------------------------------
void initMesh(vtkPolyData* outputPolyData, vtkPolyData* inputPolyData,
              Node::SPtr anatomicalMesh)
{
  MeshTopology *topology = anatomicalMesh->getNodeObject<MeshTopology>();
  vtkNew<vtkPoints> points;
  const vtkIdType numberOfPoints = topology->getNbPoints();
  std::cout << "Number of Points: " << numberOfPoints << std::endl;
  points->SetNumberOfPoints(numberOfPoints);
  for (vtkIdType pointId = 0; pointId < numberOfPoints; ++pointId)
    {
    points->InsertPoint(pointId, topology->getPX(pointId),
                        topology->getPY(pointId),
                        topology->getPZ(pointId));
    }
  outputPolyData->SetPoints(points.GetPointer());
  // Cells
  vtkNew<vtkCellArray> cells;
  for (vtkIdType cellId = 0; cellId < topology->getNbTetras(); ++cellId)
    {
    const Tetra& t = topology->getTetra(cellId);
    vtkIdType cell[4];
    cell[0] = t[0];
    cell[1] = t[1];
    cell[2] = t[2];
    cell[3] = t[3];
    cells->InsertNextCell(4, cell);
    }
  outputPolyData->SetPolys(cells.GetPointer());

  for (int i = 0; i < inputPolyData->GetPointData()->GetNumberOfArrays(); ++i)
    {
    outputPolyData->GetPointData()->AddArray(inputPolyData->GetPointData()->GetArray(i));
    }
  for (int i = 0; i < inputPolyData->GetCellData()->GetNumberOfArrays(); ++i)
    {
    outputPolyData->GetCellData()->AddArray(inputPolyData->GetCellData()->GetArray(i));
    }

}

int main(int argc, char** argv)
{
  PARSE_ARGS;

  double dt = 0.01;
  sofa::simulation::setSimulation(new sofa::simulation::graph::DAGSimulation());

  // The graph root node
  Node::SPtr root = createRootWithCollisionPipeline();
  root->setGravity( Coord3(0,0,0) );
  root->setDt(dt);

#ifdef SOFA_CUDA
  // Load SofaCUDA plugin
  sofa::component::misc::RequiredPlugin::SPtr cudaPlugin =
    addNew<sofa::component::misc::RequiredPlugin>(root,"CUDA");
  cudaPlugin->pluginName.setValue("SofaCUDA");
#endif

  if (!IsMeshInRAS)
    {
    std::cout<<"Mesh x,y coordinates will be inverted" << std::endl;
    }
  if (!IsArmatureInRAS)
    {
    std::cout<<"Armature x,y coordinates will be inverted" << std::endl;
    }

  if (Verbose)
    {
    std::cout << "Read data..." << std::endl;
    }

  // Read vtk data
  vtkSmartPointer<vtkPolyData> armature;
  armature.TakeReference(
    bender::IOUtils::ReadPolyData(ArmaturePoly.c_str(),!IsArmatureInRAS));

  vtkSmartPointer<vtkPolyData> tetMesh;
  tetMesh.TakeReference(
    bender::IOUtils::ReadPolyData(InputTetMesh.c_str(),!IsMeshInRAS));

  vtkSmartPointer<vtkPolyData> surfaceMesh;
  if (EnableCollision)
    {
    surfaceMesh.TakeReference(
      bender::IOUtils::ReadPolyData(InputSurface.c_str(),!IsMeshInRAS));
    }

  // Create a scene node
  Node::SPtr sceneNode = root->createChild("BenderSimulation");

  // Time stepper for the armature
  createEulerSolverNode(root.get(),"Implicit");

  // Node for the mesh
  Node::SPtr anatomicalMesh = sceneNode->createChild("AnatomicalMesh");

  // Create mesh dof
  Vec3Types::VecReal                youngModulus;
  MechanicalObject<Vec3Types>::SPtr posedMesh = loadMesh(
    anatomicalMesh.get(), tetMesh, youngModulus);
  UniformMass3::SPtr mass = addNew<UniformMass3>(anatomicalMesh.get(),"Mass");
  mass->setTotalMass(100);

  // Add VTK exporter
  sofa::component::misc::VTKExporter::SPtr exporter =
    addNew<sofa::component::misc::VTKExporter>(anatomicalMesh, "vtkExporter");
  exporter->exportAtEnd.setValue(true);
  exporter->vtkFilename.setValue(OutputTetMesh);
  exporter->writeTetras.setValue(true);
  exporter->writeEdges.setValue(false);

  if (Verbose)
    {
    std::cout << "Create finite element model..." << std::endl;
    }
  // Finite element method
  createFiniteElementModel(anatomicalMesh.get(),youngModulus);

  // Collision node
  if (EnableCollision)
    {
    if (Verbose)
      {
      std::cout << "************************************************************"
                << std::endl;
      std::cout << "Create collision node..." << std::endl;
      }
    createCollisionNode(anatomicalMesh.get(),
                        surfaceMesh,posedMesh.get());
    }

  if (Verbose)
    {
    std::cout << "************************************************************"
              << std::endl;
    std::cout << "Create anatomical map..." << std::endl;
    }

  // Create a constrained articulated frame
  Node::SPtr anatomicalMap = anatomicalMesh->createChild("AnatomicalMap");

  if (Verbose)
    {
    std::cout << "************************************************************"
              << std::endl;
    std::cout << "Create articulated frame..." << std::endl;
    }

  MechanicalObject<Rigid3Types>::SPtr articulatedFrame =
    createArticulatedFrame(anatomicalMap.get(),
                           armature, true, !IsArmatureInRAS);

  if (Verbose)
    {
    std::cout << "************************************************************"
              << std::endl;
    std::cout << "Skin mesh..." << std::endl;
    }
  skinMesh(anatomicalMap.get(),articulatedFrame,posedMesh,
    armature,tetMesh);
//   mapArticulatedFrameToMesh(anatomicalMap.get(),articulatedFrame,posedMesh);

  if (Verbose)
    {
    std::cout << "************************************************************"
              << std::endl;
    std::cout << "Init..." << std::endl;
    }
  // Run simulation time steps
  sofa::simulation::getSimulation()->exportXML(root.get(),"scene.scn");
  sofa::simulation::getSimulation()->init(root.get());
  root->setAnimate(true);

  if (Verbose)
    {
    std::cout << "Animate..." << std::endl;
    }
  // --- Sofa time-stepping loop
  sofa::simulation::getSimulation()->animate(root.get());

  const int nbsteps = 3; //int(1/dt);
  if (Verbose)
    {
    std::cout << "Computing "<<nbsteps<<" iterations." << std::endl;
    }
  for (unsigned int i=0; i<nbsteps; i++)
    {
    sofa::simulation::getSimulation()->animate(root.get());
    if (Verbose)
      {
      std::cout << "Iteration: " << i+1 << std::endl;
      }
    }
  vtkNew<vtkPolyData> posedSurface;
  initMesh(posedSurface.GetPointer(), tetMesh, anatomicalMesh);
  if (!IsMeshInRAS)
    {
    vtkNew<vtkTransform> transform;
    transform->RotateZ(180.0);

    vtkNew<vtkTransformPolyDataFilter> transformer;
    transformer->SetInput(posedSurface.GetPointer());
    transformer->SetTransform(transform.GetPointer());
    transformer->Update();

    bender::IOUtils::WritePolyData(transformer->GetOutput(), OutputTetMesh);
    }
  else
    {
    bender::IOUtils::WritePolyData(posedSurface.GetPointer(), OutputTetMesh);
    }

  if (Verbose)
    {
    std::cout << "Unload..." << std::endl;
    }
  sofa::simulation::getSimulation()->unload(root);

  return 0;
}
