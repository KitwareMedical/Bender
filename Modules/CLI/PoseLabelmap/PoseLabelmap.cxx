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
#include "PoseLabelmapCLP.h"
#include "benderIOUtils.h"
#include "benderWeightMap.h"
#include "benderWeightMapIO.h"
#include "benderWeightMapMath.h"
#include "vtkDualQuaternion.h"

// ITK includes
#include <itkContinuousIndex.h>
#include <itkDanielssonDistanceMapImageFilter.h>
#include <itkImage.h>
#include <itkImageFileWriter.h>
#include <itkImageRegionIteratorWithIndex.h>
#include <itkIndex.h>
#include <itkLinearInterpolateImageFunction.h>
#include <itkMath.h>
#include <itkMatrix.h>
#include <itkPluginUtilities.h>
#include <itkStatisticsImageFilter.h>
#include <itkVersor.h>
#include <itksys/SystemTools.hxx>

// VTK includes
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkCubeSource.h>
#include <vtkDoubleArray.h>
#include <vtkFloatArray.h>
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkPolyDataReader.h>
#include <vtkPolyDataWriter.h>
#include <vtkSTLReader.h>
#include <vtkSmartPointer.h>
#include <vtkTimerLog.h>

// STD includes
#include <cmath>
#include <sstream>
#include <iostream>
#include <vector>
#include <limits>

typedef itk::Matrix<double,2,4> Mat24;

typedef unsigned char CharType;
typedef unsigned short LabelType;

#define OutsideLabel 0

typedef itk::Image<unsigned short, 3>  LabelImage;
typedef itk::Image<bool, 3>  BoolImage;
typedef itk::Image<float, 3>  WeightImage;

typedef itk::Index<3> Voxel;
typedef itk::Offset<3> VoxelOffset;
typedef itk::ImageRegion<3> Region;

typedef itk::Versor<double> Versor;
typedef itk::Matrix<double,3,3> Mat33;
typedef itk::Matrix<double,4,4> Mat44;

typedef itk::Vector<double,3> Vec3;
typedef itk::Vector<double,4> Vec4;

template<class T> int DoIt(int argc, char* argv[]);

//-------------------------------------------------------------------------------
int main( int argc, char * argv[] )
{
  PARSE_ARGS;

  try
    {
    itk::ImageIOBase::IOPixelType     pixelType;
    itk::ImageIOBase::IOComponentType componentType;

    itk::GetImageType(RestLabelmap, pixelType, componentType);

    // This filter handles all types on input, but only produces
    // signed types
    switch (componentType)
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

//-------------------------------------------------------------------------------
template<class T>
inline void InvertXY(T& x)
{
  x[0] *= -1;
  x[1] *= -1;
}

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

  for (unsigned int i=0; i<3; ++i)
    {
    x[i] = Rx[i]+t[i];
    }
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
  F.SetTranslation(T);
}

//-------------------------------------------------------------------------------
vtkSmartPointer<vtkPolyData> TransformArmature(vtkPolyData* armature,
                                               const char* arrayName, bool invertXY)
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
    for (int i=0; i<3; ++i)
      for (int j=0; j<3; ++j,++iA)
        {
        R(i,j) = A[iA];
        }

    R = R.GetTranspose();
    Vec3 T;
    T[0] = A[9];
    T[1] = A[10];
    T[2] = A[11];

    if(invertXY)
      {
      //    Mat33 flipY;
      for (int i=0; i<3; ++i)
        {
        for (int j=0; j<3; ++j)
          {
          if( (i>1 || j>1) && i!=j)
            {
            R(i,j)*=-1;
            }
          }
        }
      InvertXY(T);
      }


    Vec3 ax(inPoints->GetPoint(a));
    Vec3 bx(inPoints->GetPoint(b));
    Vec3 ax1 = R*(ax-ax)+ax+T;
    Vec3 bx1 = R*(bx-ax)+ax+T;

    if(invertXY)
      {
      InvertXY(ax1);
      InvertXY(bx1);
      }
    //std::cout <<"Set point "<<a<<" to "<<Vec3(ax1)<<std::endl;
    outPoints->SetPoint(a,&ax1[0]);

    //std::cout <<"Set point "<<b<<" to "<<Vec3(bx1)<<std::endl;
    outPoints->SetPoint(b,&bx1[0]);

    ++edgeId;
    }
  return output;
}


//-------------------------------------------------------------------------------
vtkSmartPointer<vtkPolyData>
TransformArmature(vtkPolyData* armature,
                  const std::vector<RigidTransform>& transforms)
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
    for (unsigned int i=0; i<=1; ++i)
      {
      for (unsigned int j=0; j<=1; ++j)
        {
        for (unsigned int k=0; k<=1; ++k,++index)
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
bool WIComp(const std::pair<double, int>& left, const std::pair<double, int>& right)
{
  return left.first > right.first;
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
  domain->CopyInformation(image);

  WeightImage::RegionType region = image->GetLargestPossibleRegion();
  domain->SetRegions(region);
  domain->Allocate();
  domain->FillBuffer(false);

  for (int pi=0; pi<points->GetNumberOfPoints();++pi)
    {
    double xraw[3];
    points->GetPoint(pi,xraw);

    itk::Point<double,3> x(xraw);
    itk::ContinuousIndex<double,3> coord;
    image->TransformPhysicalPointToContinuousIndex(x, coord);

    Voxel p;
    p.CopyWithCast(coord);

    for (int iOff=0; iOff<8; ++iOff)
      {
      Voxel q = p + offsets[iOff];

      if(region.IsInside(q) && !domain->GetPixel(q))
        {
        domain->SetPixel(q,true);
        domainVoxels.push_back(q);
        }
      }
    }
}

//-------------------------------------------------------------------------------
template<class InImage, class OutImage>
void Allocate(typename InImage::Pointer in, typename OutImage::Pointer out)
{
  out->CopyInformation(in);
  out->SetRegions(in->GetLargestPossibleRegion());
  out->Allocate();
}

/*
//-------------------------------------------------------------------------------
itk::Vector<double,3> Transform(const itk::Vector<double,3>& restCoord,
                                size_t numSites,
                                const bender::WeightMap& weightMap,
                                bool linearBlend,
                                const std::vector<RigidTransform>& transforms,
                                const std::vector<Mat24>& dqs)
{
  itk::Vector<double,3> posedCoord(0.0);
  bender::WeightMap::WeightVector w_pi(numSites);
  weightMap.Get(imageIt.GetIndex(), w_pi);
  double wSum(0.0);
  for (size_t i = 0; i < numSites; ++i)
    {
    wSum+=w_pi[i];
    }
  if (wSum <= 0.0)
    {
    posedCoord = restCoord;
    }
  else if(linearBlend)
    {
    for (size_t i=0; i<numSites;++i)
      {
      double w = w_pi[i]/wSum;
      const RigidTransform& Fi(transforms[i]);
      Vec3 yi;
      Fi.Apply(restCoord, yi);
      posedCoord += w*yi;
      }
    }
  else
    {
    Mat24 dq;
    dq.Fill(0.0);
    for (size_t i=0; i<numSites;++i)
      {
      double w = w_pi[i]/wSum;
      Mat24& dq_i(dqs[i]);
      dq+= dq_i*w;
      }
    Vec4 q;
    Vec3 t;
    DQ2QuatTrans((const double (*)[4])&dq(0,0), &q[0], &t[0]);
    posedCoord = restCoord;
    ApplyQT(q,t,&posedCoord[0]);
    }
  return posedCoord;
  }
*/
// Initialized in DoIt;
itk::Vector<double,3> InvalidCoord;

//-------------------------------------------------------------------------------
itk::Vector<double,3> Transform(const itk::Vector<double,3>& restCoord,
                                bender::WeightMap::WeightVector w_pi,
                                bool linearBlend,
                                const int maximumNumberOfInterpolatedBones,
                                const std::vector<vtkDualQuaternion<double> >& dqs)
{
  double restPos[3];
  restPos[0] = restCoord[0];
  restPos[1] = restCoord[1];
  restPos[2] = restCoord[2];
  itk::Vector<double,3> posedCoord(0.0);

  const size_t numSites = w_pi.GetSize();
  double wSum(0.0);
  for (size_t i = 0; i < numSites; ++i)
    {
    wSum+=w_pi[i];
    }
  if (wSum <= 0.0)
    {
    posedCoord = InvalidCoord;//restCoord;
    }
  else if (linearBlend)
    {
    for (size_t i = 0; i < numSites; ++i)
      {
      double w = w_pi[i] / wSum;
      double yi[3];
      const vtkDualQuaternion<double>& transform(dqs[i]);
      transform.TransformPoint(restPos, yi);
      posedCoord += w*Vec3(yi);
      }
    }
  else
    {
    std::vector<std::pair<double, int> > ws;
    ws.reserve(numSites);
    for (size_t i=0; i < numSites; ++i)
      {
      double w = w_pi[i] / wSum;
      ws.push_back(std::make_pair(w, static_cast<int>(i)));
      }
    // To limit computation errors, it is important to start interpolating with the
    // highest w first.
    std::partial_sort(ws.begin(),
                      ws.begin() + maximumNumberOfInterpolatedBones,
                      ws.end(),
                      WIComp);
    vtkDualQuaternion<double> transform = dqs[ws[0].second];
    double w = ws[0].first;
    // Warning, Sclerp is only meant to blend 2 DualQuaternions, I'm not
    // sure it works with more than 2.
    for (int i = 1; i < maximumNumberOfInterpolatedBones; ++i)
      {
      double w2 = ws[i].first;
      int i2 = ws[i].second;
      transform = transform.ScLerp2(w2 / (w + w2), dqs[i2]);
      w += w2;
      }
    transform.TransformPoint(restPos, &posedCoord[0]);
    }
  return posedCoord;
}

//-------------------------------------------------------------------------------
template <class T>
itk::Vector<double,3> Transform(typename itk::Image<T,3>::Pointer image,
                                const itk::ContinuousIndex<double,3>& index,
                                size_t numSites,
                                const bender::WeightMap& weightMap,
                                bool linearBlend,
                                int maximumNumberOfInterpolatedBones,
                                const std::vector<vtkDualQuaternion<double> >& dqs)
{
  typename itk::Image<T,3>::PointType p;
  image->TransformContinuousIndexToPhysicalPoint(index, p);
  itk::Vector<double,3> restCoord(0.0);
  restCoord[0] = p[0];
  restCoord[1] = p[1];
  restCoord[2] = p[2];
  bender::WeightMap::WeightVector w_pi(numSites);
  //typename itk::Image<T,3>::IndexType weightIndex;
  //weightIndex[0] = round(index[0]);
  //weightIndex[1] = round(index[1]);
  //weightIndex[2] = round(index[2]);
  //weightMap.Get(weightIndex, w_pi);
  bool res = weightMap.Lerp(index, w_pi);
  if (!res)
    {
    return InvalidCoord;
    }
  return Transform(restCoord, w_pi, linearBlend, maximumNumberOfInterpolatedBones, dqs);
}


//-------------------------------------------------------------------------------
// If includeSelf is 1, then Offsets will contain the offset (0,0,0) and all its
// neighbors in that order.
template<unsigned int dimension, unsigned int includeSelf = 0>
class Neighborhood
{
public:
  Neighborhood()
  {
    for (unsigned int i=0; i<dimension; ++i)
      {
      int lo = includeSelf + 2*i;
      int hi = includeSelf + 2*i+1;
      for (unsigned int j=0; j<dimension; ++j)
        {
        if (includeSelf)
          {
          this->Offsets[0][j] = 0;
          }
        this->Offsets[lo][j] = j==i? -1 : 0;
        this->Offsets[hi][j] = j==i?  1 : 0;
        }
      }
  }
  size_t GetSize()const{return 2*dimension + includeSelf;}
  itk::Offset<dimension> Offsets[2*dimension + includeSelf];
};




//-------------------------------------------------------------------------------
// size = 0:             size = 1:             size = 2:
// /-----------\         *-----*-----*         /--*-----*--\
// |           |         |           |         |           |
// |           |         |           |         |           |
// |           |         |           |         *  *  *  *  *
// |           |         |           |         |           |
// |           |         |           |         |           |
// |     *     |         *           *         |  *     *  |
// |           |         |           |         |           |
// |           |         |           |         |           |
// |           |         |           |         *  *  *  *  *
// |           |         |           |         |           |
// |           |         |           |         |           |
// \-----------/         *-----*-----*         \--*-----*--/
template<unsigned int dimension>
class SubNeighborhood
{
public:
  SubNeighborhood(int size)
  {
    this->Size = pow(static_cast<double>(size*2+1), static_cast<int>(dimension))
                 - pow(static_cast<double>((size / 2)*2 + 1), static_cast<int>(dimension));
    this->Offsets = new itk::Offset<dimension>[this->Size];
    size_t count = 0;
    for (int z = -size; z <= size; ++z)
      {
      for (int y = -size; y <= size; ++y)
        {
        for (int x = -size; x <= size; ++x)
          {
          if ( z % 2 == 0 && y % 2 == 0 && x % 2 == 0)
            {
            continue;
            }
          this->Offsets[count][0] = x;
          this->Offsets[count][1] = y;
          this->Offsets[count][2] = z;
          ++count;
          }
        }
      }
    assert(count == this->Size);
  }
  ~SubNeighborhood()
  {
    delete [] this->Offsets;
  }
  size_t Size;
  itk::Offset<dimension>* Offsets;
};


























//-------------------------------------------------------------------------------
template<class T>
int DoIt(int argc, char* argv[])
{
 // This property controls how many bone transforms are blended together
  // when interpolating. Usually 2 but can go up to 4 sometimes.
  // 1 for no interpolation (use the closest bone transform).
  const int MaximumNumberOfInterpolatedBones = 4;
  // This property controls whether to interpolate with ScLerp
  // (Screw Linear interpolation) or DLB (Dual Quaternion Linear
  // Blending).
  // Note that DLB (faster) is not tweaked to give proper results.
  // const bool UseScLerp = true;

  InvalidCoord[0] = std::numeric_limits<double>::max();
  InvalidCoord[1] = std::numeric_limits<double>::max();
  InvalidCoord[2] = std::numeric_limits<double>::max();

  PARSE_ARGS;

  if (!IsArmatureInRAS)
    {
    std::cout <<"Armature x,y coordinates will be inverted" << std::endl;
    }

  if(LinearBlend)
    {
    std::cout <<"Use Linear Blend\n" << std::endl;
    }
  else
    {
    std::cout <<"Use Dual Quaternion blend" << std::endl;
    }

  //----------------------------
  // Read the first weight image
  // and all file names
  //----------------------------
  std::vector<std::string> fnames;
  bender::GetWeightFileNames(WeightDirectory, fnames);
  size_t numSites = fnames.size();
  if (numSites == 0)
    {
    std::cerr << "No weight file found in directory: " << WeightDirectory
              << std::endl;
    return 1;
    }

  typedef itk::ImageFileReader<WeightImage>  ReaderType;
  ReaderType::Pointer reader = ReaderType::New();
  reader->SetFileName(fnames[0].c_str());
  reader->Update();

  WeightImage::Pointer weight0 = reader->GetOutput();
  Region weightRegion = weight0->GetLargestPossibleRegion();
  std::cout << "Weight volume description: " << std::endl;
  std::cout << weightRegion << std::endl;

  if (Debug)
    {
    std::cout << "############# Compute foreground voxels...";
    size_t numVoxels(0);
    size_t numForeGround(0);
    for (itk::ImageRegionIterator<WeightImage> it(weight0, weightRegion);
         !it.IsAtEnd(); ++it)
      {
      numForeGround += (it.Get() != OutsideLabel ? 1 : 0);
      ++numVoxels;
      }
    std::cout << numForeGround << " foreground voxels for "
              << numVoxels << " voxels." << std::endl;
    }

  //----------------------------
  // Read in the labelmap
  //----------------------------
  std::cout << "############# Read input rest labelmap...";
  typedef itk::Image<T, 3> LabelImageType;
  typedef itk::ImageFileReader<LabelImageType> LabelMapReaderType;
  typename LabelMapReaderType::Pointer labelMapReader = LabelMapReaderType::New();
  labelMapReader->SetFileName(RestLabelmap.c_str() );
  labelMapReader->Update();
  typename LabelImageType::Pointer labelMap = labelMapReader->GetOutput();
  if (!labelMap)
    {
    std::cerr << "Can't read labelmap " << RestLabelmap << std::endl;
    return EXIT_FAILURE;
    }
  std::cout << "############# done." << std::endl;

  if (Debug)
    {
    std::cout << "Input Labelmap: \n"
            << " Origin: " << labelMap->GetOrigin() << "\n"
            << " Spacing: " << labelMap->GetSpacing() << "\n"
            << " Direction: " << labelMap->GetDirection() << "\n"
            << " " << labelMap->GetLargestPossibleRegion()
            << std::endl;
    }
  //vtkPoints* inputPoints = inSurface->GetPoints();
  //int numPoints = inputPoints->GetNumberOfPoints();
  //std::vector<Voxel> domainVoxels;
  //ComputeDomainVoxels(weight0,inputPoints,domainVoxels);
  //std::cout <<numPoints<<" vertices, "<<domainVoxels.size()<<" voxels"<< std::endl;


  //----------------------------
  // Read Weights
  //----------------------------
  std::cout << "############# Read weights...";
  typedef bender::WeightMap WeightMap;
  WeightMap weightMap;
  //bender::ReadWeights(fnames,domainVoxels,weightMap);
  bender::ReadWeightsFromImage<T>(fnames, labelMap, weightMap);
  // Don't interpolate weights outside of the domain (i.e. outside the body).
  // -1. is outside of domain
  // 0. is no weight for bone 0
  // 1. is full weight for bone 0
  weightMap.SetMaskImage(weight0, 0.f);
  std::cout << "############# done." << std::endl;

  //----------------------------
  // Read armature
  //----------------------------
  std::vector<RigidTransform> transforms;
  vtkSmartPointer<vtkPolyData> armature;
  armature.TakeReference(bender::IOUtils::ReadPolyData(ArmaturePoly.c_str(),!IsArmatureInRAS));
  double restArmatureBounds[6] = {0., -1., 0., -1., 0., -1.};
  armature->GetBounds(restArmatureBounds);
  std::cout << "Rest armature bounds: "
            << restArmatureBounds[0] << ", " << restArmatureBounds[1] << ", "
            << restArmatureBounds[2] << ", " << restArmatureBounds[3] << ", "
            << restArmatureBounds[4] << ", " << restArmatureBounds[5] << std::endl;

  //if (Debug) //test whether the transform makes senses.
  //  {
    std::cout << "############# Transform armature...";
    vtkSmartPointer<vtkPolyData> posedArmature = TransformArmature(armature,"Transforms",!IsArmatureInRAS);
    bender::IOUtils::WriteDebugPolyData(posedArmature,
      "PoseLabelmap_PosedArmature.vtk", WeightDirectory + "/Debug");
    std::cout << "############# done." << std::endl;
  //  }

  vtkCellArray* armatureSegments = armature->GetLines();
  vtkCellData* armatureCellData = armature->GetCellData();
  vtkNew<vtkIdList> cell;
  armatureSegments->InitTraversal();
  int edgeId(0);
  if (!armatureCellData->GetArray("Transforms"))
    {
    std::cerr << "No 'Transforms' cell array in armature" << std::endl;
    }
  else
    {
    std::cout << "# components: " << armatureCellData->GetArray("Transforms")->GetNumberOfComponents()
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
    GetArmatureTransform(armature, edgeId, "Transforms", ax, transform, !IsArmatureInRAS);
    transforms.push_back(transform);
    if (Debug)
      {
      std::cout << "Transform: o=" << transform.O
                << " t= " << transform.T
                << " r= " << transform.R
                << std::endl;
      }
    ++edgeId;
    }

  numSites = transforms.size();

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

  std::cout <<"Read "<<numSites<<" transforms"<< std::endl;


  //----------------------------
  // Output labelmap
  //----------------------------
  typename LabelImageType::Pointer posedLabelMap = LabelImageType::New();
  posedLabelMap->CopyInformation(labelMap);
  double padding = 10.;
  vtkDoubleArray* envelopes = vtkDoubleArray::SafeDownCast(
    posedArmature->GetCellData()->GetScalars("EnvelopeRadiuses"));
  if (envelopes)
    {
    double maxRadius = 0.;
    for (vtkIdType i = 0; i < envelopes->GetNumberOfTuples(); ++i)
      {
      maxRadius = std::max(maxRadius, envelopes->GetValue(i));
      }
    padding = maxRadius;
    }
  assert(padding >= 0.);
  std::cout << "Padding: " << padding << std::endl;
  double posedArmatureBounds[6] = {0., -1., 0., -1., 0., -1.};
  posedArmature->GetBounds(posedArmatureBounds);
  if (!IsArmatureInRAS)
    {
    posedArmatureBounds[0] *= -1;
    posedArmatureBounds[1] *= -1;
    posedArmatureBounds[2] *= -1;
    posedArmatureBounds[3] *= -1;
    std::swap(posedArmatureBounds[0], posedArmatureBounds[1]);
    std::swap(posedArmatureBounds[2], posedArmatureBounds[3]);
    }
  std::cout << "Armature bounds: "
            << posedArmatureBounds[0] << "," << posedArmatureBounds[1] << ","
            << posedArmatureBounds[2] << "," << posedArmatureBounds[3] << ","
            << posedArmatureBounds[4] << "," << posedArmatureBounds[5] << std::endl;
  double posedLabelmapBounds[6] = {0., -1., 0., -1., 0., -1.};
  double bounds[6] = {0., -1., 0., -1., 0., -1.};
  for (int i = 0; i < 3; ++i)
    {
    posedLabelmapBounds[i*2] = posedArmatureBounds[i*2] - padding;
    posedLabelmapBounds[i*2 + 1] = posedArmatureBounds[i*2 + 1] + padding;
    bounds[i*2] = posedLabelmapBounds[i*2];
    bounds[i*2 + 1] = posedLabelmapBounds[i*2 + 1];
    }
  typename LabelImageType::PointType origin;
  origin[0] = posedLabelMap->GetDirection()[0][0] >= 0. ? bounds[0] : bounds[1];
  origin[1] = posedLabelMap->GetDirection()[1][1] >= 0. ? bounds[2] : bounds[3];
  origin[2] = posedLabelMap->GetDirection()[2][2] >= 0. ? bounds[4] : bounds[5];
  //origin[0] = std::min(origin[0], labelMap->GetOrigin()[0]);
  //origin[1] = std::min(origin[1], labelMap->GetOrigin()[1]);
  //origin[2] = std::min(origin[2], labelMap->GetOrigin()[2]);
  posedLabelMap->SetOrigin(origin);
  typename LabelImageType::RegionType region;
  assert(bounds[1] >= bounds[0] && bounds[3] >= bounds[2] && bounds[5] >= bounds[4]);
  region.SetSize(0, (bounds[1]-bounds[0]) / posedLabelMap->GetSpacing()[0]);
  region.SetSize(1, (bounds[3]-bounds[2]) / posedLabelMap->GetSpacing()[1]);
  region.SetSize(2, (bounds[5]-bounds[4]) / posedLabelMap->GetSpacing()[2]);
  //region.SetSize(0, std::max(region.GetSize()[0], labelMap->GetLargestPossibleRegion().GetSize()[0]));
  //region.SetSize(1, std::max(region.GetSize()[1], labelMap->GetLargestPossibleRegion().GetSize()[1]));
  //region.SetSize(2, std::max(region.GetSize()[2], labelMap->GetLargestPossibleRegion().GetSize()[2]));
  posedLabelMap->SetRegions(region);
  std::cout << "Allocate output posed labelmap: \n"
            << " Origin: " << posedLabelMap->GetOrigin() << "\n"
            << " Spacing: " << posedLabelMap->GetSpacing() << "\n"
            << " Direction: " << posedLabelMap->GetDirection()
            << " " << posedLabelMap->GetLargestPossibleRegion()
            << std::endl;
  posedLabelMap->Allocate();
  posedLabelMap->FillBuffer(OutsideLabel);
/*
  std::cout << "############# Posed Indexes..." << std::endl;
  typedef typename LabelImageType::IndexType IndexType;
  typedef itk::Image<IndexType, 3> IndexImageType;
  typename IndexImageType::Pointer posedIndexes = IndexImageType::New();
  posedIndexes->CopyInformation(posedLabelMap);
  //posedIndexes->SetSpacing(posedLabelMap->GetSpacing());
  //posedIndexes->SetDirection(posedLabelMap->GetDirection());
  //posedIndexes->SetOrigin(origin);
  posedIndexes->SetRegions(region);
  posedIndexes->Allocate();
  IndexType invalidIndex = {{-1, -1, -1}};
  posedIndexes->FillBuffer(invalidIndex);

  
  std::cout << "############# Posed IDs..." << std::endl;
  typedef unsigned int IDType;
  typedef itk::Image<IDType, 3> IDImageType;
  typename IDImageType::Pointer posedIDs = IDImageType::New();
  posedIDs->CopyInformation(posedLabelMap);
  posedIDs->SetRegions(region);
  posedIDs->Allocate();
  IDType invalidID = 0;
  posedIDs->FillBuffer(invalidID);
  */

  //----------------------------
  // Perform interpolation
  //----------------------------

  std::cout << "############# First pass..." << std::endl;
  itk::ImageRegionConstIteratorWithIndex<LabelImageType> imageIt(labelMap, labelMap->GetLargestPossibleRegion());
/*
  // First pass, fill as much as possible
  //IDType index(0);
  size_t assignedPixelCount(0);
  for (; !imageIt.IsAtEnd() ; ++imageIt)
    {
    if (imageIt.Get() == OutsideLabel)
      {
      continue;
      }
    typename itk::ContinuousIndex<double, 3> index = imageIt.GetIndex();
    itk::Vector<double,3> posedCoord =
      Transform<T>(labelMap, index, numSites, weightMap, weight0, LinearBlend, transforms, dqs);
    if (posedCoord == InvalidCoord)
      {
      continue;
      }
    //std::cout << posedCoord << std::endl;
    typename LabelImageType::PointType posedPoint;
    posedPoint[0] = posedCoord[0];
    posedPoint[1] = posedCoord[1];
    posedPoint[2] = posedCoord[2];
    typename LabelImageType::IndexType posedIndex;
    bool res = posedLabelMap->TransformPhysicalPointToIndex(posedPoint, posedIndex);
    if (!res)
      {
      //assert(res);
      std::cerr << "!";
      }
    else
      {
      ++assignedPixelCount;
      posedLabelMap->SetPixel(posedIndex, imageIt.Get());
      //posedIndexes->SetPixel(posedIndex, imageIt.GetIndex());
      //posedIDs->SetPixel(posedIndex, index);
      //typename IDImageType::IndexType idIndex;
      //posedIDs->TransformPhysicalPointToIndex(posedPoint, idIndex);
      //posedIDs->SetPixel(idIndex, index);
      //std::cout << "." << std::endl;
      //typename IndexImageType::IndexType pPosedIndex;
      //res = posedIndexes->TransformPhysicalPointToIndex(posedPoint, pPosedIndex);
      //if (res)
        {
        //posedIndexes->SetPixel(pPosedIndex, imageIt.GetIndex());
        }
      //std::cout << "-" << std::endl;
      }
    }
  std::cout << "############# done." << std::endl;
  if (Debug)
    {
    bender::IOUtils::WriteImage<LabelImageType>(
      posedLabelMap, "firstPass.mha");
    std::cout << "############# done." << std::endl;
    }
*/
/*
  double stepWidth= 1.;
  size_t maxStepCount = 1;
  size_t assignedPixelCount(1);
  size_t maxPosedOffsetNorm = 2;
  for (size_t pass = 0;
       (pass <= MaximumPass) &&
        (pass <= 1 || (assignedPixelCount && maxPosedOffsetNorm > 0));
       ++pass, stepWidth /=2)
    {
    std::cout << "############# " << pass << " pass..." << std::endl;
    std::cout << "Step width: " << stepWidth << std::endl;
    maxStepCount *= 2;
    if (stepWidth >= 0.25)
      {
      maxStepCount = 1;
      }
    assignedPixelCount = 0;
    maxPosedOffsetNorm = 0;
    for (size_t stepIndex = 0; stepIndex < maxStepCount; ++stepIndex )
      {
      double step = stepWidth + (stepIndex*stepWidth*2);
      std::cout << "################### " << stepIndex+1 << "/" << maxStepCount << std::endl;
      std::cout << "Step: " << step << std::endl;
*/
  size_t assignedPixelCount(1);
  size_t countSkippedVoxels(0);
  size_t voxelIt(0);
  const size_t voxelCount =
    labelMap->GetLargestPossibleRegion().GetSize(0) *
    labelMap->GetLargestPossibleRegion().GetSize(1) *
    labelMap->GetLargestPossibleRegion().GetSize(2);
  size_t progress((voxelCount-1) / 100);
      // First pass, fill as much as possible
      for (imageIt.GoToBegin(); !imageIt.IsAtEnd() ; ++imageIt)
        {
        if (voxelIt++ % progress == 0)
          {
          std::cout << "+";
          std::cout.flush();
          }
        if (imageIt.Get() == OutsideLabel)
          {
          continue;
          }
        itk::Vector<double,3> posedCoord =
          Transform<T>(labelMap, imageIt.GetIndex(), numSites, weightMap, LinearBlend, MaximumNumberOfInterpolatedBones, dqs);
        if (posedCoord == InvalidCoord)
          {
          continue;
          }
        //std::cout << posedCoord << std::endl;
        typename LabelImageType::PointType posedPoint;
        posedPoint[0] = posedCoord[0];
        posedPoint[1] = posedCoord[1];
        posedPoint[2] = posedCoord[2];
        typename LabelImageType::IndexType posedIndex;
        bool res = posedLabelMap->TransformPhysicalPointToIndex(posedPoint, posedIndex);
        if (!res)
          {
          //assert(res);
          std::cerr << "!";
          }
        else // need to overwrite ?
          {
          ++assignedPixelCount;
          posedLabelMap->SetPixel(posedIndex, imageIt.Get());

          size_t maxPosedOffsetNorm = 2; // do it the first time.
          for (size_t radius = 1;
            maxPosedOffsetNorm > 1 && radius <= static_cast<unsigned int>(MaximumRadius);
            radius*=2)
            {
            if (radius >= 16)
              {
              std::cerr << "@" << radius ;
              }
            //size_t assignedNeighborsCount = 0;
            //itk::Neighborhood<T, 3> neighborhood;
            //neighborhood.SetRadius(radius);
            SubNeighborhood<3> neighborhood(radius);
            double step = 0.5 / radius;
            //{
            maxPosedOffsetNorm = 0;
            size_t stepAssignedPixelCount = 0;
            for (size_t iOff =0; iOff < neighborhood.Size; ++iOff)
              {
              typename itk::ContinuousIndex<double, 3> index(imageIt.GetIndex());
              index[0] += step * neighborhood.Offsets[iOff][0];
              index[1] += step * neighborhood.Offsets[iOff][1];
              index[2] += step * neighborhood.Offsets[iOff][2];
              itk::Vector<double,3> neighborPosedCoord =
                Transform<T>(labelMap, index, numSites, weightMap, LinearBlend, MaximumNumberOfInterpolatedBones, dqs);
              if (neighborPosedCoord == InvalidCoord)
                {
                continue;
                }
              typename LabelImageType::PointType neighborPosedPoint;
              neighborPosedPoint[0] = neighborPosedCoord[0];
              neighborPosedPoint[1] = neighborPosedCoord[1];
              neighborPosedPoint[2] = neighborPosedCoord[2];
              typename LabelImageType::IndexType neighborPosedIndex;
              bool neighborRes = posedLabelMap->TransformPhysicalPointToIndex(neighborPosedPoint, neighborPosedIndex);
              if (neighborRes)
                {
                size_t posedOffsetNorm = 0;
                posedOffsetNorm = std::max(posedOffsetNorm,
                                           static_cast<size_t>(std::abs(neighborPosedIndex[0] - posedIndex[0])/radius));
                posedOffsetNorm = std::max(posedOffsetNorm,
                                           static_cast<size_t>(std::abs(neighborPosedIndex[1] - posedIndex[1])/radius));
                posedOffsetNorm = std::max(posedOffsetNorm,
                                           static_cast<size_t>(std::abs(neighborPosedIndex[2] - posedIndex[2])/radius));
                maxPosedOffsetNorm = std::max(maxPosedOffsetNorm, posedOffsetNorm);
                if (posedLabelMap->GetPixel(neighborPosedIndex) ==OutsideLabel)
                  {
                  posedLabelMap->SetPixel(neighborPosedIndex, imageIt.Get());
                  ++stepAssignedPixelCount;
                  }
                }
              }
            // Test to see if the neighbors have been filled.
            //assignedNeighborsCount = 0;
            //for (size_t iOff =0; iOff < neighborhood.Size(); ++iOff)
            //  {
            //  assignedNeighborsCount +=
            //    (posedLabelMap->GetPixel(posedIndex + neighborhood.GetOffset(iOff)) != OutsideLabel) ? 1 :0;
            //  }
            assignedPixelCount += stepAssignedPixelCount;
            }
          if (maxPosedOffsetNorm > 1)
            {
            ++countSkippedVoxels;
            }
          }
        }
        //}
      std::cout << assignedPixelCount << " pixels assigned" << std::endl;
      std::cout << countSkippedVoxels << " voxels skipped" << std::endl;
    //std::cout << maxPosedOffsetNorm << " maximum posed offset norm" << std::endl;
    /*
    if (Debug)
      {
      std::stringstream ss;
      ss << "Pass-" << pass << ".mha";
      bender::IOUtils::WriteImage<LabelImageType>(
        posedLabelMap, ss.str().c_str());
      std::cout << "############# done." << std::endl;
      }
    */
    //}

  std::cout << "############# done." << std::endl;

  //----------------------------
  // Write output
  //----------------------------
  bender::IOUtils::WriteImage<LabelImageType>(
    posedLabelMap, PosedLabelmap.c_str());

  return EXIT_SUCCESS;
}
