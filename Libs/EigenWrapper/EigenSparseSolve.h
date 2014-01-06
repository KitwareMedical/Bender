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

#ifndef __EigenWrapper_h
#define __EigenWrapper_h

// Eigen includes
#include <Eigen/Dense>
#include <Eigen/Sparse>

// Bender includes
#include "BenderEigenWrapperExport.h"

typedef Eigen::SparseMatrix<float> SpMat;

//Solve a sparse linear system.  Just wrap around Eignen
Eigen::VectorXf BENDER_EIGENWRAPPER_EXPORT Solve(SpMat& A,  Eigen::VectorXf& b);

#endif
