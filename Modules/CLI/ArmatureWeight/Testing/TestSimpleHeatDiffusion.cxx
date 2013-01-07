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

  This file was originally developed by Yuanxin Liu, Kitware Inc.
  and was partially funded by NIH grant 3P41RR013218-12S1

 ==============================================================================*/

// Armatures includes
#include "HeatDiffusionProblem.h"
#include "SolveHeatDiffusionProblem.h"
#include <iostream>

typedef itk::Image<float, 2>  Image;
typedef itk::Index<2> Pixel;
typedef itk::ImageRegion<2> Region;

class SimpleHeatDiffusionProblem: public HeatDiffusionProblem<2>
{
public:
  SimpleHeatDiffusionProblem(Image::Pointer image): DataImage(image)
  {
    Region region = image->GetLargestPossibleRegion();
    this->End[0] = region.GetSize()[0];
    this->End[1] = region.GetSize()[1];
  }

  virtual bool IsBoundary(const Pixel& ij) const
  {
    bool inBound =ij[0]>0 && ij[0]<this->End[0]-1 && ij[1]>0 && ij[1]<this->End[1]-1;
    return !inBound;
  }

  //Is the pixel inside the problem domain?
  virtual bool InDomain(const Pixel& p) const
  {
    return this->DataImage->GetLargestPossibleRegion().IsInside(p);
  }

  virtual float GetBoundaryValue(const Pixel& p) const
  {
    return this->DataImage->GetPixel(p);
  }

private:
  Image::Pointer DataImage;
  Pixel End;
};

using namespace std;

Image::Pointer CreateTestImage(int n0, int n1)
{
  Image::SizeType size;
  size[0] = n0;
  size[1] = n1;

  Pixel end;
  end[0] = static_cast<int>(size[0]);
  end[1] = static_cast<int>(size[1]);

  //allocate the data image
  Image::Pointer image = Image::New();
  Image::RegionType imageRegion;
  Image::IndexType start={{0,0}};
  imageRegion.SetSize(size);
  imageRegion.SetIndex(start);
  image->SetRegions(imageRegion);
  image->Allocate();

  //create the data by sampling the linear function
  Image::IndexType ij;
  for(ij[0]=0; ij[0]<end[0]; ij[0]++)
    {
    for(ij[1]=0; ij[1]<end[1]; ij[1]++)
      {
      image->SetPixel(ij,    (double(ij[0]))/end[0] + (double(ij[1]))/end[1]);
      }
    }

  return image;
}


int TestSolve()
{
  int imageSize(64);

  Image::Pointer in = CreateTestImage(imageSize,imageSize);
  SimpleHeatDiffusionProblem problem(in);

  Image::Pointer out = Image::New();
  out->SetRegions(in->GetLargestPossibleRegion());
  out->Allocate();

  SolveHeatDiffusionProblem<Image>::Solve(problem,out);

  itk::ImageRegionIteratorWithIndex<Image> it(in,in->GetLargestPossibleRegion());
  for(it.GoToBegin(); !it.IsAtEnd(); ++it)
    {
    float trueValue = it.Get();
    float computedValue =out->GetPixel(it.GetIndex());
    if ( fabs(computedValue-trueValue)>0.0001)
      {
      cout<<"Computed value is "<<computedValue<<" but expect "<<trueValue<<endl;
      return 1;
      }
    }

  return 0;
}

int TestSolveIteratively()
{
  int imageSize(5);

  Image::Pointer in = CreateTestImage(imageSize,imageSize);
  SimpleHeatDiffusionProblem problem(in);

  Image::Pointer out = CreateTestImage(imageSize,imageSize);

  //Destroy the interior pixels so we know we are doing something
  for(int i=1; i<imageSize-1; i++)
    {
    for(int j=1; j<imageSize-1; j++)
      {
      Pixel ij = {{i,j}};
      out->SetPixel(ij,-1);
      }
    }

  SolveHeatDiffusionProblem<Image>::SolveIteratively(problem,out,1000);

  itk::ImageRegionIteratorWithIndex<Image> it(in,in->GetLargestPossibleRegion());
  for(it.GoToBegin(); !it.IsAtEnd(); ++it)
    {
    float trueValue = it.Get();
    float computedValue =out->GetPixel(it.GetIndex());
    if ( fabs(computedValue-trueValue)>0.0001)
      {
      cout<<"Computed value is "<<computedValue<<" but expect "<<trueValue<<endl;
      return 1;
      }
    }

  return 0;
}

int main(int, char* [])
{
  int errors(0);

  errors+=TestSolve();
  errors+=TestSolveIteratively();

  return errors;
}
