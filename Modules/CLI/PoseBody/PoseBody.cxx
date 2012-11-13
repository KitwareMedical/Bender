/*=========================================================================

  Program:   Insight Segmentation & Registration Toolkit
  Module:    $HeadURL$
  Language:  C++
  Date:      $Date$
  Version:   $Revision$

  Copyright (c) Insight Software Consortium. All rights reserved.
  See ITKCopyright.txt or http://www.itk.org/HTML/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "PoseBodyCLP.h"

#include "itkImageFileWriter.h"
#include "itkImage.h"
#include <itkStatisticsImageFilter.h>
#include "itkPluginUtilities.h"
#include "itkImageRegionIteratorWithIndex.h"
#include "vtkPolyDataReader.h"
#include "vtkPolyDataWriter.h"
#include "itkContinuousIndex.h"
#include "itkMath.h"
#include "itkIndex.h"
#include "itkLinearInterpolateImageFunction.h"
#include "itkVariableLengthVector.h"
#include "itkMatrix.h"

#include "vtkTimerLog.h"
#include "vtkSTLReader.h"
#include "vtkPolyData.h"
#include "vtkCellArray.h"
#include "vtkNew.h"
#include "vtkSmartPointer.h"
#include "vtkPointData.h"
#include "vtkCellData.h"
#include "vtkFloatArray.h"
#include "vtkMath.h"
#include "vtkCubeSource.h"
#include "itkVersor.h"

#include <sstream>
#include <iostream>
#include <vector>
#include <limits>



// Move me!
//----------------------------------------------------------------------


#include <math.h>

// input: unit quaternion 'q0', translation vector 't'
// output: unit dual quaternion 'dq'
void QuatTrans2UDQ(const double q0[4], const double t[3],
                  double dq[2][4])
{
   // non-dual part (just copy q0):
   for (int i=0; i<4; i++) dq[0][i] = q0[i];
   // dual part:
   dq[1][0] = -0.5*(t[0]*q0[1] + t[1]*q0[2] + t[2]*q0[3]);
   dq[1][1] = 0.5*( t[0]*q0[0] + t[1]*q0[3] - t[2]*q0[2]);
   dq[1][2] = 0.5*(-t[0]*q0[3] + t[1]*q0[0] + t[2]*q0[1]);
   dq[1][3] = 0.5*( t[0]*q0[2] - t[1]*q0[1] + t[2]*q0[0]);
}

// input: unit dual quaternion 'dq'
// output: unit quaternion 'q0', translation vector 't'
void UDQ2QuatTrans(const double dq[2][4],
                  double q0[4], double t[3])
{
   // regular quaternion (just copy the non-dual part):
   for (int i=0; i<4; i++) q0[i] = dq[0][i];
   // translation vector:
   t[0] = 2.0*(-dq[1][0]*dq[0][1] + dq[1][1]*dq[0][0] - dq[1][2]*dq[0][3] + dq[1][3]*dq[0][2]);
   t[1] = 2.0*(-dq[1][0]*dq[0][2] + dq[1][1]*dq[0][3] + dq[1][2]*dq[0][0] - dq[1][3]*dq[0][1]);
   t[2] = 2.0*(-dq[1][0]*dq[0][3] - dq[1][1]*dq[0][2] + dq[1][2]*dq[0][1] + dq[1][3]*dq[0][0]);
}

// input: dual quat. 'dq' with non-zero non-dual part
// output: unit quaternion 'q0', translation vector 't'
void DQ2QuatTrans(const double dq[2][4],
                  double q0[4], double t[3])
{
   double len = 0.0;
   for (int i=0; i<4; i++) len += dq[0][i] * dq[0][i];
   len = sqrt(len);
   for (int i=0; i<4; i++) q0[i] = dq[0][i] / len;
   t[0] = 2.0*(-dq[1][0]*dq[0][1] + dq[1][1]*dq[0][0] - dq[1][2]*dq[0][3] + dq[1][3]*dq[0][2]) / len;
   t[1] = 2.0*(-dq[1][0]*dq[0][2] + dq[1][1]*dq[0][3] + dq[1][2]*dq[0][0] - dq[1][3]*dq[0][1]) / len;
   t[2] = 2.0*(-dq[1][0]*dq[0][3] - dq[1][1]*dq[0][2] + dq[1][2]*dq[0][1] + dq[1][3]*dq[0][0]) / len;
}


//----------------------------------------------------------------------



using namespace std;

typedef unsigned char CharType;
typedef unsigned short LabelType;

typedef itk::Image<unsigned short, 3>  LabelImage;
typedef itk::Image<float, 3>  WeightImage;
typedef itk::Image<bool, 3>  BoolImage;

typedef itk::Index<3> Voxel;
typedef itk::Offset<3> VoxelOffset;
typedef itk::ImageRegion<3> Region;

typedef itk::VariableLengthVector<float> WeightVector;

typedef itk::Versor<double> Versor;
typedef itk::Matrix<double,3,3> Mat33;
typedef itk::Matrix<double,4,4> Mat44;

typedef itk::Vector<double,3> Vec3;
typedef itk::Vector<double,4> Vec4;

inline void SetToIdentityQuarternion(Vec4& v)
{
  v[0] = 1.0;
  v[1] = 0;
  v[2] = 0;
  v[3] = 0;
}
//-----------------------------------------------------------------------------
template<class T>
void PrintVector(T* a, int n)
{
  cout<<"[";
  for(int i=0; i<n; i++)
    {
    cout<<a[i]<<(i==n-1?"": ", ");
    }
  cout<<"]"<<endl;
}

void PrintVTKQuarternion(double* a)
{
  cout<<"[ ";
  cout<<a[1]<<", "<<a[2]<<", "<<a[3]<<", "<<a[0];
  cout<<" ]"<<endl;
}

Vec4 ComputeQuarternion(double axisX,
                        double axisY,
                        double axisZ,
                        double angle)
{
  Vec4 R;
  double c = cos(angle);
  double s = sin(angle);
  R[0] = c;
  R[1] = s*axisX;
  R[2] = s*axisY;
  R[3] = s*axisZ;
  return R;
}

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


Mat33 ToItkMatrix(double M[3][3])
{
  Mat33 itkM;
  for(int i=0; i<3; i++)
    {
    for(int j=0; j<3; j++)
      {
      itkM(i,j)  = M[i][j];
      }
    }

  return itkM;
}

inline Mat33 ToRotationMatrix(const Vec4& R)
{
  Versor v;
  v.Set(R[1],R[2], R[3],R[0]);
  return v.GetMatrix();
}


struct RigidTransform
{
  Vec3 O;
  Vec3 T;
  Vec4 R; //rotation quarternion
  RigidTransform()
  {
    //initialize to identity transform
    T[0] = T[1] = T[2] = 0.0;

    R[0] = 1.0;
    R[1] = R[2] = R[3] = 0.0;

    O[0]=O[1]=O[2]=0.0;
  }

  void SetRotation(double* M)
  {
    vtkMath::Matrix3x3ToQuaternion((double (*)[3])M, &R[0]);
  }
  void SetRotation(double axisX,
                   double axisY,
                   double axisZ,
                   double angle)
  {
    double c = cos(angle);
    double s = sin(angle);
    this->R[0] = c;
    this->R[1] = s*axisX;
    this->R[2] = s*axisY;
    this->R[3] = s*axisZ;
  }

  void SetRotationCenter(const double* center)
  {
    this->O = Vec3(center);
  }

  void SetTranslation(double* t)
  {
    this->T = Vec3(t);
  }
  void Apply(const double in[3], double out[3]) const
  {
    Vec3 x(in);
    x = ToRotationMatrix(this->R)*(x-this->O) + this->O +T;
    out[0] =x[0];
    out[1] =x[1];
    out[2] =x[2];
  }
};

void GetArmatureTransform(vtkPolyData* polyData, vtkIdType cellId, const char* arrayName, const double* rcenter, RigidTransform& F,bool invertY =true)
{
  double A[12];
  polyData->GetCellData()->GetArray(arrayName)->GetTuple(cellId, A);

  double R[3][3];
  double T[3];
  double RCenter[3];
  int iA(0);
  for(int i=0; i<3; i++)
    {
    for(int j=0; j<3; j++,iA++)
      {
      R[j][i] = A[iA];
      }
    }

  for(int i=0; i<3; i++)
    {
    T[i] = A[i+9];
    RCenter[i] = rcenter[i];
    }

  if(invertY)
    {
    for(int i=0; i<3; i++)
      {
      for(int j=0; j<3; j++)
        {
        if( (i==1 || j==1) && i!=j)
          {
          R[i][j]*=-1;
          }
        }
      }
    T[1]*=-1;
    RCenter[1]*=-1;
    }

  F.SetRotation(&R[0][0]);
  F.SetRotationCenter(RCenter);
  F.SetTranslation(T);
}

vtkSmartPointer<vtkPolyData> TransformArmature(vtkPolyData* armature,  const char* arrayName, bool invertY)
{
  vtkSmartPointer<vtkPolyData> output = vtkSmartPointer<vtkPolyData>::New();
  output->DeepCopy(armature);

  vtkPoints* inPoints = armature->GetPoints();
  vtkPoints* outPoints = output->GetPoints();

  vtkCellArray* armatureSegments = armature->GetLines();
  vtkCellData* armatureCellData = armature->GetCellData();
  vtkNew<vtkIdList> cell;
  armatureSegments->InitTraversal();
  int edgeId(0);
  while(armatureSegments->GetNextCell(cell.GetPointer()))
    {
    vtkIdType a = cell->GetId(0);
    vtkIdType b = cell->GetId(1);

    double A[12];
    armatureCellData->GetArray(arrayName)->GetTuple(edgeId, A);

    Mat33 R;
    int iA(0);
    for(int i=0; i<3; i++)
      for(int j=0; j<3; j++,iA++)
        {
        R(i,j) = A[iA];
        }

    R = R.GetTranspose();
    Vec3 T;
    T[0] = A[9];
    T[1] = A[10];
    T[2] = A[11];

    if(invertY)
      {
      //    Mat33 flipY;
      for(int i=0; i<3; i++)
        for(int j=0; j<3; j++)
          {
          if( (i==1 || j==1) && i!=j)
            {
            R(i,j)*=-1;
            }
//          flipY(i,j) = (i==1 && j==1) ? -1 : (i==j? 1 : 0);
          }

//      R = flipY*R*flipY;
      T[1]*=-1;
      }


    Vec3 ax(inPoints->GetPoint(a));
    Vec3 bx(inPoints->GetPoint(b));
    Vec3 ax1;
    Vec3 bx1;
    if(!invertY)
      {
      ax1 = R*(ax-ax)+ax+T;
      bx1 = R*(bx-ax)+ax+T;
      }
    else
      {
      ax[1]*=-1;
      bx[1]*=-1;
      ax1 = R*(ax-ax)+ax+T;
      bx1 = R*(bx-ax)+ax+T;
      }

    cout<<"Set point "<<a<<" to "<<Vec3(ax1)<<endl;
    outPoints->SetPoint(a,&ax1[0]);

    cout<<"Set point "<<b<<" to "<<Vec3(bx1)<<endl;
    outPoints->SetPoint(b,&bx1[0]);

    edgeId++;
    }
  return output;
}


vtkSmartPointer<vtkPolyData> TransformArmature(vtkPolyData* armature,  const std::vector<RigidTransform>& transforms)
{
  vtkSmartPointer<vtkPolyData> output = vtkSmartPointer<vtkPolyData>::New();
  output->DeepCopy(armature);

  vtkPoints* inPoints = armature->GetPoints();
  vtkPoints* outPoints = output->GetPoints();

  vtkCellArray* armatureSegments = armature->GetLines();
  vtkNew<vtkIdList> cell;
  armatureSegments->InitTraversal();
  int edgeId(0);
  while(armatureSegments->GetNextCell(cell.GetPointer()))
    {
    vtkIdType a = cell->GetId(0);
    vtkIdType b = cell->GetId(1);

    double ax[3]; inPoints->GetPoint(a,ax);
    double bx[3]; inPoints->GetPoint(b,bx);

    double ax1[3];
    double bx1[3];
    transforms[edgeId].Apply(ax,ax1);
    transforms[edgeId].Apply(bx,bx1);

    outPoints->SetPoint(a,ax1);
    outPoints->SetPoint(b,bx1);
    edgeId++;
    }
  return output;
}

//-----------------------------------------------------------------------------

void NormalizeWeight(WeightVector& v)
{
  for(unsigned int i=0; i<v.Size();i++)
    {
    assert(v[i]>=0);
    if(v[i]<0.001)
      {
      v[i]=0;
      }
    }

  float s(0);
  for(unsigned int i=0; i<v.Size();i++)
    {
    assert(v[i]>=0);
    s+=v[i];
    }
  s = s!=0 ? 1.0/s : 0;
  for(unsigned int i=0; i<v.Size();i++)
    {
    v[i]*=s;
    }
}
class CubeNeighborhood
{
public:
  CubeNeighborhood()
  {
    int index=0;
    for(unsigned int i=0; i<=1; i++)
      {
      for(unsigned int j=0; j<=1; j++)
        {
        for(unsigned int k=0; k<=1; k++,index++)
          {
          this->Offsets[index][0] = i;
          this->Offsets[index][1] = j;
          this->Offsets[index][2] = k;
          }
        }
      }
  }
  VoxelOffset Offsets[8];
};


class Neighborhood27
{
public:
  Neighborhood27()
  {
    int index=0;
    for(unsigned int i=0; i<=2; i++)
      {
      for(unsigned int j=0; j<=2; j++)
        {
        for(unsigned int k=0; k<=2; k++,index++)
          {
          this->Offsets[index][0] = i;
          this->Offsets[index][1] = j;
          this->Offsets[index][2] = k;
          }
        }
      }
    assert(index==27);
  }
  VoxelOffset Offsets[27];
};

vtkPolyData* ReadPolyData(const std::string& fileName, bool invertY=false)
{
  vtkPolyData* polyData = 0;
  vtkNew<vtkPolyDataReader> stlReader;
  stlReader->SetFileName(fileName.c_str() );
  stlReader->Update();
  polyData = stlReader->GetOutput();
  polyData->Register(0);

  vtkPoints* points = polyData->GetPoints();
  if(invertY)
    {
    cout<<"Invert y coordinate\n";
    for(int i=0; i<points->GetNumberOfPoints();i++)
      {
      double x[3];
      points->GetPoint(i,x);
      x[1]*=-1;
      points->SetPoint(i, x);
      }
    }
  return polyData;
}

//-----------------------------------------------------------------------------
void WritePolyData(vtkPolyData* polyData, const std::string& fileName)
{
  vtkNew<vtkPolyDataWriter> pdWriter;
  pdWriter->SetInput(polyData);
  pdWriter->SetFileName(fileName.c_str() );
  pdWriter->SetFileTypeToBinary();
  pdWriter->Update();
}

typedef unsigned char SiteIndex;
#define MAX_SITE_INDEX 255
struct WeightEntry
{
  SiteIndex Index;
  float Value;

  WeightEntry(): Index(MAX_SITE_INDEX), Value(0){}
};

class WeightMap
{
public:
  typedef std::vector<SiteIndex> RowSizes;
  typedef std::vector<WeightEntry> WeightEntries;

  typedef std::vector<WeightEntries> WeightLUT; //for any j, WeightTable[...][j] correspond to
  //the weights at a vixel

  typedef itk::Image<size_t,3> WeightLUTIndex; //for each voxel v, WeightLUTIndex[v] index into the
                                           //the "column" of WeightLUT

  WeightMap():Cols(0)
  {
  }
  void Init(const std::vector<Voxel>& voxels, const Region& region)
  {
    this->Cols = voxels.size();
    this->RowSize.resize(this->Cols,0);

    this->LUTIndex = WeightLUTIndex::New();
    this->LUTIndex->SetRegions(region);
    this->LUTIndex->Allocate();

    itk::ImageRegionIterator<WeightLUTIndex> it(this->LUTIndex,region);
    for(it.GoToBegin(); !it.IsAtEnd(); ++it)
      {
      it.Set(std::numeric_limits<std::size_t>::max());
      }

    for(size_t j=0; j<voxels.size(); j++)
      {
      this->LUTIndex->SetPixel(voxels[j], j);
      }
  }

  bool Insert(const Voxel& v, SiteIndex index, float value)
  {
    if(value<=0)
      {
      return false;
      }
    size_t j = this->LUTIndex->GetPixel(v);
    assert(j<Cols);
    size_t i = this->RowSize[j];
    if (i>=LUT.size())
      {
      this->AddRow();
      }
    WeightEntry& weight = LUT[i][j];
    weight.Index = index;
    weight.Value = value;

    this->RowSize[j]++;
    return true;
  }

  void Get(const Voxel& v, WeightVector& values)
  {
    values.Fill(0);
    size_t j = this->LUTIndex->GetPixel(v);
    assert(j<Cols);

    size_t rows = this->RowSize[j];

    for(size_t i=0; i<rows; i++)
      {
      const WeightEntry& entry = LUT[i][j];
      values[entry.Index] = entry.Value;

      }
  }

  void AddRow()
  {
    LUT.push_back(WeightEntries());
    WeightEntries& newRow = LUT.back();
    newRow.resize(Cols);
  }

  void Print()
  {
    int numEntries(0);
    for(RowSizes::iterator r=this->RowSize.begin();r!=this->RowSize.end();r++)
      {
      numEntries+=*r;
      }
    cout<<"Weight map "<<LUT.size()<<"x"<<Cols<<" has "<<numEntries<<" entries"<<endl;
  }

private:
  WeightLUT LUT;
  WeightLUTIndex::Pointer LUTIndex;
  RowSizes RowSize;
  size_t Cols;
};

void TestQuarternion()
{

  double A[3][3] = {{1., 0., 0.},{0., 1., 0.},{0., 0., 1.}};
  double AQuat[4];
  double A1[3][3];
  vtkMath::Matrix3x3ToQuaternion(A, AQuat);
  vtkMath::QuaternionToMatrix3x3(AQuat, A1);

  for(int i=0; i<3; i++)
    {
    for(int j=0; j<3; j++)
      {
      assert(fabs(A1[i][j]-A[i][j])<0.001);
      }
    }

  double B[3][3] = {{0,-1, 0},
                    {1, 0, 0},
                    {0, 0, 1}};
  double BQuat[4];
  vtkMath::Matrix3x3ToQuaternion(B, BQuat);
}


void TestDualQuarternion()
{
  Vec4 q = ComputeQuarternion(0,0,1,3.14/4);
  cout<<q<<endl;

  Vec3 t;
  t[0] = 0.0;
  t[1] = 1.0;
  t[2] = 0.0;

  double dq[2][4];
  QuatTrans2UDQ(&q[0],&t[0],dq);


  Vec4 q1;
  Vec3 t1;
  DQ2QuatTrans(dq,&q1[0],&t1[0]);

  cout<<q1<<endl;
  cout<<t1<<endl;


}



void TestVersor()
{
  double A[3][3] = {{1., 0., 0.},{0., 1., 0.},{0., 0., 1.}};
  double B[3][3] = {{0,-1, 0},
                    {1, 0, 0},
                    {0, 0, 1}};

  Mat33 MA = ToItkMatrix(A);
  Mat33 MB = ToItkMatrix(B);

  Versor va,vb;
  va.Set(MA);
  vb.Set(MB);

  double qa[4],qb[4];
  vtkMath::Matrix3x3ToQuaternion(A, qa);
  vtkMath::Matrix3x3ToQuaternion(B, qb);

  for(double t=0; t<1.0; t+=0.1)
    {
    Versor vt = vb.Exponential(t);
    double qt[4];
    InterpolateQuaternion(qa,qb,t,qt);
    assert(fabs(qt[1]-vt.GetX())<0.0001);
    assert(fabs(qt[2]-vt.GetY())<0.0001);
    assert(fabs(qt[3]-vt.GetZ())<0.0001);
    assert(fabs(qt[0]-vt.GetW())<0.0001);
    }

}

void TestTransformBlending()
{
  if(1)
    {
    RigidTransform A;
    double AR[3][3];
    vtkMath::QuaternionToMatrix3x3(&A.R[0], AR);

    double x[3] = {1,2,3};
    double y[3];
    A.Apply(x,y);
    for(int i=0; i<3; i++)
      {
      assert(x[i]==y[i]);
      }
    }

  // vtkSmartPointer<vtkPolyData> cube;
  // cube.TakeReference(ReadPolyData("/home/leo/temp/cube88.vtk"));

  // vtkNew<vtkPoints> points0;
  // points0->DeepCopy(cube->GetPoints());


  // RigidTransform A;
  // A.R[0] = 1; A.R[1] = 0; A.R[2] = 0; A.R[3] = 1;
  // A.R.Normalize();
  // cout<<A.R<<endl;
  // RigidTransform B;
  // vtkPoints* points =  cube->GetPoints();

  // for(int i=0; i<points->GetNumberOfPoints(); i++)
  //   {
  //   double x[3],y[3];
  //   points0->GetPoint(i,x);
  //   double t= x[2] + 0.5;
  //   assert(t>=0 && t<=1);

  //   RigidTransform F;
  //   F.R = t*A.R + (1-t)*B.R;
  //   F.T = t*A.T + (1-t)*B.T;
  //   F.R.Normalize();

  //   F.Apply(x,y);
  //   points->SetPoint(i,y);
  //   }
  // std::stringstream fname;
  // fname<<"/home/leo/temp/cubetwisted.vtk";
  // cout<<"TestTransformBlending wrote to "<<fname.str()<<endl;
  // WritePolyData(cube,fname.str());
}
void TestVector()
{
  WeightVector a(3);
  a.Fill(0);
  a[1]=2;
  a.Fill(1);
}
void TestInterpolation()
{
  typedef itk::Image<float, 2>  ImageType;
  ImageType::Pointer image = ImageType::New();

  double origin[2] = {1.5,2.5};
  double spacing[2] = {0.5,0.5};

  image->SetOrigin(origin);
  image->SetSpacing(spacing);

  ImageType::RegionType region;
  ImageType::IndexType start={{0,0}};
  ImageType::SizeType size={{2,2}};
  region.SetIndex(start);
  region.SetSize(size);

  image->SetRegions(region);
  image->Allocate();
  ImageType::IndexType ij;
  for(ij[0]=0; ij[0]<2; ij[0]++)
    {
    for(ij[1]=0; ij[1]<2; ij[1]++)
      {
      image->SetPixel(ij, double(ij[0]+ij[1]));
      }
    }


  itk::Point<float,2> p;
  p[0] = 1.9;
  p[1] = 2.9;

  itk::ContinuousIndex<float,2> coord;
  image->TransformPhysicalPointToContinuousIndex(p, coord);

  ImageType::IndexType baseIndex;
  float distance[2];
  for(int dim = 0; dim < 2; dim++ )
    {
    baseIndex[dim] = itk::Math::Floor<ImageType::IndexValueType>( coord[dim] );
    distance[dim] = coord[dim] - static_cast<float >( baseIndex[dim] );
    }

  assert(fabs(distance[0]-0.8) < 0.001);
  assert(fabs(distance[1]-0.8) < 0.001);

  itk::LinearInterpolateImageFunction<ImageType, float>::Pointer interpolator = itk::LinearInterpolateImageFunction<ImageType, float>::New();
  interpolator->SetInputImage(image);


  //for each cube vertex
  double value = 0;
  for(unsigned int index=0; index<4; index++)
    {
    //for each bit of index
    unsigned int bit = index;
    double w=1.0;
    ImageType::IndexType ij;
    for(int dim=0; dim<2; dim++)
      {
      bool upper = bit & 1;
      bit>>=1;
      float t = coord[dim]-static_cast<float>(baseIndex[dim]);
      w*= upper? t : 1-t;
      ij[dim] = baseIndex[dim]+ static_cast<int>(upper);
      }
    value+= w*image->GetPixel(ij);
    }

  assert(fabs(value-interpolator->EvaluateAtContinuousIndex(coord))<0.001);


}

BoolImage::Pointer CreateBodyDomain(LabelImage::Pointer bodyMap,bool expandOnce=true)
{
  Neighborhood27 neighbors;
  VoxelOffset* offsets = neighbors.Offsets ;

  Region region = bodyMap->GetLargestPossibleRegion();
  BoolImage::Pointer domain = BoolImage::New();
  domain->SetRegions(region);
  domain->Allocate();
  domain->FillBuffer(false);
  itk::ImageRegionIteratorWithIndex<LabelImage> it(bodyMap,region);
  int numBodyVoxels(0);
  for(it.GoToBegin(); !it.IsAtEnd(); ++it)
    {
    if(it.Get()>0)
      {
      numBodyVoxels++;
      Voxel p = it.GetIndex();
      domain->SetPixel(p,true);

      if(expandOnce)
        {
        p[0]--;
        p[1]--;
        p[2]--;
        for(int iOff=0; iOff<27; iOff++)
          {
          Voxel q = p + offsets[iOff];
          domain->SetPixel(q,true);
          }
        }
      }
    }

  cout<<numBodyVoxels<<" body voxels\n";
  return domain;
}

void ReadWeights(std::string dirName,
                 int numSites,
                 LabelImage::Pointer bodyMap,
                 WeightMap& weightMap, bool test=false)
{
  itk::ImageRegionIteratorWithIndex<LabelImage> it(bodyMap,bodyMap->GetLargestPossibleRegion());
  typedef std::vector<Voxel> Voxels;
  Voxels bodyVoxels;
  for(it.GoToBegin(); !it.IsAtEnd(); ++it)
    {
    if(it.Get()>0)
      {
      bodyVoxels.push_back(it.GetIndex());
      }
    }
  cout<<bodyVoxels.size()<<" body domain voxels\n";
  weightMap.Init(bodyVoxels,bodyMap->GetLargestPossibleRegion());

  if(test)
    {
    assert(numSites==1);
    }
  for(int i=0; i<numSites; i++)
    {
    std::stringstream filename;
    if(test)
      {
      filename<<dirName<<"/"<<"weight_test.mha";
      }
    else
      {
      filename<<dirName<<"/"<<"weight_"<<i<<".mha";
      }
    cout<<"Read weight: "<<filename.str()<<endl;

    typedef itk::ImageFileReader<WeightImage>  ReaderType;
    typename ReaderType::Pointer reader = ReaderType::New();
    reader->SetFileName(filename.str().c_str());
    reader->Update();
    WeightImage::Pointer weight_i =  reader->GetOutput();

    int numInserted(0);
    for(Voxels::iterator v_iter = bodyVoxels.begin(); v_iter!=bodyVoxels.end(); v_iter++)
      {
      const Voxel& v(*v_iter);
      SiteIndex index = static_cast<SiteIndex>(i);
      float value = weight_i->GetPixel(v);
      bool inserted = weightMap.Insert(v,index,value);
      numInserted+= inserted;
      }
    cout<<numInserted<<" inserted to weight map"<<endl;
    weightMap.Print();
    }
  weightMap.Print();

}

int main( int argc, char * argv[] )
{
  TestDualQuarternion();
  return 0;

  PARSE_ARGS;

  Vec4 Q0;
  SetToIdentityQuarternion(Q0);
  TestTransformBlending();
  TestVersor();
//  TestQuarternion();
  TestVector();
  TestInterpolation();

  typedef itk::ImageFileReader<LabelImage>  ReaderType;
//  typedef itk::ImageFileWriter<OutputImageType> WriterType;


  //----------------------------
  // Read label map
  //----------------------------
  typename ReaderType::Pointer labelMapReader = ReaderType::New();
  itk::PluginFilterWatcher watchLabelMapReader(labelMapReader, "Read Label Map",
                                        CLPProcessInformation);
  labelMapReader->SetFileName(RestLabelmap.c_str() );
  labelMapReader->Update();
  LabelImage::Pointer bodyMap = labelMapReader->GetOutput();

  //----------------------------
  // Read armature
  //----------------------------
  int numSites(0);
  std::vector<RigidTransform> transforms;

  vtkSmartPointer<vtkPolyData> armature;
  armature.TakeReference(ReadPolyData(ArmaturePoly.c_str(),false));

  if(1)
    {
    WritePolyData(TransformArmature(armature,"Transforms",true),"/home/leo/temp/test.vtk");
    }



  vtkCellArray* armatureSegments = armature->GetLines();
  vtkCellData* armatureCellData = armature->GetCellData();
  vtkNew<vtkIdList> cell;
  armatureSegments->InitTraversal();
  int edgeId(0);
  cout<<"# components: "<<armatureCellData->GetArray("Transforms")->GetNumberOfComponents()<<endl;
  while(armatureSegments->GetNextCell(cell.GetPointer()))
    {
    vtkIdType a = cell->GetId(0);
    vtkIdType b = cell->GetId(1);

    double ax[3], bx[3];
    armature->GetPoints()->GetPoint(a, ax);
    armature->GetPoints()->GetPoint(b, bx);

    RigidTransform transform;
    GetArmatureTransform(armature, edgeId, "Transforms", ax, transform,true);
    transforms.push_back(transform);
    edgeId++;
    }

  numSites = transforms.size();

  if(1)
    {
    vtkSmartPointer<vtkPolyData> testArmature;
    testArmature.TakeReference(ReadPolyData(ArmaturePoly.c_str(),true));
    WritePolyData(TransformArmature(testArmature,transforms),"/home/leo/temp/test.vtk");
    }

  cout<<"Read "<<numSites<<" transforms"<<endl;
  if(TestOne)
    {
    cout<<"Testing just one weight map. Transform is made up."<<endl;
    numSites=1;
    transforms.resize(1);
    double rCenter[3] = {-82.1714, 42.9494, -865.9};
    transforms[0].SetRotation(1,0,0,3.14/10);
    transforms[0].SetRotationCenter(rCenter);
    cout<<transforms[0].R<<endl;
    }

  //----------------------------
  // Read Weights
  //----------------------------
  WeightMap weightMap;
  ReadWeights(WeightDirectory,numSites,bodyMap,weightMap,TestOne);

  //----------------------------
  // Read in the stl file
  //----------------------------
  vtkSmartPointer<vtkPolyData> inSurface;
  inSurface.TakeReference(ReadPolyData(SurfaceInput.c_str(),false));

  vtkPoints* inputPoints = inSurface->GetPoints();
  int numPoints = inputPoints->GetNumberOfPoints();
  cout<<numPoints<<" surface points\n";

  // Check surface points.
  int numBad(0);
  int numInterior(0);
  CubeNeighborhood cubeNeighborhood;
  VoxelOffset* offsets = cubeNeighborhood.Offsets ;
  for(int pi=0; pi<numPoints;pi++)
    {
    double xraw[3];
    inputPoints->GetPoint(pi,xraw);

    itk::Point<double,3> x(xraw);

    itk::ContinuousIndex<double,3> coord;
    bodyMap->TransformPhysicalPointToContinuousIndex(x, coord);

    Voxel p;
    p.CopyWithCast(coord);

    bool hasInside(false);
    bool hasOutside(false);
    for(int iOff=0; iOff<8; iOff++)
      {
      Voxel q = p + offsets[iOff];
      if(bodyMap->GetPixel(q)>0)
        {
        hasInside=true;
        }
      else
        {
        hasOutside=true;
        }
      }
    numBad+= hasInside? 0 : 1;
    numInterior+= hasOutside? 0: 1;
    }
  if(numBad>0)
    {
    cout<<numBad<<" bad surface vertices."<<endl;
    cout<<numInterior<<" interior vertices."<<endl;
    cout<<"Bad input. Quitting"<<endl;
    return EXIT_FAILURE;
    }


  //Perform interpolation
  vtkSmartPointer<vtkPolyData> outSurface = vtkSmartPointer<vtkPolyData>::New();
  outSurface->DeepCopy(inSurface);
  vtkPoints* outPoints = outSurface->GetPoints();
  vtkPointData* outData = outSurface->GetPointData();
  outData->Initialize();
  std::vector<vtkFloatArray*> surfaceVertexWeights;
  for(int i=0; i<numSites; i++)
    {
    vtkFloatArray* arr = vtkFloatArray::New();
    arr->SetNumberOfTuples(numPoints);
    arr->SetNumberOfComponents(1);
    for(vtkIdType j=0; j<static_cast<vtkIdType>(numPoints); j++)
      {
      arr->SetValue(j,0.0);
      }
    std::stringstream name;
    name<<"weight"<<i;
    arr->SetName(name.str().c_str());
    outData->AddArray(arr);
    surfaceVertexWeights.push_back(arr);
    arr->Delete();
    assert(outData->GetArray(i)->GetNumberOfTuples()==numPoints);
    }

  int stat_num_support(0);
  WeightVector w_pi(numSites);
  WeightVector w_corner(numSites);
  for(int pi=0; pi<inputPoints->GetNumberOfPoints();pi++)
    {
    double xraw[3];
    inputPoints->GetPoint(pi,xraw);

    itk::Point<double,3> x(xraw);

    itk::ContinuousIndex<double,3> coord;
    bodyMap->TransformPhysicalPointToContinuousIndex(x, coord);

    Voxel m; //min index of the cell containing the point
    m.CopyWithCast(coord);

    w_pi.Fill(0);
    assert(w_pi.GetNorm()==0);
    //interpoalte the weight for vertex over the cube corner
    if(1)
      {
      double cornerWSum(0);
      for(unsigned int corner=0; corner<8; corner++)
        {
        //for each bit of index
        unsigned int bit = corner;
        double cornerW=1.0;
        Voxel q;
        for(int dim=0; dim<3; dim++)
          {
          bool upper = bit & 1;
          bit>>=1;
          float t = coord[dim] - static_cast<float>(m[dim]);
          cornerW*= upper? t : 1-t;
          q[dim] = m[dim]+ static_cast<int>(upper);
          }
        assert(cornerW>=0);
        w_corner.Fill(0);
        if(bodyMap->GetPixel(q)>0)
          {
          cornerWSum+=cornerW;
          weightMap.Get(q, w_corner);
          w_pi += cornerW*w_corner;
          stat_num_support++;
          }
        else
          {
          }
        }
      assert(cornerWSum!=0.0);
      w_pi*= 1.0/cornerWSum;
      }
//    NormalizeWeight(w_pi);
    for(int i=0; i<numSites;i++)
      {
      surfaceVertexWeights[i]->SetValue(pi, w_pi[i]);
      }

    double wSum(0.0);
    for(int i=0; i<numSites;i++)
      {
      wSum+=w_pi[i];
      }

    Vec3 y(0.0);
    //linearly blending transforms
    if(1)
      {
      assert(wSum>=0);
      for(int i=0; i<numSites;i++)
        {
        double w = w_pi[i]/wSum;
        const RigidTransform& Fi(transforms[i]);
        double yi[3];
        Fi.Apply(xraw,yi);
        y+= w*Vec3(yi);
        }

      // if(!normalize)
      //   {
      //   y+= (1.0-wSum)*Vec3(xraw);
      //   }
      }
    else
      {
      Vec4 R(0.0);
      Vec3 T(0.0);
      for(int i=0; i<numSites;i++)
        {
        double w = w_pi[i];
        const RigidTransform& Fi(transforms[i]);
        R+=w*Fi.R;
        Mat33 Ri = ToRotationMatrix(Fi.R);
        Vec3 Ti = -1.0*(Ri*Fi.O)+Fi.O;
        T+= w*Ti;
        }
      R = (1-wSum)*Q0+R;
      y = ToRotationMatrix(R)*Vec3(xraw)+T;
      }


    outPoints->SetPoint(pi,y[0],y[1],y[2]);

    }
  cout<< (double)stat_num_support/inputPoints->GetNumberOfPoints()<<" average support"<<endl;

  WritePolyData(outSurface,"/home/leo/temp/posedSurface.vtk");

  return EXIT_SUCCESS;
}
