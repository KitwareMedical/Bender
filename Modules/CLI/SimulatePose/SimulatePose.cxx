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

// OpenGL includes
//#include <GL/glew.h>
//#include <GL/glut.h>

// SOFA includes
#include <plugins/ldidetection/lib/LayeredDepthImagesPipeline.h>
#include <plugins/ldidetection/lib/LDIDetection.h>
#include <plugins/Flexible/quadrature/TopologyGaussPointSampler.h>
#include <plugins/Flexible/shapeFunction/BarycentricShapeFunction.h>
#include <plugins/Flexible/deformationMapping/LinearMapping.h>
#include <plugins/Flexible/strainMapping/PrincipalStretchesMapping.h>
#include <plugins/Flexible/material/StabilizedNeoHookeanForceField.h>

#include <sofa/component/collision/BaseContactMapper.h>
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
#include <sofa/component/engine/BoxROI.h>
#include <sofa/component/linearsolver/CGLinearSolver.h>
#include <sofa/component/mapping/BarycentricMappingRigid.h>
#include <sofa/component/misc/RequiredPlugin.h>
#include <sofa/component/misc/VTKExporter.h>
#include <sofa/component/odesolver/EulerImplicitSolver.h>
#include <sofa/component/odesolver/EulerSolver.h>
#include <sofa/component/projectiveconstraintset/FixedConstraint.h>
#include <sofa/component/projectiveconstraintset/SkeletalMotionConstraint.h>
#include <sofa/component/topology/MeshTopology.h>
#include <sofa/component/typedef/Sofa_typedef.h>
#include <sofa/gui/GUIManager.h>
#include <sofa/gui/Main.h>
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


// BenderVTK includes
#include <vtkDualQuaternion.h>

// VTK includes
#include <vtkCellArray.h>
#include <vtkCellCenters.h>
#include <vtkCellData.h>
#include <vtkCellDataToPointData.h>
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
using namespace sofa::component::engine;
using namespace sofa::component::forcefield;
using namespace sofa::component::linearsolver;
using namespace sofa::component::mapping;
using namespace sofa::component::odesolver;
using namespace sofa::component::projectiveconstraintset;
using namespace sofa::component::topology;
using namespace sofa::component::visualmodel;
using namespace sofa::component::shapefunction;
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

// ---------------------------------------------------------------------
bool isCellInLabel(vtkIdType cellId, vtkIntArray* materialIds, int filterLabel)
{
  return materialIds->GetValue(cellId) == filterLabel;
}

// ---------------------------------------------------------------------
bool isPointInLabel(vtkIdList* cellIdList, vtkIntArray* materialIds, int filterLabel)
{
  // Search if the point belongs to a cell that has the material Id filterLabel.
  for(vtkIdType c = 0; c < cellIdList->GetNumberOfIds(); ++c)
    {
    if (materialIds->GetValue(cellIdList->GetId(c)) == filterLabel)
      {
      return true;
      }
    }
  return false;
}

// ---------------------------------------------------------------------
bool isPointInLabel(vtkPolyData* polyMesh, int filterLabel, vtkIdType pointId)
{
  vtkIntArray* materialIds = vtkIntArray::SafeDownCast(
    polyMesh->GetCellData()->GetArray("MaterialId"));
  vtkSmartPointer<vtkIdList> cellIdList =
    vtkSmartPointer<vtkIdList>::New();
  polyMesh->GetPointCells(pointId, cellIdList);
  return isPointInLabel(cellIdList, materialIds, filterLabel);
}

// ---------------------------------------------------------------------
vtkIdType countNumberOfPointsInLabel(
  vtkPolyData* polyMesh, int filterLabel)
{
  vtkIdType count = 0;

  vtkPoints* points = polyMesh->GetPoints();
  vtkIntArray* materialIds = vtkIntArray::SafeDownCast(
    polyMesh->GetCellData()->GetArray("MaterialId"));
  if (!points || !materialIds)
    {
    std::cerr << "No material ids" << std::endl;
    return count;
    }
  const vtkIdType numberOfPoints = points->GetNumberOfPoints();
  vtkSmartPointer<vtkIdList> cellIdList =
    vtkSmartPointer<vtkIdList>::New();
  for (vtkIdType i = 0; i < numberOfPoints; ++i)
    {
    polyMesh->GetPointCells(i, cellIdList);
    if (isPointInLabel(cellIdList, materialIds, filterLabel))
      {
      ++count;
      }
    }
  return count;
}


// Copy point positions from vtk to a mechanical object
// ---------------------------------------------------------------------
std::map<vtkIdType, vtkIdType> copyVertices( vtkPoints* points,
                                             MechanicalObject<Vec3Types>* mechanicalMesh,
                                             int filter = 0, int label = 0,
                                             vtkPolyData* polyMesh = 0)
{
  std::map<vtkIdType,vtkIdType> m;
  vtkIdType numberOfPoints = points->GetNumberOfPoints();
  if (filter > 0 && polyMesh != 0)
    {
    vtkIdType numberOfPointWithLabel =
      countNumberOfPointsInLabel(polyMesh, label);
    numberOfPoints = (filter == 2) ? numberOfPoints - numberOfPointWithLabel :
      numberOfPointWithLabel;
    }
  vtkIdType meshPointId = mechanicalMesh->getSize() > 1 ? mechanicalMesh->getSize() : 0;
  mechanicalMesh->resize(numberOfPoints);

  std::cout << "  Total # of points ";
  if (filter > 0)
    {
    std::cout << "for label " << label;
    }
  std::cout  <<  ": " << numberOfPoints;
  if (filter > 0)
    {
    std::cout << " out of " << points->GetNumberOfPoints();
    }
  std::cout << " points." << std::endl;

  Data<MechanicalObject<Vec3Types>::VecCoord>* x =
    mechanicalMesh->write(VecCoordId::position());

  // Copy vertices from vtk mesh
  MechanicalObject<Vec3Types>::VecCoord &vertices = *x->beginEdit();

  for(vtkIdType i = 0, end = points->GetNumberOfPoints(); i < end; ++i)
    {
    if ((filter == 1 && !isPointInLabel(polyMesh, label, i)) ||
        (filter == 2 && isPointInLabel(polyMesh, label, i)) )
      {
      continue;
      }
    Vector3 point;
    points->GetPoint(i,point.ptr());
    m[i] = meshPointId;
    vertices[meshPointId++] = point;
//     std::cout << "vertex[" << i << "] = " << vertices[i] << std::endl;
    }
  if (meshPointId != numberOfPoints)
    {
    std::cerr << "Failed to copy vertices: " << numberOfPoints << " vs "
              << meshPointId << std::endl;
    }
  x->endEdit();
  return m;
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
                        const std::vector<std::string> &elements
                        )
{
  double stiffness = 20.;//10.; // 30.
  double friction = 0.;
  double proximity = 0.9;
  double restitution = 0.0;
  for (size_t i=0; i < elements.size(); ++i)
    {
    if (elements[i] == "Triangle")
      {
      TriangleModelInRegularGrid::SPtr triModel = addNew<TriangleModelInRegularGrid>(collisionNode,
        "TriangleCollision");
      triModel->bothSide.setValue(false);
      triModel->setSelfCollision(true);
      triModel->setContactStiffness(stiffness);
      triModel->setContactFriction(friction);
      triModel->setContactRestitution(restitution);
      triModel->setProximity(proximity);
      }
    else if(elements[i] == "Line")
      {
      LineModel::SPtr lineModel = addNew<LineModel>(collisionNode,
        "LineCollision");
      lineModel->bothSide.setValue(false);
      lineModel->setSelfCollision(true);
      lineModel->setContactStiffness(stiffness);
      lineModel->setContactFriction(friction);
      lineModel->setContactRestitution(restitution);
      lineModel->setProximity(proximity);
      }
    else if (elements[i] == "Point")
      {
      PointModel::SPtr pointModel = addNew<PointModel>(collisionNode,
        "PointCollision");
      pointModel->bothSide.setValue(false);
      pointModel->setSelfCollision(true);
      pointModel->setContactStiffness(stiffness);
      pointModel->setContactFriction(friction);
      pointModel->setContactRestitution(restitution);
      pointModel->setProximity(proximity);
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
  //   typedef LocalMinDistance ProximityIntersectionType;
  typedef MinProximityIntersection ProximityIntersectionType;
  Node::SPtr root = getSimulation()->createNewGraph("root");

  //Components for collision management
  //------------------------------------
  //--> adding collision pipeline
  sofa::component::misc::RequiredPlugin::SPtr ldicollisionPlugin =
  addNew<sofa::component::misc::RequiredPlugin>(root,"LDI Detection");
  ldicollisionPlugin->pluginName.setValue("ldidetection");

  LayeredDepthImagesPipeline::SPtr collisionPipeline =
    addNew<LayeredDepthImagesPipeline>(root,"Collision Pipeline");
  collisionPipeline->Kselfpressure.setValue(10000);
  collisionPipeline->Kpressure.setValue(20);
  collisionPipeline->resolution.setValue(256);
  collisionPipeline->resolutionPixel.setValue(20);
  collisionPipeline->depthBB.setValue(8);
  collisionPipeline->bSelfCollision.setValue(true);

  //--> adding collision detection system
  addNew<LDIDetection>(root,"LDIDetection");

  //--> adding collision detection system
  addNew<BruteForceDetection>(root,"Detection");

  //--> adding contact manager
  addNew<DefaultContactManager>(root,"Contact Manager");

  //--> adding component to detection intersection of elements
  ProximityIntersectionType::SPtr detectionProximity = addNew<ProximityIntersectionType>(root,"Proximity");
  detectionProximity->setAlarmDistance(0.5);     //warning distance
  detectionProximity->setContactDistance(0.2);   //min distance before setting a spring to create a repulsion

  //--> adding component to handle groups of collision.
  addNew<DefaultCollisionGroupManager>(root,"Collision Group Manager");

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
template <class T>
void getBoneCoordinates(
  vtkPolyData* armature,
  sofa::helper::vector<SkeletonJoint<T> >& skeletonJoints,
  sofa::helper::vector<SkeletonBone>& skeletonBones,
  sofa::helper::vector<typename T::Coord>& restCoordinates,
  bool invertXY = true)
{
  vtkCellArray* armatureSegments = armature->GetLines();
  vtkCellData*  armatureCellData = armature->GetCellData();

  vtkPoints* points = armature->GetPoints();

  std::cout << "Number of bones: " << armatureSegments->GetNumberOfCells() <<
    std::endl;

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

    typename T::Coord finalPose,restPosition;
    Vector3            centerOfMass = 0.5*(childJoint+parentJoint);

    //vtkQuaterniond q = computeOrientationFromReferenceAxis(centerOfMass,
    //  childJoint);

    //restPosition.getCenter()      = centerOfMass;
    restPosition = centerOfMass;
    //restPosition.getOrientation() = Quat3(q.GetX(),q.GetY(),q.GetZ(),q.GetW());
    restCoordinates.push_back(restPosition);

    //finalPose.getCenter() = rotation*
    //                        (centerOfMass-parentJoint)+parentJoint+translation;
    finalPose = rotation* (centerOfMass-parentJoint)+parentJoint+translation;

    //Matrix3 orientation;
    //restPosition.getOrientation().toMatrix(orientation);
    //finalPose.getOrientation().fromMatrix(rotation*orientation);

    skeletonJoints.push_back(SkeletonJoint<T>());
    SkeletonJoint<T>& skeletonJoint = skeletonJoints.back();
    skeletonJoint.addChannel(restPosition, 0.);
    skeletonJoint.addChannel(finalPose, 1.);
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

//-------------------------------------------------------------------------------
template<class T>
inline void InvertXY(T& x)
{
  x[0]*=-1;
  x[1]*=-1;
}
typedef itk::Vector<double,3> Vec3;
typedef itk::Matrix<double,3,3> Mat33;
//-------------------------------------------------------------------------------
Mat33 ToItkMatrix(double M[3][3])
{
  Mat33 itkM;
  for (int i=0; i<3; ++i)
    {
    for (int j=0; j<3; ++j)
      {
      itkM(i,j)  = M[i][j];
      }
    }

  return itkM;
}

//-------------------------------------------------------------------------------
struct RigidTransform
{
  Vec3 O;
  Vec3 T;
  Mat33 R;
  //vtkQuaternion<double> R; //rotation quarternion

  RigidTransform()
  {
    //initialize to identity transform
    T[0] = T[1] = T[2] = 0.0;

    R.SetIdentity();

    O[0]=O[1]=O[2]=0.0;
  }

  void SetRotation(double* M)
  {
    this->R = ToItkMatrix((double (*)[3])M);
  }

  void SetRotationCenter(const double* center)
  {
    this->O = Vec3(center);
  }
  void GetRotationCenter(double* center)
  {
    center[0] = this->O[0];
    center[1] = this->O[1];
    center[2] = this->O[2];
  }

  void SetTranslation(const double* t)
  {
    this->T = Vec3(t);
  }
  void GetTranslation(double* t)const
  {
    t[0] = this->T[0];
    t[1] = this->T[1];
    t[2] = this->T[2];
  }
  void GetTranslationComponent(double* tc)const
  {
    Vec3 res = this->R*(-this->O) + this->O +T;
    tc[0] = res[0];
    tc[1] = res[1];
    tc[2] = res[2];
  }
  void Apply(const double in[3], double out[3]) const
  {
    Vec3 x(in);
    x = this->R*(x-this->O) + this->O +T;
    out[0] =x[0];
    out[1] =x[1];
    out[2] =x[2];
  }
};

//-------------------------------------------------------------------------------
void GetArmatureTransform(vtkPolyData* polyData, vtkIdType cellId,
                          const char* arrayName, const double* rcenter,
                          RigidTransform& F,bool invertXY = true)
{
  double A[12];
  polyData->GetCellData()->GetArray(arrayName)->GetTuple(cellId, A);

  double R[3][3];
  double T[3];
  double RCenter[3];
  int iA(0);
  for (int i=0; i<3; ++i)
    {
    for (int j=0; j<3; ++j,++iA)
      {
      R[j][i] = A[iA];
      }
    }

  for (int i=0; i<3; ++i)
    {
    T[i] = A[i+9];
    RCenter[i] = rcenter[i];
    }

  if(invertXY)
    {
    for (int i=0; i<3; ++i)
      {
      for (int j=0; j<3; ++j)
        {
        if( (i>1 || j>1) && i!=j)
          {
          R[i][j]*=-1;
          }
        }
      }
    InvertXY(T);
    }

  F.SetRotation(&R[0][0]);
  F.SetRotationCenter(RCenter);
  // Translation between the head in rest and head in pose (translated because of its parent).
  F.SetTranslation(T);
}

//-------------------------------------------------------------------------------
bool WIComp(const std::pair<double, int>& left, const std::pair<double, int>& right)
{
  return left.first > right.first;
}

//-------------------------------------------------------------------------------
vtkPoints* poseMesh(vtkPolyData* mesh, vtkPolyData* armature,
                    bool invertXY = true, double coef = 1.,
                    bool verbose = true)
{
  coef = std::min (1., std::max(0., coef));
  if (verbose)
    {
    std::cout << "Pose mesh for frame: " << coef << std::endl;
    std::cout << "  Number of mesh points: " << mesh->GetNumberOfPoints() << std::endl;
    }
  vtkPoints* outPoints = vtkPoints::New();
  outPoints->Allocate(mesh->GetNumberOfPoints());
  outPoints->SetNumberOfPoints(mesh->GetNumberOfPoints());

  //----------------------------
  // Read armature
  //----------------------------
  std::vector<RigidTransform> transforms;

  vtkCellArray* armatureSegments = armature->GetLines();
  vtkCellData* armatureCellData = armature->GetCellData();
  vtkNew<vtkIdList> cell;
  armatureSegments->InitTraversal();
  int edgeId = 0;
  if (!armatureCellData->GetArray("Transforms"))
    {
    std::cerr << "  No 'Transforms' cell array in armature" << std::endl;
    }
  else if (verbose)
    {
    std::cout << "  # components: "
      << armatureCellData->GetArray("Transforms")->GetNumberOfComponents()
      << std::endl;
    }

  while(armatureSegments->GetNextCell(cell.GetPointer()))
    {
    vtkIdType a = cell->GetId(0);
    vtkIdType b = cell->GetId(1);

    double ax[3], bx[3];
    armature->GetPoints()->GetPoint(a, ax);
    armature->GetPoints()->GetPoint(b, bx);

    RigidTransform transform;
    GetArmatureTransform(armature, edgeId, "Transforms", ax, transform, invertXY);
    transforms.push_back(transform);
    ++edgeId;
    }

  size_t numSites = transforms.size();

  std::vector<vtkDualQuaternion<double> > dqs;
  for (size_t i = 0; i < numSites; ++i)
    {
    RigidTransform& trans = transforms[i];
    vtkQuaternion<double> rotation;
    rotation.FromMatrix3x3((double (*)[3])&trans.R(0,0));
    double tc[3];
    trans.GetTranslationComponent(tc);
    vtkDualQuaternion<double> dq;
    dq.SetRotationTranslation(rotation, &tc[0]);
    dqs.push_back(dq);
    }

  if (verbose)
    {
    std::cout<<"  Read "<<numSites<<" transforms"<<std::endl;
    }

  //------------------------------------------------------
  // Get the weights
  //------------------------------------------------------

  // Find out if all the weight have a corresponding array
  vtkPointData* pointData = mesh->GetPointData();
  vtkPoints* inputPoints = mesh->GetPoints();
  int numPoints = mesh->GetNumberOfPoints();

  std::vector<vtkFloatArray*> surfaceVertexWeights;
  if (verbose)
    {
    std::cout<<"Trying to use the " << numSites
             << " weight field data" << std::endl;
    }

  size_t numWeights = numSites;
  for (size_t i = 0; i < numSites; ++i)
    {
    vtkFloatArray* weightArray =
      vtkFloatArray::SafeDownCast(pointData->GetArray(i));

    if (!weightArray || weightArray->GetNumberOfTuples() != numPoints)
      {
      //surfaceVertexWeights.clear();
      numWeights = i;

      std::cerr<<"Could not find field array for weight " << i << std::endl;
      break;
      }
    else
      {
      surfaceVertexWeights.push_back(weightArray);
      }
    }

  const size_t MaximumNumberOfInterpolatedBones = 4;
  // This property controls whether to interpolate with ScLerp
  // (Screw Linear interpolation) or DLB (Dual Quaternion Linear
  // Blending).
  // Note that DLB (faster) is not tweaked to give proper results.
  const bool UseScLerp = true;

  if (verbose)
    {
    std::cout << "  Pose " << numPoints << "points" << std::endl;
    }

  //----------------------------
  // Pose
  //----------------------------
  for (vtkIdType pi = 0; pi < numPoints; ++pi)
    {

    double xraw[3];
    inputPoints->GetPoint(pi,xraw);

    double wSum = 0.0;
    for (size_t i = 0; i < numWeights; ++i)
      {
      wSum += surfaceVertexWeights[i]->GetValue(pi);
      }

    Vec3 y(0.0);
    if (wSum <= 0.0) // shortcut
      {
      y = xraw;
      }
    else
      {
      bool LinearBlend = false;
      if (LinearBlend)
        {
        for (size_t i = 0; i < numWeights; ++i)
          {
          double w = surfaceVertexWeights[i]->GetValue(pi) / wSum;
          double yi[3];
          const vtkDualQuaternion<double>& transform(dqs[i]);
          transform.TransformPoint(xraw, yi);
          vtkDualQuaternion<double> t = transform * coef;
          t.TransformPoint(xraw, yi);
          y += w*Vec3(yi);
          }
        }
      else
        {
        std::vector<std::pair<double, int> > ws;
        for (size_t i=0; i < numWeights; ++i)
          {
          double w = surfaceVertexWeights[i]->GetValue(pi) / wSum;
          ws.push_back(std::make_pair(w, i));
          }
        // To limit computation errors, it is important to start interpolating with the
        // highest w first.
        std::partial_sort(ws.begin(),
                          ws.begin() + MaximumNumberOfInterpolatedBones,
                          ws.end(),
                          WIComp);
        vtkDualQuaternion<double> transform = dqs[ws[0].second];
        double w = ws[0].first;
        // Warning, Sclerp is only meant to blend 2 DualQuaternions, I'm not
        // sure it works with more than 2.
        for (size_t i=1; i < MaximumNumberOfInterpolatedBones; ++i)
          {
          double w2 = ws[i].first;
          int i2 = ws[i].second;
          vtkDualQuaternion<double> dq;
          if (UseScLerp)
            {
            dq = transform.ScLerp2(w2 / (w + w2), dqs[i2]);
            // vtkDualQuaternion<double> dq2 = dqs[i2].ScLerp2(w/ (w + w2), dq);
            // vtkDualQuaternion<double> dq3 = dq.ScLerp(w2/ (w + w2), dqs[i2]);
            // vtkDualQuaternion<double> dq4 = dqs[i2].ScLerp(w/ (w + w2), dq);
            }
          else
            {
            dq = transform.Lerp(w2 / (w + w2), dqs[i2]);
            }
          transform = dq;
          w += w2;
          }
        transform = transform * coef;
        transform.TransformPoint(xraw, &y[0]);
        }
      }

    //bool IsSurfaceInRAS = invertXY;
    //if (!IsSurfaceInRAS)
    //  {
      //InvertXY(y);
    //  }
    outPoints->SetPoint(pi,y[0],y[1],y[2]);
    }
  if (verbose)
    {
    std::cout << "  outPoints: " << outPoints << " -> "
              << outPoints->GetNumberOfPoints() << std::endl;
    }
  return outPoints;
}

//------------------------------------------------------------------------------
void poseMechanicalObject(
  MechanicalObject<Vec3Types>::SPtr mechanicalObject,
  vtkPolyData* mesh,
  vtkPolyData* armature, bool invertArmatureXY,
  double coef = 1.0)
{
  vtkPoints* posedPoints = poseMesh(mesh, armature, invertArmatureXY,
                                    coef, /*verbose= */false);

  size_t numberOfPoints = posedPoints->GetNumberOfPoints();
  Data<MechanicalObject<Vec3Types>::VecCoord> *x =
    mechanicalObject->write(VecCoordId::position());

  MechanicalObject<Vec3Types>::VecCoord &vertices = *x->beginEdit();

  for(size_t i = 0; i < numberOfPoints; ++i)
    {
    Vector3 point;
    posedPoints->GetPoint(i, point.ptr());
    vertices[i] = point;
    }
  x->endEdit();
  posedPoints->Delete();
}

// ---------------------------------------------------------------------
MechanicalObject<Vec3Types>::SPtr createGhostMesh(
  Node *       parentNode,
  vtkPolyData *armature,
  vtkPolyData* mesh,
  Vector6 &box,
  bool         invertXY = true,
  double frame = 1.
  )
{

  vtkPoints* posedPoints = poseMesh(mesh, armature, invertXY, frame);
  MechanicalObject<Vec3Types>::SPtr ghostMesh =
    addNew<MechanicalObject<Vec3Types> >(parentNode, "ghostMesh");

  // Get bone positions

  size_t numberOfPoints = posedPoints->GetNumberOfPoints();
  std::cout << "Number of points: " << numberOfPoints << std::endl;

  ghostMesh->resize(numberOfPoints);
  Data<MechanicalObject<Vec3Types>::VecCoord> *x =
    ghostMesh->write(VecCoordId::position());

  MechanicalObject<Vec3Types>::VecCoord &vertices = *x->beginEdit();
  for(size_t i = 0; i < numberOfPoints; ++i)
    {
    Vector3 point;
    posedPoints->GetPoint(i, point.ptr());
    vertices[i] = point;
    }
  x->endEdit();

  UniformMass3::SPtr frameMass = addNew<UniformMass3>(parentNode,"FrameMass");
  frameMass->setTotalMass(1);

  posedPoints->ComputeBounds();
  double bounds[6];
  posedPoints->GetBounds(bounds);

  box[0] = bounds[0];
  box[1] = bounds[2];
  box[2] = bounds[4];
  box[3] = bounds[1];
  box[4] = bounds[3];
  box[5] = bounds[5];

  posedPoints->Delete();
  return ghostMesh;
}


/// Create a FEM in parentNode.  A MeshTopology should be defined in
/// parentNode prior to calling this function.
// ---------------------------------------------------------------------
void createFiniteElementModel(Node* parentNode, bool linearFEM,
                              Vec3Types::VecReal &youngModulus )
{
  if (linearFEM)
    {
    TetrahedronFEMForceField< Vec3Types >::SPtr femSolver =
      addNew<TetrahedronFEMForceField< Vec3Types > >(parentNode,"femSolver");
    femSolver->setComputeGlobalMatrix(false);
    femSolver->setMethod("large");
    femSolver->setPoissonRatio(.4);
    femSolver->_youngModulus.setValue(youngModulus);
    return;
    }

  Node::SPtr behavior = parentNode->createChild("behavior");
  MechanicalObject<Vec3Types> *tetMesh =
    dynamic_cast<MechanicalObject<Vec3Types>*>( parentNode->getMechanicalState());

  // Define control nodes
  BarycentricShapeFunction<ShapeFunction3>::SPtr shapeFunction =
    addNew<BarycentricShapeFunction<ShapeFunction3> >(parentNode,"shapeFunction");

  // Sample mesh where the deformation gradients re going to be defined
  TopologyGaussPointSampler::SPtr sampler = addNew<TopologyGaussPointSampler>(behavior,"sampler");
  sampler->f_inPosition.setParent(&tetMesh->x);

  MechanicalObject<F331Types>::SPtr F = addNew<MechanicalObject<F331Types> >(behavior,"F");

  // Map mesh to sampled nodes
  LinearMapping<Vec3Types,F331Types>::SPtr linearMapping =
    addNew<LinearMapping<Vec3Types,F331Types> >(behavior,"linearMapping");
  linearMapping->setModels(tetMesh,F.get());

  // Create strain measurements
  Node::SPtr strainNode = behavior->createChild("strain");
    {
    MechanicalObject<U331Types>::SPtr U = addNew<MechanicalObject<U331Types> >(strainNode,"U");
    PrincipalStretchesMapping<F331Types,U331Types>::SPtr principalMapping =
      addNew<PrincipalStretchesMapping<F331Types,U331Types> >(strainNode,"principalMapping");
    principalMapping->threshold.setValue(0.6);
    principalMapping->asStrain.setValue(false);
    principalMapping->setModels(F.get(),U.get());

    StabilizedNeoHookeanForceField<U331Types>::SPtr forceField =
      addNew<StabilizedNeoHookeanForceField<U331Types> >(strainNode,"Force Field");

    Vec3Types::VecReal &modulus = *forceField->_youngModulus.beginEdit();
    modulus = youngModulus;
    forceField->_youngModulus.endEdit();
    Vec3Types::VecReal &poisson = *forceField->_poissonRatio.beginEdit();
    poisson[0] = 0.3;
    forceField->_poissonRatio.endEdit();
    }
}

// ---------------------------------------------------------------------
MechanicalObject<Vec3Types>::SPtr loadBoneMesh(Node*               parentNode,
                                               vtkPolyData *       polyMesh,
                                               int                 boneLabel = 209)
{
  // load mesh
  vtkSmartPointer<vtkPoints>    points;
  vtkSmartPointer<vtkCellArray> tetras;
  vtkSmartPointer<vtkCellData>  data;

  points = polyMesh->GetPoints();
  tetras = polyMesh->GetPolys();
  data   = polyMesh->GetCellData();

  std::stringstream meshName;
  meshName << "BoneMesh";

  // Create mechanical object (dof) for the mesh and extract material parameters
  MechanicalObject<Vec3Types>::SPtr mechanicalMesh =
    addNew<MechanicalObject<Vec3Types> >(parentNode,meshName.str());

  std::map<vtkIdType, vtkIdType> m =
    copyVertices(points.GetPointer(),mechanicalMesh.get(), /*filter =*/1, boneLabel, polyMesh);

  // Create the MeshTopology
  MeshTopology::SPtr meshTopology = addNew<MeshTopology>(parentNode,"BoneTopology");
  meshTopology->seqPoints.setParent(&mechanicalMesh->x);

  // Copy tetrahedra array from vtk cell array
  MeshTopology::SeqTetrahedra& tetrahedra =
    *meshTopology->seqTetrahedra.beginEdit();
  // \fixme needed ?
  //tetrahedra.reserve(tetras->GetNumberOfCells());

  vtkIntArray* materialIds = vtkIntArray::SafeDownCast(
    polyMesh->GetCellData()->GetArray("MaterialId"));

  tetras->InitTraversal();

  vtkNew<vtkIdList> element;
  vtkIdType cellCount = 0;
  for (vtkIdType cellId = 0; tetras->GetNextCell(element.GetPointer());++cellId)
    {
    if(element->GetNumberOfIds() != 4)
      {
      std::cerr << "Error: Non-tetrahedron encountered." << std::endl;
      continue;
      }
    if (!isCellInLabel(cellId, materialIds, boneLabel))
      {
      continue;
      }
    tetrahedra.push_back(MeshTopology::Tetra(m[element->GetId(0)],
                                             m[element->GetId(1)],
                                             m[element->GetId(2)],
                                             m[element->GetId(3)]));
    ++cellCount;
    }
  meshTopology->seqTetrahedra.endEdit();

  std::cout << "Total # of bone tetrahedra: " << cellCount
            << " over " << tetras->GetNumberOfCells() << " cells" << std::endl;
  return mechanicalMesh;
}


/// Loads a vtk tetrahedral polymesh and creates a mechanical object and
/// the corresponding MeshTopology.
// ---------------------------------------------------------------------
MechanicalObject<Vec3Types>::SPtr loadMesh(Node*               parentNode,
                                           vtkPolyData *       polyMesh,
                                           Vec3Types::VecReal &youngModulus,
                                           int                 boneLabel = -1
                                           )
{
  // load mesh
  vtkSmartPointer<vtkPoints>    points;
  vtkSmartPointer<vtkCellArray> tetras;
  vtkSmartPointer<vtkCellData>  data;

  points = polyMesh->GetPoints();
  tetras = polyMesh->GetPolys();
  data   = polyMesh->GetCellData();

  std::stringstream meshName;
  meshName << "Mesh";

  // Create mechanical object (dof) for the mesh and extract material parameters
  MechanicalObject<Vec3Types>::SPtr mechanicalMesh =
    addNew<MechanicalObject<Vec3Types> >(parentNode,meshName.str());

  std::map<vtkIdType, vtkIdType> skinMap =
    copyVertices(points.GetPointer(),mechanicalMesh.get(),
                 /*filter=*/ boneLabel >= 0 ? 2 : 0, boneLabel, polyMesh);

  // Create the MeshTopology
  MeshTopology::SPtr meshTopology = addNew<MeshTopology>(parentNode, "Topology");
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
  for (vtkIdType cellId = 0; tetras->GetNextCell(element.GetPointer());++cellId)
    {
    if(element->GetNumberOfIds() != 4)
      {
      std::cerr << "Error: Non-tetrahedron encountered." << std::endl;
      continue;
      }

    tetrahedra.push_back(MeshTopology::Tetra(element->GetId(0), element->GetId(1), element->GetId(2), element->GetId(3)));
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

// ---------------------------------------------------------------------
void skinBoneMesh(Node *                              parentNode,
                  MechanicalObject<Rigid3Types>::SPtr articulatedFrame,
                  MechanicalObject<Vec3Types>::SPtr   mechanicalObject,
                  vtkPolyData *                       armature,
                  vtkPolyData *                       polyMesh,
                  int                                 boneLabel = 203
)
{
  typedef SkinningMapping<Rigid3Types, Vec3Types> SkinningMappingType;

  vtkSmartPointer<vtkPoints>    points;
  vtkSmartPointer<vtkPointData> pointData;

  points = polyMesh->GetPoints();
  pointData = polyMesh->GetPointData();

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

  vtkIdType numberOfPoints = points->GetNumberOfPoints();
  vtkIdType numberOfBonePoints = countNumberOfPointsInLabel(polyMesh, boneLabel);
  indices.resize(numberOfBonePoints);
  weights.resize(numberOfBonePoints);
  nbIds.resize(numberOfBonePoints,0);
  weightSum.resize(numberOfBonePoints, 0.);

  vtkIntArray* materialIds = vtkIntArray::SafeDownCast(
    polyMesh->GetCellData()->GetArray("MaterialId"));
  std::cout << "Material IDS: " << materialIds << std::endl;

  vtkSmartPointer<vtkIdList> cellIdList =
    vtkSmartPointer<vtkIdList>::New();
  for(vtkIdType boneId = 0; boneId < numberOfBones; ++boneId)
    {
    vtkFloatArray *weightArray = vtkFloatArray::SafeDownCast(pointData->GetArray(boneId));
    if( !weightArray || weightArray->GetNumberOfTuples() != numberOfPoints)
      {
      std::cerr << "Error extracting weight array" << boneId << "." << std::endl;
      continue;
      }

    vtkIdType meshPointId = 0;
    for (vtkIdType pointId = 0; pointId < numberOfPoints; ++pointId)
      {
      polyMesh->GetPointCells(pointId, cellIdList);
      if (!isPointInLabel(cellIdList, materialIds, boneLabel))
        {
        continue;
        }

      float weight = weightArray->GetValue(pointId);
      if (weight < 0.001)
        {
        //++meshPointId;
        //continue;
        }
      weights[meshPointId].push_back(weight);
      indices[meshPointId].push_back(boneId);
      ++nbIds[meshPointId];
      weightSum[meshPointId] += weight;
      ++meshPointId;
      }
    if (meshPointId != numberOfBonePoints)
      {
      std::cerr << "Mismatch between the number of bones and the number of points" << std::endl;
      }
    }

  // Make sure each vertex has at least one valid associated weight
  // TODO: Normalize weights -> weights[i][*]/weightSum[i]
  int weightErrorCount = 0;
  for(size_t i = 0; i < weightSum.size(); ++i)
    {
    if(weightSum[i] < 0.0001)
      {
      //indices[i].push_back(0);
      //weights[i].push_back(0.);

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
  vtkSmartPointer<vtkPointData> pointData;

  if(label != 0)
    {
    vtkSmartPointer<vtkThreshold> meshThreshold = vtkSmartPointer<vtkThreshold>::New();
    meshThreshold->SetInput(polyMesh);
    meshThreshold->ThresholdBetween(label,label);
    meshThreshold->Update();

    vtkUnstructuredGrid *mesh = meshThreshold->GetOutput();
    points = mesh->GetPoints();
    pointData   = mesh->GetPointData();
    }
  else
    {
    points = polyMesh->GetPoints();
    pointData = polyMesh->GetPointData();
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

  vtkIntArray* materialIds = vtkIntArray::SafeDownCast(polyMesh->GetCellData()->GetArray("MaterialId"));
  std::cout << "Material IDS: " << materialIds << std::endl;

  vtkSmartPointer<vtkIdList> cellIdList =
    vtkSmartPointer<vtkIdList>::New();
  for(vtkIdType i = 0; i < numberOfBones; ++i)
    {
    vtkFloatArray *weightArray = vtkFloatArray::SafeDownCast(pointData->GetArray(i));
    if( !weightArray || weightArray->GetNumberOfTuples() != numberOfVertices)
      {
      std::cerr << "Error extracting weight array." << std::endl;
      continue;
      }

    for (vtkIdType j = 0; j < numberOfVertices; ++j)
      {
      polyMesh->GetPointCells(j, cellIdList);
      bool hasWeight = false;
      for(vtkIdType c = 0; c < cellIdList->GetNumberOfIds(); ++c)
        {
        if (materialIds->GetValue(cellIdList->GetId(c)) != 1)
          {
          hasWeight = true;
          }
        }
      if (!hasWeight)
        {
        //continue;
        }

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
      //indices[i].push_back(0);
      //weights[i].push_back(0.);

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
                               bool createCollisionSurface = true
                               )
{
  std::cout << "Creating collision node..." << std::endl;

  std::vector<std::string> modelTypes;
  modelTypes.push_back("Triangle");
  modelTypes.push_back("Line");
  modelTypes.push_back("Point");

  //Node::SPtr collisionNode(parentNode, false);//
  Node::SPtr collisionNode = parentNode;
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
    vtkPoints* points = 0;
    vtkCellArray* triangles =0;
    vtkSmartPointer<vtkThreshold> meshThreshold;
    vtkSmartPointer<vtkTriangleFilter> extractTriangles;
    if(label != 0)
      {
      meshThreshold = vtkSmartPointer<vtkThreshold>::New();
      meshThreshold->SetInput(polyMesh);
      meshThreshold->ThresholdBetween(label,label);
      meshThreshold->Update();

      vtkUnstructuredGrid *mesh = meshThreshold->GetOutput();
      points    = mesh->GetPoints();
      triangles = mesh->GetCells();
      }
    else
      {
      extractTriangles = vtkSmartPointer<vtkTriangleFilter>::New();
      extractTriangles->SetInput(polyMesh);
      extractTriangles->Update();

      points    = extractTriangles->GetOutput()->GetPoints();
      triangles = extractTriangles->GetOutput()->GetPolys();
      }

    std::stringstream meshName;
    meshName << "SurfaceMesh" << label;

    // Create mechanical object for the mesh vertices
    MechanicalObject<Vec3Types>::SPtr surfaceMesh =
      addNew<MechanicalObject<Vec3Types> >(collisionNode,meshName.str());

    copyVertices(points,surfaceMesh.get());

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
    vtkIdType* element;
    vtkIdType elementSize;

    for(vtkIdType cellId = 0; triangles->GetNextCell(elementSize, element); ++cellId)
      {
      if(elementSize != 3)
        {
        std::cerr << " Error: Non-triangle encountered at cell "
                  << cellId << "." << std::endl;
        continue;
        }
      cellId++;
      MeshTopology::Triangle t(element[0], element[1], element[2]);
      triangleArray.push_back(t);
      }

    // Use a barycentric mapping to map surface to volume mesh
    BarycentricMapping3_to_3::SPtr mechMapping =
      addNew<BarycentricMapping3_to_3>(
        collisionNode,"collisionMapping");
    mechMapping->setModels(volumeMesh, surfaceMesh.get());
    }
  addCollisionModels(collisionNode,modelTypes);
  //sofa::helper::vector< BaseNode* > parents = collisionNode->getParents();
  //for (vtkIdType i=0; i < parents.size(); ++i)
  //  {
  //  std::cout << "Parent " << i << ": " << parents[i]->name.getValue() << std::endl;
  //  }
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

//------------------------------------------------------------------------------
double meanSquareError(MechanicalObject<Vec3Types>::SPtr mesh1,
                       MechanicalObject<Vec3Types>::SPtr mesh2)
{
  const Data<MechanicalObject<Vec3Types>::VecCoord>* position1 =
    mesh1->read(VecCoordId::position());
  const Data<MechanicalObject<Vec3Types>::VecCoord>* position2 =
    mesh2->read(VecCoordId::position());

  if (!position1 || !position2)
    {
    std::cerr << "No positions: " << position1 << ", " << position2 << std::endl;
    return -1.;
    }
  // Copy vertices from vtk mesh
  const MechanicalObject<Vec3Types>::VecCoord& vertices1 = position1->getValue();
  const MechanicalObject<Vec3Types>::VecCoord& vertices2 = position2->getValue();

  size_t numberOfPoints = vertices1.size();
  if (numberOfPoints != vertices2.size())
    {
    std::cerr << "Not the same number of vertices: "
              << vertices1.size() << " != " << vertices2.size() << std::endl;
    return -1.;
    }

  double error = 0.;
  MechanicalObject<Vec3Types>::VecCoord::const_iterator it1;
  MechanicalObject<Vec3Types>::VecCoord::const_iterator it2;
  for(it1 = vertices1.begin(), it2 = vertices2.begin();
      it1 != vertices1.end();
      ++it1, ++it2)
    {
    Vector3 distance = *it1 - *it2;
    error += distance.norm2() / numberOfPoints;
    }
  return error;
}

//------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
  PARSE_ARGS;

  const double dt = 0.0001;

  sofa::simulation::setSimulation(new sofa::simulation::graph::DAGSimulation());

  // Create the scene graph root node
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

  if (Verbose)
    {
    std::cout << "************************************************************"
              << std::endl;
    std::cout << "Create anatomical map..." << std::endl;
    }

  // Create a constrained articulated frame
  Node::SPtr skeletalNode = sceneNode->createChild("Skeleton");

  if (Verbose)
    {
    std::cout << "************************************************************"
              << std::endl;
    std::cout << "Create ghost mesh..." << std::endl;
    }

  Vector6 box;
  MechanicalObject<Vec3Types>::SPtr ghostMesh =
    createGhostMesh(skeletalNode.get(), armature, tetMesh,box,
                    !IsArmatureInRAS, 1. / NumberOfArmatureSteps );

  // Crete a fix contraint to fix the positions of the posed ghost frame
  BoxROI<Vec3Types>::SPtr boxRoi = addNew<BoxROI<Vec3Types> >(skeletalNode.get(),"BoxRoi");
  sofa::helper::vector<Vector6> &boxes = *boxRoi->boxes.beginEdit();
  boxes.push_back(box);
  boxRoi->boxes.endEdit();

  FixedConstraint<Vec3Types>::SPtr fixedConstraint =
    addNew<FixedConstraint<Vec3Types> >(skeletalNode.get(),"fixedContraint");
  fixedConstraint->f_indices.setParent(&boxRoi->f_indices);

  if (Verbose)
    {
    std::cout << "************************************************************"
              << std::endl;
    std::cout << "Create Anatomical mesh..." << std::endl;
    }

  // Node for the mesh
  Node::SPtr anatomicalNode = sceneNode->createChild("AnatomicalMesh");

  // Create mesh dof
  Vec3Types::VecReal                youngModulus;
  MechanicalObject<Vec3Types>::SPtr posedMesh = loadMesh(
    anatomicalNode.get(), tetMesh, youngModulus);
  UniformMass3::SPtr anatomicalMass = addNew<UniformMass3>(anatomicalNode.get(),"Mass");
  anatomicalMass->setTotalMass(100);

  if (Verbose)
    {
    std::cout << "************************************************************"
              << std::endl;
    std::cout << "Create finite element model..." << std::endl;
    }

  // Finite element method
  createFiniteElementModel(anatomicalNode.get(), LinearFEM, youngModulus);

  // Collision node
  if (EnableCollision)
    {
    if (Verbose)
      {
      std::cout << "************************************************************"
                << std::endl;
      std::cout << "Create collision node..." << std::endl;
      }
    Node::SPtr collisionNode;
    collisionNode = createCollisionNode(anatomicalNode.get(),
                                        surfaceMesh,posedMesh.get());
    }

  if (Verbose)
    {
    std::cout << "************************************************************"
              << std::endl;
    std::cout << "Create spring forces..." << std::endl;
    }
  using sofa::component::interactionforcefield::StiffSpringForceField;

  StiffSpringForceField<Vec3Types>::SPtr stiffspringforcefield =
    sofa::core::objectmodel::New<StiffSpringForceField<Vec3Types> >(ghostMesh.get(),posedMesh.get());
  stiffspringforcefield->setName("Spring-Contact");
  anatomicalNode->addObject(stiffspringforcefield);

  double stiffness = 10000.;
  double distance = 1.;
  const vtkIdType numberOfPoints = tetMesh->GetPoints()->GetNumberOfPoints();
  size_t sample = 0;
  for (vtkIdType pointId = 0; pointId < numberOfPoints; ++pointId)
    {
    if (isPointInLabel(tetMesh, BoneLabel, pointId) && !(sample++ % 1))
      {
      stiffspringforcefield->addSpring(pointId, pointId, stiffness, 0.0, distance);
      }
    }

  const sofa::core::objectmodel::TagSet &tags = posedMesh->getTags();
  for (sofa::core::objectmodel::TagSet::const_iterator it=tags.begin(); it!=tags.end(); ++it)
    stiffspringforcefield->addTag(*it);

  if (Verbose)
    {
    std::cout << "************************************************************"
              << std::endl;
    }
  // Run simulation time steps
  if (Debug)
    {
    std::string sceneFileName = OutputTetMesh;
    sceneFileName += ".scn";
    std::cout << "Write scene at " << sceneFileName << std::endl;
    sofa::simulation::getSimulation()->exportXML(root.get(), sceneFileName.c_str());
    }
  if (Verbose)
    {
    std::cout << "Init..." << std::endl;
    }
  sofa::simulation::getSimulation()->init(root.get());

  int gluArgc  = 1;
  char** gluArgv = new char *;
  gluArgv[0] = new char[strlen(argv[0])+1];
  memcpy(gluArgv[0], argv[0], strlen(argv[0])+1);
  glutInit(&gluArgc, gluArgv);

  if (GUI)
    {
    std::cout << "Open GUI..." << std::endl;
    //
    sofa::gui::initMain();
    sofa::gui::GUIManager::Init(gluArgv[0]);
    root->setAnimate(true);
    int err = sofa::gui::GUIManager::MainLoop(root);
    if (err)
      {
      std::cerr << "Error in SOFA. " << std::endl;
      return err;
      }
    }
  else
    {
    if (Verbose)
      {
      std::cout << "Create OpenGL context..." << std::endl;
      }
    glutCreateWindow(argv[0]);
  //glewExperimental=true;

    root->setAnimate(true);

    if (Verbose)
      {
      std::cout << "Animate..." << std::endl;
      }

    // Forces take time to start moving the mesh
    const size_t minimumNumberOfSteps = 30;

    double lastError = 1.;
    double stdDeviation = 0.;

    // We can't use the distance error directly because the simulation might
    // oscillate.
    for (size_t step = 0;
         (step < minimumNumberOfSteps || stdDeviation > MinimumStandardDeviation) &&
         (step < static_cast<size_t>(MaximumNumberOfSimulationSteps)) ; ++step)
      {
      sofa::simulation::getSimulation()->animate(root.get(), dt);
      //sofa::simulation::getSimulation()->animate(root.get());

      if (step < NumberOfArmatureSteps)
        {
        poseMechanicalObject(ghostMesh, tetMesh, armature, !IsArmatureInRAS,
                             static_cast<double>(step + 2 )/ NumberOfArmatureSteps);
        }

      const double error = meanSquareError(ghostMesh, posedMesh);
      double mean = (lastError + error) / 2.;
      stdDeviation = sqrt((pow(lastError - mean, 2) + pow(error - mean, 2)) / 2.);
      //errorChange =  fabs(lastError-error) / lastError;
      lastError = error;

      if (Verbose)
        {
        std::cout << " Iteration #" << step << " (distance: " << lastError
                    << " std: " << stdDeviation << std::endl;
        }
      }
    }
  vtkNew<vtkPolyData> posedSurface;
  initMesh(posedSurface.GetPointer(), tetMesh, anatomicalNode);
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

  return EXIT_SUCCESS;
}

