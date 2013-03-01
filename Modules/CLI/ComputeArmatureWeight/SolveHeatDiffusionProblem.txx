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

#ifndef __SolveHeatDiffusionProblem_txx
#define __SolveHeatDiffusionProblem_txx

template<class Image>
void SolveHeatDiffusionProblem<Image>::Solve(const HeatDiffusionProblem<Image::ImageDimension>& problem,  typename Image::Pointer heat)  ////output
{
  Neighborhood neighbors;
  PixelOffset* offsets = neighbors.Offsets ;

  int m(0),n(0);
  itk::ImageRegionIteratorWithIndex<Image> it(heat,heat->GetLargestPossibleRegion());
  for(it.GoToBegin(); !it.IsAtEnd(); ++it)
    {
    if(problem.InDomain(it.GetIndex()))
      {
      ++n;
      if(!problem.IsBoundary(it.GetIndex()))
        {
        ++m;
        }
      }
    }
  std::cout << "Problem dimension: "<<m<<" x "<<n << std::endl;

  typedef itk::Image<int, Image::ImageDimension> ImageIndexMap;
  typename ImageIndexMap::Pointer matrixIndex = ImageIndexMap::New();

  matrixIndex->SetOrigin(heat->GetOrigin());
  matrixIndex->SetSpacing(heat->GetSpacing());
  matrixIndex->SetRegions(heat->GetLargestPossibleRegion());
  matrixIndex->Allocate();

  std::vector<Pixel> imageIndex(n);
  int i1(0),i2(m); //index is the image pixel index, i1 and i2 are matrix indices
  for(it.GoToBegin(); !it.IsAtEnd(); ++it)
    {
    Pixel pixel = it.GetIndex();
    if(problem.InDomain(pixel)) //in domain
      {
      if(!problem.IsBoundary(it.GetIndex()))
        {
        imageIndex[i1] = pixel;
        matrixIndex->SetPixel(pixel, i1);
        ++i1;
        }
      else
        {
        assert(i2<n);
        imageIndex[i2] = pixel;
        matrixIndex->SetPixel(pixel,i2);
        ++i2;
        }
      }
    else
      {
      matrixIndex->SetPixel(pixel,-1);//set to invalid value;
      }
    }
  assert(i1==m);
  assert(i2==n);

  //Contruct the matrices
  typedef Eigen::SparseMatrix<float> SpMat;
  SpMat A(m,m);
  SpMat B(m,n-m);
  typedef Eigen::Triplet<float> SpMatEntry;
  std::vector<SpMatEntry> entryA, entryB;
  for(int i=0; i<m; ++i)
    {
    float Aii(0);
    for(unsigned int ii =0; ii<2*Image::ImageDimension; ++ii)
      {
      Pixel pixel = imageIndex[i]+offsets[ii];
      if(problem.InDomain(pixel))
        {
        int j = matrixIndex->GetPixel(pixel);
        if (j<m)
          {
          entryA.push_back( SpMatEntry(i,j,-1));
          }
        else
          {
          entryB.push_back( SpMatEntry(i,j-m,-1));
          }
        Aii+=1.0;
        }
      }
    entryA.push_back(SpMatEntry(i,i,Aii));
    }
  A.setFromTriplets(entryA.begin(),entryA.end());
  B.setFromTriplets(entryB.begin(),entryB.end());


  //contruct the vectors
  Eigen::VectorXf xB(n-m);
  int numOnes=0;
  for(int i=0,i0=m; i<n-m; ++i,++i0)
    {
    Pixel pixel = imageIndex[i0];
    xB[i] = problem.GetBoundaryValue(pixel);
    numOnes += xB[i]==1.0;
    }

  Eigen::VectorXf b(m);
  b = B*xB;
  b*=-1.0;

  // Solving:
  Eigen::VectorXf xI(m);
  if(0)
    {
    }
  else
    {
    try
      {
      xI = ::Solve(A,b);
      }
    catch(std::bad_alloc)
      {
      std::cout << "Cholesky runs out of memory, switch to conjugate gradient instead\n";
      Eigen::ConjugateGradient<SpMat> solver(A);
      xI= solver.solve(b);
      }
    }

  //set the interior pixels by xI
  for(int i=0; i<m; ++i)
    {
    Pixel pixel = imageIndex[i];
    heat->SetPixel(pixel, xI[i]);
    }
  //set the boundary pixels by xB
  for(int i=m; i<n; ++i)
    {
    heat->SetPixel(imageIndex[i],xB[i-m]);
    }
}

template<class Image>
void SolveHeatDiffusionProblem<Image>::SolveIteratively(const HeatDiffusionProblem<Image::ImageDimension>& problem,  typename Image::Pointer heat, int numIterations)
{
  typename Image::RegionType region = heat->GetLargestPossibleRegion();
  Neighborhood nbrs;
  PixelOffset* offsets = nbrs.Offsets;

  std::vector<Pixel> interior;
  for(itk::ImageRegionIteratorWithIndex<Image> it(heat,region); !it.IsAtEnd(); ++it)
    {
    Pixel p = it.GetIndex();
    if(problem.InDomain(p) && !problem.IsBoundary(p))
      {
      interior.push_back(p);
      }
    }

  std::cout<<"Interior size: "<<interior.size()<<std::endl;
  std::cout << "Smooth iteratively: ";
  for(int iteration=0; iteration<numIterations; ++iteration)
    {
    std::cout << ". " << std::flush;
    for(typename std::vector<Pixel>::iterator pi = interior.begin(); pi!=interior.end(); ++pi)
      {
      Pixel p = *pi;
      Real sum=0.0;
      Real diag = 0.0;
      for(PixelOffset* offset = offsets; offset!=offsets+2*Image::ImageDimension; ++offset)
        {
        Pixel q = p + *offset;
        if(problem.InDomain(q))
          {
          sum+=heat->GetPixel(q);
          diag+=1.0;
          }
        }
      if(diag==0.0)
        {
        std::cerr<<p<<" has no neighbor" << std::endl;
        }
      else
        {
        heat->SetPixel(p, sum/diag);
        }
      }
    }
  std::cout << std::endl;
}
#endif
