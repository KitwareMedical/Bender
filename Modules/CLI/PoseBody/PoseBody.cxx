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

#include "PoseBodyCLP.h"

#include "dqconv.h"
#include "benderWeightMap.h"
#include "benderWeightMapIO.h"
#include "benderWeightMapMath.h"

#include <itkImageFileWriter.h>
#include <itkImage.h>
#include <itkStatisticsImageFilter.h>
#include <itkPluginUtilities.h>
#include <itkImageRegionIteratorWithIndex.h>
#include <vtkPolyDataReader.h>
#include <vtkPolyDataWriter.h>
#include <itkContinuousIndex.h>
#include <itkMath.h>
#include <itkIndex.h>
#include <itkLinearInterpolateImageFunction.h>
#include <itkMatrix.h>

#include <vtkTimerLog.h>
#include <vtkSTLReader.h>
#include <vtkPolyData.h>
#include <vtkCellArray.h>
#include <vtkNew.h>
#include <vtkSmartPointer.h>
#include <vtkPointData.h>
#include <vtkCellData.h>
#include <vtkFloatArray.h>
#include <vtkMath.h>
#include <vtkCubeSource.h>
#include <itkVersor.h>

#include <sstream>
#include <iostream>
#include <vector>
#include <limits>

typedef itk::Matrix<double,2,4> Mat24;

using namespace std;

typedef unsigned char CharType;
typedef unsigned short LabelType;

typedef itk::Image<unsigned short, 3>  LabelImage;
typedef itk::Image<float, 3>  WeightImage;
typedef itk::Image<bool, 3>  BoolImage;

typedef itk::Index<3> Voxel;
typedef itk::Offset<3> VoxelOffset;
typedef itk::ImageRegion<3> Region;

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
  for(int i=0; i<n; ++i)
    {
    cout<<a[i]<<(i==n-1?"": ", ");
    }
  cout<<"]"<<endl;
}

//-------------------------------------------------------------------------------
void PrintVTKQuarternion(double* a)
{
  cout<<"[ ";
  cout<<a[1]<<", "<<a[2]<<", "<<a[3]<<", "<<a[0];
  cout<<" ]"<<endl;
}

//-------------------------------------------------------------------------------
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

//-------------------------------------------------------------------------------
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


//-------------------------------------------------------------------------------
Mat33 ToItkMatrix(double M[3][3])
{
  Mat33 itkM;
  for(int i=0; i<3; ++i)
    {
    for(int j=0; j<3; ++j)
      {
      itkM(i,j)  = M[i][j];
      }
    }

  return itkM;
}

//-------------------------------------------------------------------------------
inline Mat33 ToRotationMatrix(const Vec4& R)
{
  Versor v;
  v.Set(R[1],R[2], R[3],R[0]);
  return v.GetMatrix();
}

//-------------------------------------------------------------------------------
void ApplyQT(Vec4& q, Vec3& t, double* x)
{
  double R[3][3];
  vtkMath::QuaternionToMatrix3x3(&q[0], R);

  double Rx[3];
  vtkMath::Multiply3x3(R, x, Rx);

  for(unsigned int i=0; i<3; ++i)
    {
    x[i] = Rx[i]+t[i];
    }
}

//-------------------------------------------------------------------------------
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
  Vec3 GetTranslationComponent()
  {
    return ToRotationMatrix(this->R)*(-this->O) + this->O +T;
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

//-------------------------------------------------------------------------------
void GetArmatureTransform(vtkPolyData* polyData, vtkIdType cellId, const char* arrayName, const double* rcenter, RigidTransform& F,bool invertY =true)
{
  double A[12];
  polyData->GetCellData()->GetArray(arrayName)->GetTuple(cellId, A);

  double R[3][3];
  double T[3];
  double RCenter[3];
  int iA(0);
  for(int i=0; i<3; ++i)
    {
    for(int j=0; j<3; ++j,++iA)
      {
      R[j][i] = A[iA];
      }
    }

  for(int i=0; i<3; ++i)
    {
    T[i] = A[i+9];
    RCenter[i] = rcenter[i];
    }

  if(invertY)
    {
    for(int i=0; i<3; ++i)
      {
      for(int j=0; j<3; ++j)
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

//-------------------------------------------------------------------------------
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
    for(int i=0; i<3; ++i)
      for(int j=0; j<3; ++j,++iA)
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
      for(int i=0; i<3; ++i)
        for(int j=0; j<3; ++j)
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

    ++edgeId;
    }
  return output;
}


//-------------------------------------------------------------------------------
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
    ++edgeId;
    }
  return output;
}

//-------------------------------------------------------------------------------
class CubeNeighborhood
{
public:
  CubeNeighborhood()
  {
    int index=0;
    for(unsigned int i=0; i<=1; ++i)
      {
      for(unsigned int j=0; j<=1; ++j)
        {
        for(unsigned int k=0; k<=1; ++k,++index)
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


//-------------------------------------------------------------------------------
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
    for(int i=0; i<points->GetNumberOfPoints();++i)
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
  cout<<"Write polydata to "<<fileName<<endl;
  vtkNew<vtkPolyDataWriter> pdWriter;
  pdWriter->SetInput(polyData);
  pdWriter->SetFileName(fileName.c_str() );
  pdWriter->SetFileTypeToBinary();
  pdWriter->Update();
}

//-------------------------------------------------------------------------------
void TestQuarternion()
{

  double A[3][3] = {{1., 0., 0.},{0., 1., 0.},{0., 0., 1.}};
  double AQuat[4];
  double A1[3][3];
  vtkMath::Matrix3x3ToQuaternion(A, AQuat);
  vtkMath::QuaternionToMatrix3x3(AQuat, A1);

  for(int i=0; i<3; ++i)
    {
    for(int j=0; j<3; ++j)
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


//-------------------------------------------------------------------------------
void TestDualQuarternion()
{
  Vec4 q = ComputeQuarternion(0,0,1,3.14/4);
  Vec3 t;
  t[0] = 0.0;
  t[1] = 1.0;
  t[2] = 0.0;

  double dq[2][4];
  QuatTrans2UDQ(&q[0],&t[0],dq);

  Vec4 q1;
  Vec3 t1;
  DQ2QuatTrans(dq,&q1[0],&t1[0]);
}

//-------------------------------------------------------------------------------
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

//-------------------------------------------------------------------------------
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
    for(int i=0; i<3; ++i)
      {
      assert(x[i]==y[i]);
      }
    }
}

//-------------------------------------------------------------------------------
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
  for(int dim = 0; dim < 2; ++dim )
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
  for(unsigned int index=0; index<4; ++index)
    {
    //for each bit of index
    unsigned int bit = index;
    double w=1.0;
    ImageType::IndexType ij;
    for(int dim=0; dim<2; ++dim)
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

//-------------------------------------------------------------------------------
void ComputeDomainVoxels(WeightImage::Pointer image //input
                         ,vtkPoints* points //input
                         ,std::vector<Voxel>& domainVoxels //output
                         )
{
  CubeNeighborhood cubeNeighborhood;
  VoxelOffset* offsets = cubeNeighborhood.Offsets;

  BoolImage::Pointer domain = BoolImage::New();
  domain->SetRegions(image->GetLargestPossibleRegion());
  domain->Allocate();
  domain->FillBuffer(false);

  for(int pi=0; pi<points->GetNumberOfPoints();++pi)
    {
    double xraw[3];
    points->GetPoint(pi,xraw);

    itk::Point<double,3> x(xraw);
    itk::ContinuousIndex<double,3> coord;
    image->TransformPhysicalPointToContinuousIndex(x, coord);

    Voxel p;
    p.CopyWithCast(coord);

    for(int iOff=0; iOff<8; ++iOff)
      {
      Voxel q = p + offsets[iOff];
      if(!domain->GetPixel(q))
        {
        domain->SetPixel(q,true);
        domainVoxels.push_back(q);
        }
      }
    }
}

//-------------------------------------------------------------------------------
int main( int argc, char * argv[] )
{
  typedef bender::WeightMap WeightMap;
  //run some tests
  TestTransformBlending();
  TestVersor();
  TestInterpolation();

  PARSE_ARGS;


  cout<<"Armature Y coordinate will be inverted\n";

  if(LinearBlend)
    {
    cout<<"Use Linear Blend"<<endl;
    }
  else
    {
    cout<<"Use Dual Quaternion blend"<<endl;
    }

  //----------------------------
  // Read the first weight image
  // and all file names
  //----------------------------
  vector<string> fnames;
  bender::GetWeightFileNames(WeightDirectory, fnames);
  int numSites = fnames.size();
  if(numSites<1)
    {
    cerr<<"No weight file is found."<<endl;
    return 1;
    }

  typedef itk::ImageFileReader<WeightImage>  ReaderType;
  ReaderType::Pointer reader = ReaderType::New();
  reader->SetFileName(fnames[0].c_str());
  reader->Update();

  WeightImage::Pointer weight0 =  reader->GetOutput();
  Region weightRegion = weight0->GetLargestPossibleRegion();
  cout<<"Weight volume description: "<<endl;
  cout<<weightRegion<<endl;

  int numForeGround(0);
  for(itk::ImageRegionIterator<WeightImage> it(weight0,weightRegion);!it.IsAtEnd(); ++it)
    {
    numForeGround+= it.Get()>=0;
    }
  cout<<numForeGround<<" foreground voxels"<<endl;

  //----------------------------
  // Read in the surface file
  //----------------------------
  vtkSmartPointer<vtkPolyData> inSurface;
  inSurface.TakeReference(ReadPolyData(SurfaceInput.c_str(),false));

  vtkPoints* inputPoints = inSurface->GetPoints();
  int numPoints = inputPoints->GetNumberOfPoints();
  std::vector<Voxel> domainVoxels;
  ComputeDomainVoxels(weight0,inputPoints,domainVoxels);
  cout<<numPoints<<" vertices, "<<domainVoxels.size()<<" voxels"<<endl;


  //----------------------------
  // Read Weights
  //----------------------------
  WeightMap weightMap;
  bender::ReadWeights(fnames,domainVoxels,weightMap);

  //----------------------------
  // Read armature
  //----------------------------
  std::vector<RigidTransform> transforms;
  vtkSmartPointer<vtkPolyData> armature;
  armature.TakeReference(ReadPolyData(ArmaturePoly.c_str(),false));

  if(0) //test whether the transform makes senses.
    {
    WritePolyData(TransformArmature(armature,"Transforms",true),"./test.vtk");
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
    ++edgeId;
    }

  numSites = transforms.size();
  std::vector<Mat24> dqs;
  for(size_t i=0; i<transforms.size(); ++i)
    {
    Mat24 dq;
    RigidTransform& trans = transforms[i];
    Vec3 T = trans.GetTranslationComponent();
    QuatTrans2UDQ(&trans.R[0], &T[0], (double (*)[4]) &dq(0,0));
    dqs.push_back(dq);
    }

  cout<<"Read "<<numSites<<" transforms"<<endl;

  //----------------------------
  // Check surface points
  //----------------------------
  int numBad(0);
  int numInterior(0);
  CubeNeighborhood cubeNeighborhood;
  VoxelOffset* offsets = cubeNeighborhood.Offsets ;
  for(int pi=0; pi<numPoints;++pi)
    {
    double xraw[3];
    inputPoints->GetPoint(pi,xraw);

    itk::Point<double,3> x(xraw);

    itk::ContinuousIndex<double,3> coord;
    weight0->TransformPhysicalPointToContinuousIndex(x, coord);

    Voxel p;
    p.CopyWithCast(coord);

    bool hasInside(false);
    bool hasOutside(false);
    for(int iOff=0; iOff<8; ++iOff)
      {
      Voxel q = p + offsets[iOff];
      if(weight0->GetPixel(q)>=0)
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
    cout<<"WARNING: "<<numBad<<" bad surface vertices."<<endl;
    }


  //----------------------------
  // Perform interpolation
  //----------------------------
  vtkSmartPointer<vtkPolyData> outSurface = vtkSmartPointer<vtkPolyData>::New();
  outSurface->DeepCopy(inSurface);
  vtkPoints* outPoints = outSurface->GetPoints();
  vtkPointData* outData = outSurface->GetPointData();
  outData->Initialize();
  std::vector<vtkFloatArray*> surfaceVertexWeights;
  for(int i=0; i<numSites; ++i)
    {
    vtkFloatArray* arr = vtkFloatArray::New();
    arr->SetNumberOfTuples(numPoints);
    arr->SetNumberOfComponents(1);
    for(vtkIdType j=0; j<static_cast<vtkIdType>(numPoints); ++j)
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

  WeightMap::WeightVector w_pi(numSites);
  WeightMap::WeightVector w_corner(numSites);
  for(int pi=0; pi<inputPoints->GetNumberOfPoints();++pi)
    {
    double xraw[3];
    inputPoints->GetPoint(pi,xraw);

    itk::Point<double,3> x(xraw);

    itk::ContinuousIndex<double,3> coord;
    weight0->TransformPhysicalPointToContinuousIndex(x, coord);

    bool res = bender::Lerp<WeightImage>(weightMap,coord,weight0, 0, w_pi);
    if(!res)
      {
      cerr<<"Lerp failed for "<<coord<<endl;
      }
    else
      {
//    NormalizeWeight(w_pi);
      for(int i=0; i<numSites;++i)
        {
        surfaceVertexWeights[i]->SetValue(pi, w_pi[i]);
        }
      }

    double wSum(0.0);
    for(int i=0; i<numSites;++i)
      {
      wSum+=w_pi[i];
      }

    Vec3 y(0.0);
    if(LinearBlend)
      {
      assert(wSum>=0);
      for(int i=0; i<numSites;++i)
        {
        double w = w_pi[i]/wSum;
        const RigidTransform& Fi(transforms[i]);
        double yi[3];
        Fi.Apply(xraw,yi);
        y+= w*Vec3(yi);
        }
      }
    else
      {
      Mat24 dq;
      dq.Fill(0.0);
      for(int i=0; i<numSites;++i)
        {
        double w = w_pi[i]/wSum;
        Mat24& dq_i(dqs[i]);
        dq+= dq_i*w;
        }
      Vec4 q;
      Vec3 t;
      DQ2QuatTrans((const double (*)[4])&dq(0,0), &q[0], &t[0]);
      y = Vec3(xraw);
      ApplyQT(q,t,&y[0]);
      }

    outPoints->SetPoint(pi,y[0],y[1],y[2]);

    }

  //----------------------------
  // Write output
  //----------------------------
  WritePolyData(outSurface,OutputSurface);

  return EXIT_SUCCESS;
}
