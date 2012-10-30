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

#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkObjectFactory.h>
#include <vtkPolyData.h>
#include <vtkProperty.h>
#include <vtkSmartPointer.h>
#include <vtkSphereSource.h>
#include <vtkBoxWidget.h>
#include <vtkBiDimensionalRepresentation2D.h>
#include <vtkCommand.h>
#include <vtkMath.h>
#include <vtkMatrix3x3.h>
#include <vtkTransform.h>
#include <vtkPointHandleRepresentation3D.h>
#include <vtkAxesActor.h>
#include <vtkCaptionActor2D.h>
#include <vtkOrientationMarkerWidget.h>
#include <vtkNew.h>
#include <vtkTransform.h>

#include <vtkInteractorStyleTrackballCamera.h>

#include "vtkArmatureWidget.h"
#include "vtkBoneWidget.h"
#include "vtkCylinderBoneRepresentation.h"
#include "vtkDoubleConeBoneRepresentation.h"

#include <map>

void Matrix3x3ToMatrix4x4(double M[3][3], double resultM[4][4])
{
  resultM[0][0] = M[0][0];
  resultM[0][1] = M[0][1];
  resultM[0][2] = M[0][2];

  resultM[1][0] = M[1][0];
  resultM[1][1] = M[1][1];
  resultM[1][2] = M[1][2];

  resultM[2][0] = M[2][0];
  resultM[2][1] = M[2][1];
  resultM[2][2] = M[2][2];

  resultM[0][3] = 0.0;
  resultM[1][3] = 0.0;
  resultM[2][3] = 0.0;

  resultM[3][0] = 0.0;
  resultM[3][1] = 0.0;
  resultM[3][2] = 0.0;
  resultM[3][3] = 1.0;
}

void Matrix4x4ToElements(double M[4][4], double elements[16])
{
  int index = 0;
  for (int i =0; i < 4; ++i)
    {
    for (int j= 0; j < 4; ++j)
      {
      elements[index++] = M[i][j];
      }
    }
}

void AxisAngleToMatrix3x3(double axis[3], double angle, double M[3][3])
{
  // normalize the axis first (to remove unwanted scaling)
  if (vtkMath::Normalize(axis) < 1e-13)
    {
    vtkMath::Identity3x3(M);
    return;
    }
  else
    {
    // now convert this to a 3x3 matrix
    double co = cos(angle);
    double si = sin(angle);

    double ico = (1.0 - co);
    double nsi[3];
    nsi[0] = axis[0] * si;
    nsi[1] = axis[1] * si;
    nsi[2] = axis[2] * si;

    M[0][0] = ((axis[0] * axis[0]) * ico) + co;
    M[0][1] = ((axis[0] * axis[1]) * ico) + nsi[2];
    M[0][2] = ((axis[0] * axis[2]) * ico) - nsi[1];
    M[1][0] = ((axis[0] * axis[1]) * ico) - nsi[2];
    M[1][1] = ((axis[1] * axis[1]) * ico) + co;
    M[1][2] = ((axis[1] * axis[2]) * ico) + nsi[0];
    M[2][0] = ((axis[0] * axis[2]) * ico) + nsi[1];
    M[2][1] = ((axis[1] * axis[2]) * ico) - nsi[0];
    M[2][2] = ((axis[2] * axis[2]) * ico) + co;
    }

  M[0][3] = 0.0;
  M[1][3] = 0.0;
  M[2][3] = 0.0;
  M[3][0] = 0.0;
  M[3][1] = 0.0;
  M[3][2] = 0.0;
  M[3][3] = 1.0;
}

void MultiplyQuaternion(double* quad1, double* quad2, double resultQuad[4])
{
  //Quaternion are (w, x, y, z)
  //The multiplication is given by :
  //(Q1 * Q2).w = (w1w2 - x1x2 - y1y2 - z1z2)
  //(Q1 * Q2).x = (w1x2 + x1w2 + y1z2 - z1y2)
  //(Q1 * Q2).y = (w1y2 - x1z2 + y1w2 + z1x2)
  //(Q1 * Q2).z = (w1z2 + x1y2 - y1x2 + z1w2)

  resultQuad[0] = quad1[0]*quad2[0] - quad1[1]*quad2[1]
                  - quad1[2]*quad2[2] - quad1[3]*quad2[3];

  resultQuad[1] = quad1[0]*quad2[1] + quad1[1]*quad2[0]
                  + quad1[2]*quad2[3] - quad1[3]*quad2[2];

  resultQuad[2] = quad1[0]*quad2[2] + quad1[2]*quad2[0]
                  + quad1[3]*quad2[1] - quad1[1]*quad2[3];

  resultQuad[3] = quad1[0]*quad2[3] + quad1[3]*quad2[0]
                  + quad1[1]*quad2[2] - quad1[2]*quad2[1];
}

void ConjugateQuaternion(double* quad, double resultQuad[4])
{
  //Quaternion are (w, x, y, z)
  //The conjugate is (w, -x, -y, -z);

  resultQuad[0] = quad[0];
  resultQuad[1] = -quad[1];
  resultQuad[2] = -quad[2];
  resultQuad[3] = -quad[3];
}

double NormQuaternion(double* quad)
{
  //Quaternion are (w, x, y, z)
  return sqrt(quad[0]*quad[0]
              + quad[1]*quad[1]
              + quad[2]*quad[2]
              + quad[3]*quad[3]);
}

double NormalizeQuaternion(double* quad)
{
  //Quaternion are (w, x, y, z)
  double norm = sqrt(quad[0]*quad[0]
                     + quad[1]*quad[1]
                     + quad[2]*quad[2]
                     + quad[3]*quad[3]);

  if (norm > 0.0)
    {
    quad[0] /= norm;
    quad[1] /= norm;
    quad[2] /= norm;
    quad[3] /= norm;
    }

  return norm;
}

void InverseQuaternion(double* quad, double resultQuad[4])
{
  //Quaternion are (w, x, y, z)
  ConjugateQuaternion(quad, resultQuad);
  NormalizeQuaternion(resultQuad);
}

void RotateVectorWithQuaternion(double vec[3], double quad[4], double resultVec[3])
{
  //Quaternion are (w, x, y, z)
  //vec = q*vec*conjugate(q)
  double v[3];
  v[0] = vec[0];
  v[1] = vec[1];
  v[2] = vec[2];

  double vecQuad[4], resultQuad[4], conjugateQuad[4];
  vecQuad[0] = 0.0;
  vecQuad[1] = v[0];
  vecQuad[2] = v[1];
  vecQuad[3] = v[2];

  ConjugateQuaternion(quad, conjugateQuad);

  MultiplyQuaternion(vecQuad, conjugateQuad, resultQuad);
  MultiplyQuaternion(quad, resultQuad, resultQuad);

  resultVec[0] = resultQuad[1];
  resultVec[1] = resultQuad[2];
  resultVec[2] = resultQuad[3];
}

void AxisAngleToQuaternion(double axis[3], double angle, double quad[4])
{
  quad[0] = cos( angle / 2.0 );
  double f = sin( angle / 2.0);

  quad[1] =  axis[0] * f;
  quad[2] =  axis[1] * f;
  quad[3] =  axis[2] * f;
}

/*
----------------------FROM HERE ------------------

p1 0 0 0
p2 0.2 0 -0.1
lineVect 0.894427 0 -0.447214
rotationAxis -0.447214 0 -0.894427
angle 1.5708
Angle different !
 - Got 1.5708
 - Got:    -0.447214 0 -0.894427

*/

class Test1KeyPressInteractorStyle : public vtkInteractorStyleTrackballCamera
{
  public:
    static Test1KeyPressInteractorStyle* New();
    vtkTypeMacro(Test1KeyPressInteractorStyle, vtkInteractorStyleTrackballCamera);

    virtual void OnKeyPress()
      {
      vtkRenderWindowInteractor *rwi = this->Interactor;
      std::string key = rwi->GetKeySym();
      std::cout<<"Key Pressed: "<<key<<std::endl;

      if (key == "Shift_L")
        {
        std::cout<<"Changing representation !"<<std::endl;

        // Cycling through the representations
        int newRepresentationType = Armature->GetBonesRepresentationType() + 1;
        if (newRepresentationType > vtkArmatureWidget::DoubleCone
            || newRepresentationType < vtkArmatureWidget::Bone) //last case is case no representation
          {
          newRepresentationType = 0;
          }
        Armature->SetBonesRepresentation(newRepresentationType);
        }

      //if (key == "a")
        //{
        //vtkSmartPointer<vtkTransform> transform =
        //  vtkSmartPointer<vtkTransform>::New();
        //transform->Translate(Armature->GetBone(1)->GetParentToBoneRestTranslation());

        //transform->Concatenate(Armature->GetBone(1)->CreateParentToBoneRestRotation());

        //Axes->SetUserTransform(transform);
        //}
      if (key == "Control_L")
        {
        Armature->SetWidgetState( ! Armature->GetWidgetState() );
        }
      else if (key == "Tab")
        {
        int state = Armature->GetAxesVisibility() + 1;
        if (state > vtkBoneWidget::ShowPoseTransform)
          {
          state = 0;
          }
        Armature->SetAxesVisibility(state);
        }
      }

  vtkArmatureWidget* Armature;
  //vtkAxesActor* Axes;
};

vtkStandardNewMacro(Test1KeyPressInteractorStyle);

void printNormal(double axis[3])
{
  double Y[3] = {0.0, 1.0, 0.0}, normal[3];
  vtkMath::Cross(Y, axis, normal);
  vtkMath::Normalize(normal);
  std::cout<<normal[0] <<" "<< normal[1] <<" "<< normal[2] <<std::endl;
}

int vtkBoneWidgetTwoBonesTestRotationMatrix(int, char *[])
{
/* double axis[3];

   axis[0] = 0.0; axis[1] =-0.999534; axis[2] = 0.001;
  printNormal(axis);

  axis[0] = 0.0; axis[1] = -0.999534; axis[2] = 0.0001;
  printNormal(axis);

  axis[0] = 0.0; axis[1] =-0.999534; axis[2] = 0.00001;
  printNormal(axis);

  axis[0] = 0.0; axis[1] =-0.999534; axis[2] = 0.00001;
  printNormal(axis);

  axis[0] = 0.0; axis[1] = -0.999534; axis[2] = 0.000001;
  printNormal(axis);

  axis[0] = -0.0171035; axis[1] = -0.999413; axis[2] = -0.0296692;
  std::cout<<vtkMath::Norm(axis)<<std::endl;
  printNormal(axis);

lineVect -0.0171035 -0.999413 -0.0296692
rotationAxis -0.866354 0 0.49943*/

  vtkSmartPointer<vtkRenderer> renderer =
    vtkSmartPointer<vtkRenderer>::New();
  vtkSmartPointer<vtkRenderWindow> renderWindow =
    vtkSmartPointer<vtkRenderWindow>::New();
  renderWindow->AddRenderer(renderer);

  // An interactor
  vtkSmartPointer<vtkRenderWindowInteractor> renderWindowInteractor =
    vtkSmartPointer<vtkRenderWindowInteractor>::New();
  renderWindowInteractor->SetRenderWindow(renderWindow);

  vtkSmartPointer<vtkArmatureWidget> armature =
    vtkSmartPointer<vtkArmatureWidget>::New();
  armature->SetInteractor(renderWindowInteractor);
  armature->SetCurrentRenderer(renderer);
  armature->CreateDefaultRepresentation();
  armature->SetBonesRepresentation(vtkArmatureWidget::Bone);
  armature->SetWidgetState(vtkArmatureWidget::Rest);

  vtkBoneWidget* arm = armature->CreateBone(NULL);
  armature->AddBone(arm, NULL, "Arm");
  arm->SetWorldHeadRest(0.0, 0.0, 0.0);
  arm->SetWorldTailRest(10.0, 0.0, 0.0);
  vtkSmartPointer<vtkCylinderBoneRepresentation> armRep =
    vtkSmartPointer<vtkCylinderBoneRepresentation>::New();
  arm->SetRepresentation(armRep);
  armRep->GetCylinderProperty()->SetOpacity(0.4);

  vtkBoneWidget* forearm = armature->CreateBone(arm);
  armature->AddBone(forearm, arm, 20.0, 0.0, 0.0);
  vtkSmartPointer<vtkDoubleConeBoneRepresentation> forearmRep =
    vtkSmartPointer<vtkDoubleConeBoneRepresentation>::New();
  forearm->SetRepresentation(forearmRep);
  forearmRep->GetConesProperty()->SetOpacity(0.4);

  vtkBoneWidget* thumb = armature->CreateBone(forearm);
  armature->AddBone(thumb, forearm, 20.0, 4.0, 0.0);
  thumb->SetAxesVisibility(vtkBoneWidget::ShowPoseTransform);

  vtkBoneWidget* indexFinger = armature->CreateBone(forearm);
  armature->AddBone(indexFinger, forearm, 22.0, 2.0, 0.0);
  indexFinger->SetAxesVisibility(vtkBoneWidget::ShowPoseTransform);

  vtkBoneWidget* middleFinger = armature->CreateBone(forearm);
  armature->AddBone(middleFinger, forearm, 22.0, 1.0, 0.0);
  middleFinger->SetAxesVisibility(vtkBoneWidget::ShowPoseTransform);

  vtkBoneWidget* ringFinger = armature->CreateBone(forearm);
  armature->AddBone(ringFinger, forearm, 22.0, -1.0, 0.0);
  ringFinger->SetAxesVisibility(vtkBoneWidget::ShowPoseTransform);

  vtkBoneWidget* littleFinger = armature->CreateBone(forearm);
  armature->AddBone(littleFinger, forearm, 22.0, -2.0, 0.0);
  littleFinger->SetAxesVisibility(vtkBoneWidget::ShowPoseTransform);

  armature->SetWidgetState(vtkArmatureWidget::Pose);

  vtkSmartPointer<Test1KeyPressInteractorStyle> style =
    vtkSmartPointer<Test1KeyPressInteractorStyle>::New();
  renderWindowInteractor->SetInteractorStyle(style);
  style->Armature = armature;

  vtkSmartPointer<vtkAxesActor> axes =
    vtkSmartPointer<vtkAxesActor>::New();

  vtkSmartPointer<vtkOrientationMarkerWidget> axesWidget =
    vtkSmartPointer<vtkOrientationMarkerWidget>::New();
  axesWidget->SetOrientationMarker( axes );
  axesWidget->SetInteractor( renderWindowInteractor );
  axesWidget->On();

  // Render
  renderWindow->Render();
  renderWindowInteractor->Initialize();
  renderWindow->Render();
  armature->On();

  // Begin mouse interaction
  renderWindowInteractor->Start();

  return EXIT_SUCCESS;
}

