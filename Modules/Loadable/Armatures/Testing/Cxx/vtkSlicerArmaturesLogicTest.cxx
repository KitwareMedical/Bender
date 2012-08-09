/*==============================================================================

  Program: 3D Slicer

  Copyright (c) Kitware Inc.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Julien Finet, Kitware Inc.
  and was partially funded by NIH grant 3P41RR013218-12S1

==============================================================================*/

// Armatures includes
#include <vtkSlicerArmaturesLogic.h>
#include <vtkSlicerModelsLogic.h>

// MRML includes
#include <vtkThreeDViewInteractorStyle.h>
#include <vtkMRMLModelNode.h>

// VTK includes
#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkErrorCode.h>
#include <vtkInteractorEventRecorder.h>
#include <vtkMath.h>
#include <vtkMatrix3x3.h>
#include <vtkNew.h>
#include <vtkPolyDataMapper.h>
#include <vtkPNGWriter.h>
#include <vtkRegressionTestImage.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkWindowToImageFilter.h>

const char vtkMRMLVolumeRenderingDisplayableManagerTest1EventLog[] =
"# StreamVersion 1\n";

bool Pose(double tail[3], double res[3],
          std::vector<double*> orientations,
          std::vector<double*> poses);
bool BasicTest();
bool Test1Bone();
bool Test2Bones();

bool Test1();
bool Test2();
bool Test3();

bool Test3Bones();
bool Test3Bones2();
bool Test4Bones();

//----------------------------------------------------------------------------
int vtkSlicerArmaturesLogicTest(int argc, char* argv[])
{
  if (argc < 2)
    {
    std::cout << "Usage: vtkSlicerArmaturesLogicTest path/to/file.arm [-I]"
      << std::endl;
    //return EXIT_FAILURE;
    }

  bool res = true;
  res = BasicTest() && res;
  res = Test1Bone() && res;
  res = Test2Bones() && res;
  res = Test1() && res;
  res = Test2() && res;
  res = Test3() && res;
  res = Test3Bones() && res;
  res = Test3Bones2() && res;
  res = Test4Bones() && res;

  // Renderer, RenderWindow and Interactor
  vtkNew<vtkRenderer> renderer;
  vtkNew<vtkRenderWindow> renderWindow;
  vtkNew<vtkRenderWindowInteractor> renderWindowInteractor;
  renderWindow->SetSize(600, 600);
  renderWindow->SetMultiSamples(0); // Ensure to have the same test image everywhere

  renderWindow->AddRenderer(renderer.GetPointer());
  renderWindow->SetInteractor(renderWindowInteractor.GetPointer());

  // Set Interactor Style
  vtkNew<vtkThreeDViewInteractorStyle> iStyle;
  renderWindowInteractor->SetInteractorStyle(iStyle.GetPointer());

  // move back far enough to see the reformat widgets
  //renderer->GetActiveCamera()->SetPosition(0,0,-500.);

  // MRML scene
  vtkMRMLScene* scene = vtkMRMLScene::New();

  // Application logic - Handle creation of vtkMRMLSelectionNode and vtkMRMLInteractionNode
  vtkMRMLApplicationLogic* applicationLogic = vtkMRMLApplicationLogic::New();
  applicationLogic->SetMRMLScene(scene);

  vtkNew<vtkSlicerModelsLogic> modelsLogic;
  modelsLogic->SetMRMLScene(scene);
  vtkNew<vtkSlicerArmaturesLogic> armaturesLogic;
  armaturesLogic->SetMRMLScene(scene);
  armaturesLogic->SetModelsLogic(modelsLogic.GetPointer());
  vtkMRMLModelNode* model =
    armaturesLogic->AddArmatureFile(argv[1]);

  vtkNew<vtkPolyDataMapper> mapper;
  mapper->SetInput(model->GetPolyData());
  vtkNew<vtkActor> actor;
  actor->SetMapper(mapper.GetPointer());
  renderer->AddActor(actor.GetPointer());

  renderer->ResetCamera();

  // Event recorder
  bool disableReplay = false, record = false, screenshot = false;
  for (int i = 0; i < argc; i++)
    {
    disableReplay |= (strcmp("--DisableReplay", argv[i]) == 0);
    record        |= (strcmp("--Record", argv[i]) == 0);
    screenshot    |= (strcmp("--Screenshot", argv[i]) == 0);
    }
  vtkNew<vtkInteractorEventRecorder> recorder;
  recorder->SetInteractor(renderWindowInteractor.GetPointer());
  if (!disableReplay)
    {
    if (record)
      {
      std::cout << "Recording ..." << std::endl;
      recorder->SetFileName("vtkInteractorEventRecorder.log");
      recorder->On();
      recorder->Record();
      }
    else
      {
      // Play
      recorder->ReadFromInputStringOn();
      recorder->SetInputString(vtkMRMLVolumeRenderingDisplayableManagerTest1EventLog);
      recorder->Play();
      }
    }

  int retval = vtkRegressionTestImageThreshold(renderWindow.GetPointer(), 85.0);
  if ( record || retval == vtkRegressionTester::DO_INTERACTOR)
    {
    renderWindowInteractor->Initialize();
    renderWindowInteractor->Start();
    }

  if (record || screenshot)
    {
    vtkNew<vtkWindowToImageFilter> windowToImageFilter;
    windowToImageFilter->SetInput(renderWindow.GetPointer());
    windowToImageFilter->SetMagnification(1); //set the resolution of the output image
    windowToImageFilter->Update();

    vtkNew<vtkTesting> testHelper;
    testHelper->AddArguments(argc, const_cast<const char **>(argv));

    vtkStdString screenshootFilename = testHelper->GetDataRoot();
    screenshootFilename += "/Baseline/vtkSlicerArmaturesLogicTest.png";
    vtkNew<vtkPNGWriter> writer;
    writer->SetFileName(screenshootFilename.c_str());
    writer->SetInput(windowToImageFilter->GetOutput());
    writer->Write();
    std::cout << "Saved screenshot: " << screenshootFilename << std::endl;
    }

  applicationLogic->Delete();
  scene->Delete();

  return !retval;
}

double identity[4] = {1.0, 0.0, 0.0, 0.0};
double xRotation[4] = {0.7071065902709961, 0.7071070671081543, 0.0, 0.0};
double yRotation[4] = {0.7071065902709961, 0.0, 0.7071070671081543, 0.0};
double zRotation[4] = {0.7071065902709961, 0.0, 0.0, 0.7071070671081543};

double mxRotation[4] = {0.7071065902709961, -0.7071070671081543, 0.0, 0.0};
double myRotation[4] = {0.7071065902709961, 0.0, -0.7071070671081543, 0.0};
double mzRotation[4] = {0.7071065902709961, 0.0, 0.0, -0.7071070671081543};


bool Pose(double tail[3], double expected[3],
          std::vector<double*> orientations,
          std::vector<double*> poses)
{
  bool success = false;
  double res[3] = {0., 1., 0.};
  double mat[3][3] = {1.,0.,0.,0.,1.,0.,0.,0.,1.};
  double quat[4] = {1., 0., 0., 0.}, quat2[4] = {1., 0., 0., 0.};

  //double orientation0[4] = {0.7071068286895752, 0.0, 0.0, -0.7071068286895752};
  //orientations.insert( orientations.begin(), orientation0);

  int orientationCount = orientations.size();// - 1;
  size_t poseCount = poses.size();
  std::vector<double(*) [3]> orientationMats;
  std::vector<double(*) [3]> poseMats;
  std::vector<double(*) [3]> orientationInvMats;
  std::vector<double(*) [3]> poseInvMats;

  for (size_t i = 0; i < orientationCount; ++i)
    {
    double(* orientationMat)[3] = new double[3][3];
    vtkMath::QuaternionToMatrix3x3(orientations[i], orientationMat);
    orientationMats.push_back(orientationMat);
    double(* orientationInvMat)[3] = new double[3][3];
    vtkMath::Invert3x3(orientationMat, orientationInvMat);
    orientationInvMats.push_back(orientationInvMat);
    }
  for (size_t i = 0; i < poseCount; ++i)
    {
    double (* poseMat)[3]= new double[3][3];
    vtkMath::QuaternionToMatrix3x3(poses[i], poseMat);
    poseMats.push_back(poseMat);
    double(* poseInvMat)[3] = new double[3][3];
    vtkMath::Invert3x3(poseMat, poseInvMat);
    poseInvMats.push_back(poseInvMat);
    }
  //orientationCount--;

  // Technique 0 - Blender style
  res[0] = 0.; res[1] = 1.; res[2] = 0.;
  vtkMath::MultiplyScalar(res, vtkMath::Norm(tail));
  mat[0][0] = mat[1][1] = mat[2][2] = 1.;
  mat[0][1] = mat[0][2] = mat[1][0] = 0.;
  mat[1][2] = mat[2][0] = mat[2][1] = 0.;
  for (size_t i = 0; i < poseCount; ++i)
    {
    vtkMath::Multiply3x3(orientationInvMats[i], mat, mat);
    vtkMath::Multiply3x3(poseInvMats[i], mat, mat);
    }
  res[0] = mat[1][0]; res[1] = mat[1][1]; res[2] = mat[1][2];
  vtkMath::MultiplyScalar(res, vtkMath::Norm(tail));
  if (vtkMath::Distance2BetweenPoints(res, expected) > 0.0001)
    {
    success = false;
    }
  else
    {
    //success = true;
    }

  // Technique 1
  memcpy(res, tail, 3 * sizeof(double));
  //vtkMath::MultiplyScalar(res, vtkMath::Norm(tail));

  //for(size_t i = orientationCount - 1; i != -1; --i)
  for(size_t i = 0; i < orientationCount; ++i)
    {
    //vtkMath::Multiply3x3(orientationMats[i], res, res);
    //vtkMath::Multiply3x3(orientationInvMats[i], res, res);
    }
  for(size_t i = 0; i < poseCount; ++i)
  //for(size_t i = poseCount - 1; i != -1; --i)
    {
    double x[3] = {1., 0., 0.};
    double y[3] = {0., 1., 0.};
    double z[3] = {0., 0., 1.};
    //vtkMath::Multiply3x3(poseMats[i], res, res);
    //vtkMath::Multiply3x3(poseInvMats[i], res, res);
    for(size_t j = 0; j <= i; ++j)
    //for(size_t j = i; j != -1; --j)
    //size_t j = i;
      {
      //double(* mat)[3] = orientationMats[j];
      double(* mat)[3] = orientationInvMats[j];
      vtkMath::Multiply3x3(mat, res, res);
      vtkMath::Multiply3x3(mat, x, x);
      vtkMath::Multiply3x3(mat, y, y);
      vtkMath::Multiply3x3(mat, z, z);
      //vtkMath::Multiply3x3(poseMats[j], res, res);
      }
    //vtkMath::Multiply3x3(orientationInvMats[i], res, res);
    vtkMath::Multiply3x3(poseMats[i], res, res);
    //vtkMath::Multiply3x3(poseInvMats[i], res, res);
    //vtkMath::Multiply3x3(orientationMats[i], res, res);
    for(size_t j = i; j != -1; --j)
      {
      vtkMath::Multiply3x3(orientationMats[j], res, res);
      }
    //vtkMath::Multiply3x3(orientationInvMats[i], res, res);
    //vtkMath::Multiply3x3(orientationMats[i], res, res);
    }
  //for(size_t i = orientationCount - 1; i != -1; --i)
  //for(size_t i = 0; i < orientationCount; ++i)
      {
      //vtkMath::Multiply3x3(orientationMats[i], res, res);
      }
  if (vtkMath::Distance2BetweenPoints(res, expected) > 0.0001)
    {
    //success = false;
    }

  // Technique 2
  res[0] = 0.; res[1] = 1.; res[2] = 0.;
  vtkMath::MultiplyScalar(res, vtkMath::Norm(tail));
  mat[0][0] = mat[1][1] = mat[2][2] = 1.;
  mat[0][1] = mat[0][2] = mat[1][0] = 0.;
  mat[1][2] = mat[2][0] = mat[2][1] = 0.;
  for (size_t i = 0; i < poseCount; ++i)
    {
    for(size_t j = 0; j <= i; ++j)
      {
      vtkMath::Multiply3x3(mat, orientationMats[j], mat);
      }
    vtkMath::Multiply3x3(mat, poseMats[i], mat);
    for(size_t j = i; j != -1; --j)
      {
      vtkMath::Multiply3x3(mat, orientationInvMats[j], mat);
      }
    }
  vtkMath::Multiply3x3(mat, tail, res);
  //res[0] = mat[1][0]; res[1] = mat[1][1]; res[2] = mat[1][2];
  //vtkMath::MultiplyScalar(res, vtkMath::Norm(tail));
  if (vtkMath::Distance2BetweenPoints(res, expected) > 0.0001)
    {
    success = false;
    }
  else
    {
    //success = true;
    }

  // Technique 3
  quat[0] = 1.; quat[1] = 0.; quat[2] = 0.; quat[3] = 0.;
  quat2[0] = 1.; quat2[1] = 0.; quat2[2] = 0.; quat2[3] = 0.;
  for(size_t i = 0; i < poseCount; ++i)
    {
    for(size_t j = 0; j <= i; ++j)
      {
      vtkMath::MultiplyQuaternion(quat, orientations[j], quat);
      }
    vtkMath::MultiplyQuaternion(quat, poses[i], quat);
    for(size_t j = i; j != -1; --j)
      {
      double inv[4];
      inv[0] = orientations[j][0];
      inv[1] = -orientations[j][1];
      inv[2] = -orientations[j][2];
      inv[3] = -orientations[j][3];
      vtkMath::MultiplyQuaternion(quat, inv, quat);
      }
    }
  vtkMath::QuaternionToMatrix3x3(quat, mat);
  vtkMath::Multiply3x3(mat, tail, res);
  if (vtkMath::Distance2BetweenPoints(res, expected) > 0.0001)
    {
    success = false;
    }

  // Technique 4
  quat[0] = 1.; quat[1] = 0.; quat[2] = 0.; quat[3] = 0.;
  quat2[0] = 1.; quat2[1] = 0.; quat2[2] = 0.; quat2[3] = 0.;
  for(size_t i = 0; i < orientationCount; ++i)
    {
    vtkMath::MultiplyQuaternion(orientations[i], quat2, quat2);
    }
  for(size_t i = poseCount -1; i != -1; --i)
    {
    vtkMath::MultiplyQuaternion(poses[i], quat, quat);
    }
  //vtkMath::MultiplyQuaternion(quat2, quat, quat);
  vtkMath::MultiplyQuaternion(quat, quat2, quat);
  vtkMath::QuaternionToMatrix3x3(quat, mat);
  vtkMath::Multiply3x3(mat, tail, res);
  if (vtkMath::Distance2BetweenPoints(res, expected) > 0.0001)
    {
    //success = false;
    }

  // Technique 5
  quat[0] = 1.; quat[1] = 0.; quat[2] = 0.; quat[3] = 0.;
  quat2[0] = 1.; quat2[1] = 0.; quat2[2] = 0.; quat2[3] = 0.;
  for(size_t i = 0; i < orientationCount; ++i)
    {
    vtkMath::MultiplyQuaternion(orientations[i], quat2, quat2);
    }
  for(size_t i = 0; i < poseCount; ++i)
    {
    vtkMath::MultiplyQuaternion(quat, poses[i], quat);
    }
  //vtkMath::MultiplyQuaternion(quat2, quat, quat);
  vtkMath::MultiplyQuaternion(quat, quat2, quat);
  vtkMath::QuaternionToMatrix3x3(quat, mat);
  vtkMath::Multiply3x3(mat, tail, res);
  if (vtkMath::Distance2BetweenPoints(res, expected) > 0.0001)
    {
    //success = false;
    }


  for (size_t i = 0; i < orientationCount; ++i)
    {
    delete [] orientationMats[i];
    delete [] orientationInvMats[i];
    }
  for (size_t i = 0; i < poseCount; ++i)
    {
    delete [] poseMats[i];
    delete [] poseInvMats[i];
    }

  return success;
}

///
bool BasicTest()
{
  double tail[3] = {0., 0., 10.};
  double rotationMat[3][3];
  double u[3];
  u[0] = tail[0];
  u[1] = tail[1];
  u[2] = tail[2];
  double angle = 0.;
  vtkMath::Normalize(u);
  rotationMat[0][0] = cos(angle) + u[0]*u[0]*(1.-cos(angle));
  rotationMat[0][1] = u[0]*u[1]*(1.-cos(angle))-u[2]*sin(angle);
  rotationMat[0][2] = u[0]*u[2]*(1.-cos(angle))+u[1]*sin(angle);
  rotationMat[1][0] = u[1]*u[0]*(1.-cos(angle))+u[2]*sin(angle);
  rotationMat[1][1] = cos(angle)+u[1]*u[1]*(1-cos(angle));
  rotationMat[1][2] = u[1]*u[2]*(1.-cos(angle))-u[0]*sin(angle);
  rotationMat[2][0] = u[2]*u[0]*(1.-cos(angle))+u[1]*sin(angle);
  rotationMat[2][1] = u[2]*u[1]*(1.-cos(angle))+u[0]*sin(angle);
  rotationMat[2][2] = cos(angle)+u[2]*u[2]*(1.-cos(angle));
  double quat[4];
  vtkMath::Matrix3x3ToQuaternion(rotationMat, quat);
  double res[3];
  vtkMath::Multiply3x3(rotationMat, tail, res);
  return true;
}

///
bool Test1Bone()
{
  double tail[3];
  double orientation[4];
  double pose[4];
  double expected[3];

  std::vector<double *> orientations;
  orientations.push_back(orientation);

  std::vector<double *> poses;
  poses.push_back(pose);

  // ************** Z **************
  tail[0] =  0.; tail[1] =  0.; tail[2] = 10.;
  memcpy(orientation, xRotation, 4*sizeof(double));

  // Zi
  memcpy(pose, identity, 4 * sizeof(double));
  expected[0] = 0.; expected[1] = 0.; expected[2] = 10.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Zz
  memcpy(pose, zRotation, 4 * sizeof(double));
  expected[0] =-10.; expected[1] =  0.; expected[2] =  0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Zy
  memcpy(pose, yRotation, 4 * sizeof(double));
  expected[0] = 0.; expected[1] = 0.; expected[2] = 10.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Zx
  memcpy(pose, xRotation, 4 * sizeof(double));
  expected[0] = 0.; expected[1] = -10.; expected[2] = 0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // ************** X **************
  tail[0] = 10.; tail[1] = 0.; tail[2] = 0.;
  memcpy(orientation, mzRotation, 4*sizeof(double));

  // Xi
  memcpy(pose, identity, 4 * sizeof(double));
  expected[0] = 10.; expected[1] = 0.; expected[2] = 0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Xz
  memcpy(pose, zRotation, 4 * sizeof(double));
  expected[0] = 0.; expected[1] = 10.; expected[2] = 0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Xy
  memcpy(pose, yRotation, 4 * sizeof(double));
  expected[0] = 10.; expected[1] = 0.; expected[2] = 0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Xx
  memcpy(pose, xRotation, 4 * sizeof(double));
  expected[0] = 0.; expected[1] = 0.; expected[2] = 10.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // ************** Y **************
  tail[0] = 0.; tail[1] = 10.; tail[2] = 0.;
  memcpy(orientation, identity, 4 * sizeof(double));

  // Yi
  memcpy(pose, identity, 4 * sizeof(double));
  expected[0] = 0.; expected[1] = 10.; expected[2] = 0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Yz
  memcpy(pose, zRotation, 4 * sizeof(double));
  expected[0] = -10.; expected[1] = 0.; expected[2] = 0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Yy
  memcpy(pose, yRotation, 4 * sizeof(double));
  expected[0] = 0.; expected[1] = 10.; expected[2] = 0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Yx
  memcpy(pose, xRotation, 4 * sizeof(double));
  expected[0] = 0.; expected[1] = 0.; expected[2] = 10.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }
  return true;
}

bool Test2Bones()
{
  double tail[3],expected[3];
  double orientation1[4], orientation2[4];
  double pose1[4], pose2[4];

  std::vector<double *> orientations;
  orientations.push_back(orientation1);
  orientations.push_back(orientation2);

  std::vector<double *> poses;
  poses.push_back(pose1);
  poses.push_back(pose2);

  // *************** Zi ***************
  memcpy(orientation1, xRotation, 4*sizeof(double));
  memcpy(pose1, identity, 4 * sizeof(double));

  // *************** Zi X ***************
  tail[0] = 10.; tail[1] = 0.; tail[2] = 0.;
  orientation2[0] = 0.5; orientation2[1] = -0.5; orientation2[2] = -0.5; orientation2[3] = -0.5;

  // Zi Xi
  memcpy(pose2, identity, 4 * sizeof(double));
  expected[0] = 10.; expected[1] = 0.; expected[2] = 0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Zi Xx
  memcpy(pose2, xRotation, 4 * sizeof(double));
  expected[0] = 0.; expected[1] = 0.; expected[2] = 10.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // *************** Zx ***************
  memcpy(pose1, xRotation, 4 * sizeof(double));

  // *************** Zx X ***************
  tail[0] = 10.; tail[1] = 0.; tail[2] = 0.;
  orientation2[0] = 0.5; orientation2[1] = -0.5; orientation2[2] = -0.5; orientation2[3] = -0.5;

  // Zx Xi
  memcpy(pose2, identity, 4 * sizeof(double));
  expected[0] = 10.; expected[1] = 0.; expected[2] = 0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Zx Xx
  memcpy(pose2, xRotation, 4 * sizeof(double));
  expected[0] =0.; expected[1] = -10.; expected[2] = 0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Zx Xy
  memcpy(pose2, yRotation, 4 * sizeof(double));
  expected[0] = 10.; expected[1] = 0.; expected[2] = 0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Zx Xz
  memcpy(pose2, zRotation, 4 * sizeof(double));
  expected[0] = 0.; expected[1] = 0.; expected[2] = 10.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // *************** Zx Y ***************
  tail[0] = 0.; tail[1] = 10.; tail[2] = 0.;
  memcpy(orientation2, mxRotation, 4 * sizeof(double));

  // Zx Yx
  memcpy(pose2, xRotation, 4 * sizeof(double));
  expected[0] = 0.; expected[1] = -10.; expected[2] = 0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Zx Yy
  memcpy(pose2, yRotation, 4 * sizeof(double));
  expected[0] = 0.; expected[1] = 0.; expected[2] = 10.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Zx Yz
  memcpy(pose2, zRotation, 4 * sizeof(double));
  expected[0] = -10.; expected[1] = 0.; expected[2] = 0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // *************** Zx Z ***************
  tail[0] = 0.; tail[1] = 0.; tail[2] = 10.;
  memcpy(orientation2, identity, 4 * sizeof(double));

  // Zx Zx
  memcpy(pose2, xRotation, 4 * sizeof(double));
  expected[0] = 0.; expected[1] = 0.; expected[2] = -10.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Zx Zy
  memcpy(pose2, yRotation, 4 * sizeof(double));
  expected[0] = 0.; expected[1] = -10.; expected[2] = 0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Zx Zz
  memcpy(pose2, zRotation, 4 * sizeof(double));
  expected[0] = -10.; expected[1] = 0.; expected[2] = 0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }



  // *************** Zy ***************
  memcpy(pose1, yRotation, 4 * sizeof(double));

  // *************** Zy X ***************
  tail[0] = 10.; tail[1] = 0.; tail[2] = 0.;
  orientation2[0] = 0.5; orientation2[1] = -0.5; orientation2[2] = -0.5; orientation2[3] = -0.5;

  // Zy Xi
  memcpy(pose2, identity, 4 * sizeof(double));
  expected[0] = 0.; expected[1] = 10.; expected[2] = 0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Zy Xx
  memcpy(pose2, xRotation, 4 * sizeof(double));
  expected[0] =0.; expected[1] = 0.; expected[2] = 10.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Zy Xy
  memcpy(pose2, yRotation, 4 * sizeof(double));
  expected[0] = 0.; expected[1] = 10.; expected[2] = 0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Zy Xz
  memcpy(pose2, zRotation, 4 * sizeof(double));
  expected[0] = -10.; expected[1] = 0.; expected[2] = 0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // *************** Zy Y ***************
  tail[0] = 0.; tail[1] = 10.; tail[2] = 0.;
  memcpy(orientation2, mxRotation, 4 * sizeof(double));

  // Zy Yx
  memcpy(pose2, xRotation, 4 * sizeof(double));
  expected[0] = 0.; expected[1] = 0.; expected[2] = 10.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Zy Yy
  memcpy(pose2, yRotation, 4 * sizeof(double));
  expected[0] = -10.; expected[1] = 0.; expected[2] = 0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Zy Yz
  memcpy(pose2, zRotation, 4 * sizeof(double));
  expected[0] = 0.; expected[1] = -10.; expected[2] = 0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // *************** Zy Z ***************
  tail[0] = 0.; tail[1] = 0.; tail[2] = 10.;
  memcpy(orientation2, identity, 4 * sizeof(double));

  // Zy Zx
  memcpy(pose2, xRotation, 4 * sizeof(double));
  expected[0] = 10.; expected[1] = 0.; expected[2] = 0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Zy Zy
  memcpy(pose2, yRotation, 4 * sizeof(double));
  expected[0] = 0.; expected[1] = 0.; expected[2] = 10.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Zy Zz
  memcpy(pose2, zRotation, 4 * sizeof(double));
  expected[0] = 0.; expected[1] = -10.; expected[2] = 0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }



  // *************** Yi ***************
  memcpy(orientation1, identity, 4*sizeof(double));
  memcpy(pose1, identity, 4 * sizeof(double));

  // *************** Yi X ***************
  tail[0] = 10.; tail[1] = 0.; tail[2] = 0.;
  memcpy(orientation2, mzRotation, 4*sizeof(double));

  // Yi Xi
  memcpy(pose2, identity, 4 * sizeof(double));
  expected[0] = 10.; expected[1] = 0.; expected[2] = 0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Yi Xx
  memcpy(pose2, xRotation, 4 * sizeof(double));
  expected[0] = 0.; expected[1] = 0.; expected[2] = 10.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // *************** Yx ***************
  memcpy(pose1, xRotation, 4 * sizeof(double));

  // *************** Yx X ***************
  tail[0] = 10.; tail[1] = 0.; tail[2] = 0.;
  memcpy(orientation2, mzRotation, 4*sizeof(double));

  // Yx Xi
  memcpy(pose2, identity, 4 * sizeof(double));
  expected[0] = 10.; expected[1] = 0.; expected[2] = 0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Yx Xx
  memcpy(pose2, xRotation, 4 * sizeof(double));
  expected[0] =0.; expected[1] = -10.; expected[2] = 0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Yx Xy
  memcpy(pose2, yRotation, 4 * sizeof(double));
  expected[0] = 10.; expected[1] = 0.; expected[2] = 0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Yx Xz
  memcpy(pose2, zRotation, 4 * sizeof(double));
  expected[0] = 0.; expected[1] = 0.; expected[2] = 10.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // *************** Yx Y ***************
  tail[0] = 0.; tail[1] = 10.; tail[2] = 0.;
  memcpy(orientation2, identity, 4 * sizeof(double));

  // Yx Yx
  memcpy(pose2, xRotation, 4 * sizeof(double));
  expected[0] = 0.; expected[1] = -10.; expected[2] = 0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Yx Yy
  memcpy(pose2, yRotation, 4 * sizeof(double));
  expected[0] = 0.; expected[1] = 0.; expected[2] = 10.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Yx Yz
  memcpy(pose2, zRotation, 4 * sizeof(double));
  expected[0] = -10.; expected[1] = 0.; expected[2] = 0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // *************** Yx Z ***************
  tail[0] = 0.; tail[1] = 0.; tail[2] = 10.;
  memcpy(orientation2, xRotation, 4 * sizeof(double));

  // Yx Zx
  memcpy(pose2, xRotation, 4 * sizeof(double));
  expected[0] = 0.; expected[1] = 0.; expected[2] = -10.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Yx Zy
  memcpy(pose2, yRotation, 4 * sizeof(double));
  expected[0] = 0.; expected[1] = -10.; expected[2] = 0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Yx Zz
  memcpy(pose2, zRotation, 4 * sizeof(double));
  expected[0] = -10.; expected[1] = 0.; expected[2] = 0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }


  // *************** Yy ***************
  memcpy(pose1, yRotation, 4 * sizeof(double));

  // *************** Yy X ***************
  tail[0] = 10.; tail[1] = 0.; tail[2] = 0.;
  memcpy(orientation2, mzRotation, 4*sizeof(double));

  // Yy Xi
  memcpy(pose2, identity, 4 * sizeof(double));
  expected[0] = 0.; expected[1] = 0.; expected[2] = -10.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Yy Xx
  memcpy(pose2, xRotation, 4 * sizeof(double));
  expected[0] = 10.; expected[1] = 0.; expected[2] = 0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Yy Xy
  memcpy(pose2, yRotation, 4 * sizeof(double));
  expected[0] = 0.; expected[1] = 0.; expected[2] = -10.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Yy Xz
  memcpy(pose2, zRotation, 4 * sizeof(double));
  expected[0] = 0.; expected[1] = 10.; expected[2] = 0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // *************** Yy Y ***************
  tail[0] = 0.; tail[1] = 10.; tail[2] = 0.;
  memcpy(orientation2, identity, 4 * sizeof(double));

  // Yy Yx
  memcpy(pose2, xRotation, 4 * sizeof(double));
  expected[0] = 10.; expected[1] = 0.; expected[2] = 0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Yy Yy
  memcpy(pose2, yRotation, 4 * sizeof(double));
  expected[0] = 0.; expected[1] = 10.; expected[2] = 0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Yy Yz
  memcpy(pose2, zRotation, 4 * sizeof(double));
  expected[0] = 0.; expected[1] = 0.; expected[2] = 10.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // *************** Yy Z ***************
  tail[0] = 0.; tail[1] = 0.; tail[2] = 10.;
  memcpy(orientation2, xRotation, 4 * sizeof(double));

  // Yy Zx
  memcpy(pose2, xRotation, 4 * sizeof(double));
  expected[0] = 0.; expected[1] = -10.; expected[2] = 0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Yy Zy
  memcpy(pose2, yRotation, 4 * sizeof(double));
  expected[0] = 10.; expected[1] = 0.; expected[2] = 0.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }

  // Yy Zz
  memcpy(pose2, zRotation, 4 * sizeof(double));
  expected[0] = 0.; expected[1] = 0.; expected[2] = 10.;
  if (Pose(tail, expected, orientations, poses) != true)
    {
    return false;
    }



  return true;
}

///
bool Test1()
{
  double tail[3] = {10., 0., 0.};

  double orientation1[4] = {0.7071068286895752, 0.7071068286895752, 0.0, 0.0};
  double orientation2[4] = {0.7071068286895752, -0.7071068286895752,0.0,0.0};
  double orientation3[4] = {0.7071068286895752, 0.0,0.0, -0.7071068286895752,};

  double pose1[4] = {0.7071065902709961, 0.7071070671081543, 0.0, 0.0};
  double pose2[4] = {0.7071065902709961, 0.7071070671081543, 0.0, 0.0};
  double pose3[4] = {0.7071065902709961, 0.0, 0.0, 0.7071070671081543};

  double expected[3] = {0., -10., 0.};

  std::vector<double *> orientations;
  orientations.push_back(orientation1);
  orientations.push_back(orientation2);
  orientations.push_back(orientation3);

  std::vector<double *> poses;
  poses.push_back(pose1);
  poses.push_back(pose2);
  poses.push_back(pose3);

  return Pose(tail, expected, orientations, poses);
}

bool Test2()
{
  double tail[3] = {10., 0., 0.};

  double orientation1[4] = {0.7071068286895752, 0.7071068286895752, 0.0, 0.0};
  double orientation2[4] = {0.7071068286895752, -0.7071068286895752,0.0,0.0};
  double orientation3[4] = {0.7071068286895752, 0.0, 0.0, -0.7071068286895752};
  double orientation4[4] = {0.7071068286895752, 0.0, 0.0, -0.7071068286895752};

  double pose1[4] = {1., 0.0, 0.0, 0.0};
  double pose2[4] = {0.7071065902709961, 0.7071070671081543, 0.0, 0.0};
  double pose3[4] = {0.7071065902709961, 0.0, 0.0, 0.7071070671081543};
  double pose4[4] = {0.7071065902709961, 0.0, 0.0, 0.7071070671081543};

  double expected[3] = {0., 0., 10.};

  std::vector<double*> orientations;
  orientations.push_back(orientation1);
  orientations.push_back(orientation2);
  orientations.push_back(orientation3);
  orientations.push_back(orientation4);

  std::vector<double*> poses;
  poses.push_back(pose1);
  poses.push_back(pose2);
  poses.push_back(pose3);
  poses.push_back(pose4);

  return Pose(tail, expected, orientations, poses);
}

bool Test3()
{
  double tail[3] = {10., 0., 0.};

  double pose1[4] = {1.0, 0.0, 0.0, 0.0};
  double pose2[4] = {0.7071065902709961, 0.7071070671081543, 0.0, 0.0};
  double pose3[4] = {0.7071065902709961, 0.7071070671081543, 0.0, 0.0};

  //double orientation0[4] = {0.7071068286895752, 0.0, 0.0, 0.7071068286895752};
  double orientation1[4] = {0.7071068286895752, 0.7071068286895752, 0.0, 0.0};
  double orientation2[4] = {0.7071068286895752, -0.7071068286895752,0.0,0.0};
  double orientation3[4] = {0.7071068286895752, 0.0,0.0, -0.7071068286895752,};

  double expected[3] = {0., -10., 0.};

  std::vector<double*> orientations;
  orientations.push_back(orientation1);
  orientations.push_back(orientation2);
  orientations.push_back(orientation3);

  std::vector<double*> poses;
  poses.push_back(pose1);
  poses.push_back(pose2);
  poses.push_back(pose3);

  return Pose(tail, expected, orientations, poses);
}

bool Test3Bones()
{
  double tail[3] = {10., 0., 0.}, tail1[3], tail2[3], tail3[3], tail4[3], tail5[3], tail6[3];
  double expected[3] = {0., -10., 0.};
  double pose1[4] = {0.7071065902709961, 0.7071070671081543, 0.0, 0.0};
  double pose2[4] = {0.7071065902709961, 0.7071070671081543, 0.0, 0.0};
  double pose3[4] = {0.7071065902709961, 0.0, 0.0, 0.7071070671081543};

  double orientation1[4] = {0.7071068286895752, 0.7071068286895752, 0.0, 0.0};
  double orientation2[4] = {0.7071068286895752, -0.7071068286895752,0.0,0.0};
  double orientation3[4] = {0.7071068286895752, 0.0,0.0, -0.7071068286895752,};

  double pose1Mat[3][3], pose2Mat[3][3], pose3Mat[3][3];
  double orientation1Mat[3][3], orientation2Mat[3][3], orientation3Mat[3][3];
  double pose1InvMat[3][3], pose2InvMat[3][3], pose3InvMat[3][3];
  double orientation1InvMat[3][3], orientation2InvMat[3][3], orientation3InvMat[3][3];

  vtkMath::QuaternionToMatrix3x3(pose1, pose1Mat);
  vtkMath::QuaternionToMatrix3x3(pose2, pose2Mat);
  vtkMath::QuaternionToMatrix3x3(pose3, pose3Mat);
  vtkMath::QuaternionToMatrix3x3(orientation1, orientation1Mat);
  vtkMath::QuaternionToMatrix3x3(orientation2, orientation2Mat);
  vtkMath::QuaternionToMatrix3x3(orientation3, orientation3Mat);

  vtkMath::Invert3x3(pose1Mat, pose1InvMat);
  vtkMath::Invert3x3(pose2Mat, pose2InvMat);
  vtkMath::Invert3x3(pose3Mat, pose3InvMat);
  vtkMath::Invert3x3(orientation1Mat, orientation1InvMat);
  vtkMath::Invert3x3(orientation2Mat, orientation2InvMat);
  vtkMath::Invert3x3(orientation3Mat, orientation3InvMat);

  // YES !!!!
  vtkMath::Multiply3x3(orientation1Mat, tail, tail2);
  vtkMath::Multiply3x3(orientation2Mat, tail2, tail3);
  vtkMath::Multiply3x3(pose3Mat, tail3, tail4);
  vtkMath::Multiply3x3(pose2Mat, tail4, tail5);
  vtkMath::Multiply3x3(pose1Mat, tail5, tail6);

  // YES !!
  double mat[3][3] = {1.,0.,0.,0.,1.,0.,0.,0.,1.};
  vtkMath::Multiply3x3(mat, pose1Mat, mat);
  vtkMath::Multiply3x3(mat, pose2Mat, mat);
  vtkMath::Multiply3x3(mat, pose3Mat, mat);
  vtkMath::Multiply3x3(mat, orientation2Mat, mat);
  vtkMath::Multiply3x3(mat, orientation1Mat, mat);
  vtkMath::Multiply3x3(mat, tail, tail2);

  // YES !!!
  double quat[4] = {1., 0., 0., 0.}, quat2[4] = {1., 0., 0., 0.};
  vtkMath::MultiplyQuaternion(orientation1, quat, quat);
  vtkMath::MultiplyQuaternion(orientation2, quat, quat);
  vtkMath::MultiplyQuaternion(pose3, quat, quat);
  vtkMath::MultiplyQuaternion(pose2, quat, quat);
  vtkMath::MultiplyQuaternion(pose1, quat, quat);
  vtkMath::QuaternionToMatrix3x3(quat, mat);
  vtkMath::Multiply3x3(mat, tail, tail2);

  // YES !
  quat[0] = 1.; quat[1] = 0.; quat[2] = 0.; quat[3] = 0.;
  quat2[0] = 1.; quat2[1] = 0.; quat2[2] = 0.; quat2[3] = 0.;
  vtkMath::MultiplyQuaternion(orientation1, quat2, quat2);
  vtkMath::MultiplyQuaternion(orientation2, quat2, quat2);
  vtkMath::MultiplyQuaternion(pose3, quat, quat);
  vtkMath::MultiplyQuaternion(pose2, quat, quat);
  vtkMath::MultiplyQuaternion(pose1, quat, quat);
  //vtkMath::MultiplyQuaternion(quat2, quat, quat);
  vtkMath::MultiplyQuaternion(quat, quat2, quat);
  vtkMath::QuaternionToMatrix3x3(quat, mat);
  vtkMath::Multiply3x3(mat, tail, tail2);

  // YES !
  quat[0] = 1.; quat[1] = 0.; quat[2] = 0.; quat[3] = 0.;
  quat2[0] = 1.; quat2[1] = 0.; quat2[2] = 0.; quat2[3] = 0.;
  vtkMath::MultiplyQuaternion(orientation1, quat2, quat2);
  vtkMath::MultiplyQuaternion(orientation2, quat2, quat2);
  vtkMath::MultiplyQuaternion(quat, pose1, quat);
  vtkMath::MultiplyQuaternion(quat, pose2, quat);
  vtkMath::MultiplyQuaternion(quat, pose3, quat);
  //vtkMath::MultiplyQuaternion(quat2, quat, quat);
  vtkMath::MultiplyQuaternion(quat, quat2, quat);
  vtkMath::QuaternionToMatrix3x3(quat, mat);
  vtkMath::Multiply3x3(mat, tail, tail2);
  return true;
}

bool Test4Bones()
{
  double tail[3] = {10., 0., 0.}, tail1[3], tail2[3], tail3[3], tail4[3], tail5[3], tail6[3], tail7[3], tail8[3];
  double expected[3] = {0., 0., 10.};
  double pose1[4] = {1., 0.0, 0.0, 0.0};
  double pose2[4] = {0.7071065902709961, 0.7071070671081543, 0.0, 0.0};
  double pose3[4] = {0.7071065902709961, 0.0, 0.0, 0.7071070671081543};
  double pose4[4] = {0.7071065902709961, 0.0, 0.0, 0.7071070671081543};

  double orientation1[4] = {0.7071068286895752, 0.7071068286895752, 0.0, 0.0};
  double orientation2[4] = {0.7071068286895752, -0.7071068286895752,0.0,0.0};
  double orientation3[4] = {0.7071068286895752, 0.0, 0.0, -0.7071068286895752};
  double orientation4[4] = {0.7071068286895752, 0.0, 0.0, -0.7071068286895752};

  double pose1Mat[3][3], pose2Mat[3][3], pose3Mat[3][3], pose4Mat[3][3];
  double orientation1Mat[3][3], orientation2Mat[3][3], orientation3Mat[3][3], orientation4Mat[3][3];
  double pose1InvMat[3][3], pose2InvMat[3][3], pose3InvMat[3][3], pose4InvMat[3][3];
  double orientation1InvMat[3][3], orientation2InvMat[3][3], orientation3InvMat[3][3], orientation4InvMat[3][3];

  vtkMath::QuaternionToMatrix3x3(pose1, pose1Mat);
  vtkMath::QuaternionToMatrix3x3(pose2, pose2Mat);
  vtkMath::QuaternionToMatrix3x3(pose3, pose3Mat);
  vtkMath::QuaternionToMatrix3x3(pose4, pose4Mat);
  vtkMath::QuaternionToMatrix3x3(orientation1, orientation1Mat);
  vtkMath::QuaternionToMatrix3x3(orientation2, orientation2Mat);
  vtkMath::QuaternionToMatrix3x3(orientation3, orientation3Mat);
  vtkMath::QuaternionToMatrix3x3(orientation4, orientation4Mat);

  vtkMath::Invert3x3(pose1Mat, pose1InvMat);
  vtkMath::Invert3x3(pose2Mat, pose2InvMat);
  vtkMath::Invert3x3(pose3Mat, pose3InvMat);
  vtkMath::Invert3x3(pose4Mat, pose4InvMat);
  vtkMath::Invert3x3(orientation1Mat, orientation1InvMat);
  vtkMath::Invert3x3(orientation2Mat, orientation2InvMat);
  vtkMath::Invert3x3(orientation3Mat, orientation3InvMat);
  vtkMath::Invert3x3(orientation4Mat, orientation4InvMat);

  // YES !!
  vtkMath::Multiply3x3(orientation1Mat, tail, tail2);
  vtkMath::Multiply3x3(orientation2Mat, tail2, tail3);
  vtkMath::Multiply3x3(orientation3Mat, tail3, tail4);
  vtkMath::Multiply3x3(pose4Mat, tail4, tail5);
  vtkMath::Multiply3x3(pose3Mat, tail5, tail6);
  vtkMath::Multiply3x3(pose2Mat, tail6, tail7);
  vtkMath::Multiply3x3(pose1Mat, tail7, tail8);

  // YES !!
  double mat[3][3] = {1.,0.,0.,0.,1.,0.,0.,0.,1.};
  vtkMath::Multiply3x3(mat, pose1Mat, mat);
  vtkMath::Multiply3x3(mat, pose2Mat, mat);
  vtkMath::Multiply3x3(mat, pose3Mat, mat);
  vtkMath::Multiply3x3(mat, pose4Mat, mat);
  vtkMath::Multiply3x3(mat, orientation3Mat, mat);
  vtkMath::Multiply3x3(mat, orientation2Mat, mat);
  vtkMath::Multiply3x3(mat, orientation1Mat, mat);
  vtkMath::Multiply3x3(mat, tail, tail2);

  // YES !!
  mat[0][0] = mat[1][1] = mat[2][2] = 1.0;
  mat[0][1] = mat[0][2] = mat[1][0] = 0.0;
  mat[1][2] = mat[2][0] = mat[2][1] = 0.0;
  vtkMath::Multiply3x3(orientation1Mat, mat, mat);
  vtkMath::Multiply3x3(orientation2Mat, mat, mat);
  vtkMath::Multiply3x3(orientation3Mat, mat, mat);
  vtkMath::Multiply3x3(pose4Mat, mat, mat);
  vtkMath::Multiply3x3(pose3Mat, mat, mat);
  vtkMath::Multiply3x3(pose2Mat, mat, mat);
  vtkMath::Multiply3x3(pose1Mat, mat, mat);
  vtkMath::Multiply3x3(mat, tail, tail2);

  // YES !!!
  double quat[4] = {1., 0., 0., 0.}, quat2[4] = {1., 0., 0., 0.};
  vtkMath::MultiplyQuaternion(orientation1, quat, quat);
  vtkMath::MultiplyQuaternion(orientation2, quat, quat);
  vtkMath::MultiplyQuaternion(orientation3, quat, quat);
  vtkMath::MultiplyQuaternion(pose4, quat, quat);
  vtkMath::MultiplyQuaternion(pose3, quat, quat);
  vtkMath::MultiplyQuaternion(pose2, quat, quat);
  vtkMath::MultiplyQuaternion(pose1, quat, quat);
  vtkMath::QuaternionToMatrix3x3(quat, mat);
  vtkMath::Multiply3x3(mat, tail, tail2);

  // YES !
  quat[0] = 1.; quat[1] = 0.; quat[2] = 0.; quat[3] = 0.;
  quat2[0] = 1.; quat2[1] = 0.; quat2[2] = 0.; quat2[3] = 0.;
  vtkMath::MultiplyQuaternion(orientation1, quat2, quat2);
  vtkMath::MultiplyQuaternion(orientation2, quat2, quat2);
  vtkMath::MultiplyQuaternion(orientation3, quat2, quat2);
  vtkMath::MultiplyQuaternion(pose4, quat, quat);
  vtkMath::MultiplyQuaternion(pose3, quat, quat);
  vtkMath::MultiplyQuaternion(pose2, quat, quat);
  vtkMath::MultiplyQuaternion(pose1, quat, quat);
  //vtkMath::MultiplyQuaternion(quat2, quat, quat);
  vtkMath::MultiplyQuaternion(quat, quat2, quat);
  vtkMath::QuaternionToMatrix3x3(quat, mat);
  vtkMath::Multiply3x3(mat, tail, tail2);

  // YES !
  quat[0] = 1.; quat[1] = 0.; quat[2] = 0.; quat[3] = 0.;
  quat2[0] = 1.; quat2[1] = 0.; quat2[2] = 0.; quat2[3] = 0.;
  vtkMath::MultiplyQuaternion(orientation1, quat2, quat2);
  vtkMath::MultiplyQuaternion(orientation2, quat2, quat2);
  vtkMath::MultiplyQuaternion(orientation3, quat2, quat2);
  vtkMath::MultiplyQuaternion(quat, pose1, quat);
  vtkMath::MultiplyQuaternion(quat, pose2, quat);
  vtkMath::MultiplyQuaternion(quat, pose3, quat);
  vtkMath::MultiplyQuaternion(quat, pose4, quat);
  //vtkMath::MultiplyQuaternion(quat2, quat, quat);
  vtkMath::MultiplyQuaternion(quat, quat2, quat);
  vtkMath::QuaternionToMatrix3x3(quat, mat);
  vtkMath::Multiply3x3(mat, tail, tail2);
  return true;
}

bool Test3Bones2()
{
  double tail[3] = {10., 0., 0.}, tail1[3], tail2[3], tail3[3], tail4[3], tail5[3], tail6[3];
  double expected[3] = {0., -10., 0.};
  double pose1[4] = {1.0, 0.0, 0.0, 0.0};
  double pose2[4] = {0.7071065902709961, 0.7071070671081543, 0.0, 0.0};
  double pose3[4] = {0.7071065902709961, 0.7071070671081543, 0.0, 0.0};

  double orientation0[4] = {0.7071068286895752, 0.0, 0.0, 0.7071068286895752};
  double orientation1[4] = {0.7071068286895752, 0.7071068286895752, 0.0, 0.0};
  double orientation2[4] = {0.7071068286895752, -0.7071068286895752,0.0,0.0};
  double orientation3[4] = {0.7071068286895752, 0.0,0.0, -0.7071068286895752,};

  double pose1Mat[3][3], pose2Mat[3][3], pose3Mat[3][3];
  double orientation0Mat[3][3], orientation1Mat[3][3], orientation2Mat[3][3], orientation3Mat[3][3];
  double pose1InvMat[3][3], pose2InvMat[3][3], pose3InvMat[3][3];
  double orientation0InvMat[3][3], orientation1InvMat[3][3], orientation2InvMat[3][3], orientation3InvMat[3][3];

  vtkMath::QuaternionToMatrix3x3(pose1, pose1Mat);
  vtkMath::QuaternionToMatrix3x3(pose2, pose2Mat);
  vtkMath::QuaternionToMatrix3x3(pose3, pose3Mat);
  vtkMath::QuaternionToMatrix3x3(orientation0, orientation0Mat);
  vtkMath::QuaternionToMatrix3x3(orientation1, orientation1Mat);
  vtkMath::QuaternionToMatrix3x3(orientation2, orientation2Mat);
  vtkMath::QuaternionToMatrix3x3(orientation3, orientation3Mat);

  vtkMath::Invert3x3(pose1Mat, pose1InvMat);
  vtkMath::Invert3x3(pose2Mat, pose2InvMat);
  vtkMath::Invert3x3(pose3Mat, pose3InvMat);
  vtkMath::Invert3x3(orientation0Mat, orientation0InvMat);
  vtkMath::Invert3x3(orientation1Mat, orientation1InvMat);
  vtkMath::Invert3x3(orientation2Mat, orientation2InvMat);
  vtkMath::Invert3x3(orientation3Mat, orientation3InvMat);

  // YES !!!!
  vtkMath::Multiply3x3(orientation0Mat, tail, tail1);
  vtkMath::Multiply3x3(orientation1Mat, tail1, tail2);
  vtkMath::Multiply3x3(orientation2Mat, tail2, tail3);
  vtkMath::Multiply3x3(pose3Mat, tail3, tail4);
  vtkMath::Multiply3x3(pose2Mat, tail4, tail5);
  vtkMath::Multiply3x3(pose1Mat, tail5, tail6);
  //vtkMath::Multiply3x3(pose1Mat, tail3, tail4);
  //vtkMath::Multiply3x3(pose2Mat, tail4, tail5);
  //vtkMath::Multiply3x3(pose3Mat, tail5, tail6);

  // YES !!
  double mat[3][3] = {1.,0.,0.,0.,1.,0.,0.,0.,1.};
  vtkMath::Multiply3x3(mat, pose1Mat, mat);
  vtkMath::Multiply3x3(mat, pose2Mat, mat);
  vtkMath::Multiply3x3(mat, pose3Mat, mat);
  vtkMath::Multiply3x3(mat, orientation2Mat, mat);
  vtkMath::Multiply3x3(mat, orientation1Mat, mat);
  vtkMath::Multiply3x3(mat, orientation0Mat, mat);
  vtkMath::Multiply3x3(mat, tail, tail2);

  // YES !!!
  double quat[4] = {1., 0., 0., 0.}, quat2[4] = {1., 0., 0., 0.};
  vtkMath::MultiplyQuaternion(orientation0, quat, quat);
  vtkMath::MultiplyQuaternion(orientation1, quat, quat);
  vtkMath::MultiplyQuaternion(orientation2, quat, quat);
  vtkMath::MultiplyQuaternion(pose3, quat, quat);
  vtkMath::MultiplyQuaternion(pose2, quat, quat);
  vtkMath::MultiplyQuaternion(pose1, quat, quat);
  vtkMath::QuaternionToMatrix3x3(quat, mat);
  vtkMath::Multiply3x3(mat, tail, tail2);

  // YES !
  quat[0] = 1.; quat[1] = 0.; quat[2] = 0.; quat[3] = 0.;
  quat2[0] = 1.; quat2[1] = 0.; quat2[2] = 0.; quat2[3] = 0.;
  vtkMath::MultiplyQuaternion(orientation0, quat2, quat2);
  vtkMath::MultiplyQuaternion(orientation1, quat2, quat2);
  vtkMath::MultiplyQuaternion(orientation2, quat2, quat2);
  vtkMath::MultiplyQuaternion(pose3, quat, quat);
  vtkMath::MultiplyQuaternion(pose2, quat, quat);
  vtkMath::MultiplyQuaternion(pose1, quat, quat);
  //vtkMath::MultiplyQuaternion(quat2, quat, quat);
  vtkMath::MultiplyQuaternion(quat, quat2, quat);
  vtkMath::QuaternionToMatrix3x3(quat, mat);
  vtkMath::Multiply3x3(mat, tail, tail2);

  // YES !
  quat[0] = 1.; quat[1] = 0.; quat[2] = 0.; quat[3] = 0.;
  quat2[0] = 1.; quat2[1] = 0.; quat2[2] = 0.; quat2[3] = 0.;
  vtkMath::MultiplyQuaternion(orientation0, quat2, quat2);
  vtkMath::MultiplyQuaternion(orientation1, quat2, quat2);
  vtkMath::MultiplyQuaternion(orientation2, quat2, quat2);
  vtkMath::MultiplyQuaternion(quat, pose1, quat);
  vtkMath::MultiplyQuaternion(quat, pose2, quat);
  vtkMath::MultiplyQuaternion(quat, pose3, quat);
  //vtkMath::MultiplyQuaternion(quat2, quat, quat);
  vtkMath::MultiplyQuaternion(quat, quat2, quat);
  vtkMath::QuaternionToMatrix3x3(quat, mat);
  vtkMath::Multiply3x3(mat, tail, tail2);
  return true;
}
